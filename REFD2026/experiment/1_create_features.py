import argparse
import csv
import json
import logging
import math
import multiprocessing as mp
import os
import shutil
import time
from collections import defaultdict
from multiprocessing import Pool, cpu_count
from typing import Any, Dict, Iterator, List, Optional, Tuple

import feature_extraction as feature_module


WINDOW_MS = 1000
LOGGER = logging.getLogger("create_features")
_EXTRACTOR = None
_EVENT_CLS = None
_FEATURE_NAMES_HINT: Optional[List[str]] = None
PROGRESS_EVERY = 200
FLUSH_EVERY_ROWS = 50000


def _safe_float(value: Any) -> float:
    if value is None:
        return 0.0
    try:
        return float(value)
    except (TypeError, ValueError):
        return 0.0


def _normalize_extract_result(result: Any) -> Dict[str, float]:
    if isinstance(result, dict):
        names = [str(k) for k in result.keys()]
        values = [_safe_float(v) for v in result.values()]
        return dict(zip(names, values))

    if isinstance(result, tuple) and len(result) == 2:
        names_raw, values_raw = result
        names = [str(k) for k in list(names_raw)]
        values = [_safe_float(v) for v in list(values_raw)]
        if len(names) != len(values):
            raise ValueError("feature_extraction returned mismatched name/value lengths")
        return dict(zip(names, values))

    if hasattr(result, "tolist"):
        values_list = result.tolist()  # type: ignore
        if isinstance(values_list, (int, float)):
            values = [_safe_float(values_list)]
        else:
            values = [_safe_float(v) for v in list(values_list)]

        if not _FEATURE_NAMES_HINT:
            raise ValueError(
                "feature names are unavailable for vector output from feature_extraction"
            )
        if len(_FEATURE_NAMES_HINT) != len(values):
            raise ValueError(
                "feature_extraction vector length does not match feature_names()"
            )
        return dict(zip(_FEATURE_NAMES_HINT, values))

    raise TypeError(f"Unsupported feature_extraction output type: {type(result)!r}")


def _extract_window_features(window_events: List[Dict[str, Any]]) -> Dict[str, float]:
    payload = [ev["event_obj"] for ev in window_events]
    result = _EXTRACTOR.extract(payload)  # type: ignore
    return _normalize_extract_result(result)


def split_events_by_time_windows(
    events: List[Dict[str, Any]],
    window_ms: int = WINDOW_MS,
    t_start: int = 0,
) -> List[List[Dict[str, Any]]]:
    """Legacy fixed-window behavior."""
    if not events:
        return []

    if t_start == 0:
        t_start = int(events[0]["ts"])

    t_end = int(events[-1]["ts"])
    num_windows = int(math.ceil((t_end - t_start) / window_ms))
    windows = [[] for _ in range(num_windows)]

    for ev in events:
        idx = int((int(ev["ts"]) - t_start) // window_ms)
        if 0 <= idx < num_windows:
            windows[idx].append(ev)

    return windows


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def clear_split_output(feature_base: str, split: str) -> None:
    split_dir = os.path.join(feature_base, split)
    if os.path.isdir(split_dir):
        shutil.rmtree(split_dir)
        LOGGER.info("[clean] Removed old output: %s", split_dir)


def _safe_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def get_output_csv(record: Dict[str, Any], split: str, feature_base: str) -> str:
    label = _safe_int(record.get("label"), 0)

    if label == 0:
        exe = os.path.basename(str(record.get("pid_path", ""))).lower()
        name = exe.rsplit(".", 1)[0] if "." in exe else exe
        if not name:
            fallback = str(record.get("name", "benign_unknown")).strip()
            name = fallback or "benign_unknown"
        out_dir = os.path.join(feature_base, split, "benign")
    else:
        name = str(record.get("name", "ransom_unknown")).strip() or "ransom_unknown"
        out_dir = os.path.join(feature_base, split, "ransom")

    ensure_dir(out_dir)
    return os.path.join(out_dir, f"{name}.csv")


def init_worker(window_ms: int = WINDOW_MS) -> None:
    global _EXTRACTOR, _EVENT_CLS, _FEATURE_NAMES_HINT
    window_seconds = float(window_ms) / 1000.0

    _EVENT_CLS = feature_module.Event
    _EXTRACTOR = feature_module.FileBehaviorFeatureExtractor(window_seconds=window_seconds)
    _FEATURE_NAMES_HINT = list(_EXTRACTOR.feature_names())


def _normalize_events(events: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    normalized: List[Dict[str, Any]] = []

    for ev in events:
        if not isinstance(ev, dict):
            continue
        if "ts" not in ev:
            continue

        try:
            ts = int(ev["ts"])
        except (TypeError, ValueError):
            continue

        op = str(ev.get("op", "")).lower()
        path = str(ev.get("file", ""))
        new_path = ""
        if op == "rename":
            new_path = ""

        event_obj = _EVENT_CLS(  # type: ignore
            operation=op,
            path=path,
            time=float(ts) / 1000.0,
            new_path=new_path,
        )

        fixed = {
            "ts": ts,
            "op": op,
            "file": path,
            "new_path": new_path,
            "event_obj": event_obj,
        }

        normalized.append(fixed)

    normalized.sort(key=lambda x: x["ts"])
    return normalized


def process_record_worker(args: Tuple[str, int, str]) -> List[Tuple[Dict[str, Any], Dict[str, Any], str]]:
    raw_line, fallback_label, split = args

    try:
        record = json.loads(raw_line)
    except json.JSONDecodeError:
        LOGGER.warning("Skipping malformed JSONL line")
        return []

    if not isinstance(record, dict):
        return []

    label = _safe_int(record.get("label"), _safe_int(fallback_label, 0))
    label = 1 if label == 1 else 0
    record["label"] = label

    events = record.get("events", [])
    if not isinstance(events, list) or not events:
        return []

    normalized_events = _normalize_events(events)
    if not normalized_events:
        return []

    t_start = int(normalized_events[0]["ts"])
    tss = set()
    for ev in normalized_events:
        r = int(ev["ts"]) % WINDOW_MS
        k = (t_start - r) // WINDOW_MS
        n = k * WINDOW_MS + r
        if n <= t_start:
            tss.add(n)

    sorted_tss = sorted(tss)
    sorted_tss = sorted_tss[:1] if label == 0 else sorted_tss[:1]

    rows: List[Tuple[Dict[str, Any], Dict[str, Any], str]] = []
    for ts in sorted_tss:
        windows = split_events_by_time_windows(normalized_events, WINDOW_MS, ts)
        for idx, win_events in enumerate(windows):
            if not win_events:
                continue

            try:
                features = _extract_window_features(win_events)
            except Exception as exc:
                LOGGER.exception("Feature extraction failed: %s", exc)
                continue

            row = {
                "name": str(record.get("name", "")),
                "pid": _safe_int(record.get("pid"), 0),
                "pid_path": str(record.get("pid_path", "")),
                "window_start": int(ts + idx * WINDOW_MS),
                "window_end": int(ts + (idx + 1) * WINDOW_MS),
                "label": label,
            }
            row.update(features)
            rows.append((record, row, split))

    return rows


def write_rows_grouped(
    results: List[Tuple[Dict[str, Any], Dict[str, Any], str]],
    feature_base: str,
) -> None:
    cache: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
    for record, row, split in results:
        out_csv = get_output_csv(record, split, feature_base)
        cache[out_csv].append(row)

    for out_csv, rows in sorted(cache.items(), key=lambda x: x[0]):
        ensure_dir(os.path.dirname(out_csv))
        # rows.sort(
        #     key=lambda r: (
        #         str(r.get("name", "")),
        #         _safe_int(r.get("pid"), 0),
        #         _safe_int(r.get("window_start"), 0),
        #         _safe_int(r.get("window_end"), 0),
        #     )
        # )
        write_header = not os.path.exists(out_csv)
        meta_cols = ["name", "pid", "pid_path", "window_start", "window_end"]
        ignore = set(meta_cols + ["label"])
        feature_cols: List[str] = []
        seen = set()
        for row in rows:
            for k in row.keys():
                if k in ignore or k in seen:
                    continue
                seen.add(k)
                feature_cols.append(k)
        fieldnames = meta_cols + feature_cols + ["label"]
        with open(out_csv, "a", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            if write_header:
                writer.writeheader()
            writer.writerows(rows)


def iter_raw_lines(path: str) -> Iterator[str]:
    if os.path.isfile(path):
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                if line.strip():
                    yield line
        return

    if os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            dirs.sort()
            files.sort()
            for fn in files:
                if not fn.lower().endswith(".jsonl"):
                    continue
                fpath = os.path.join(root, fn)
                with open(fpath, "r", encoding="utf-8") as f:
                    for line in f:
                        if line.strip():
                            yield line
        return

    LOGGER.warning("Path not found, skipping: %s", path)


def iter_tasks(
    paths_with_labels: List[Tuple[str, int]],
    split: str,
) -> Iterator[Tuple[str, int, str]]:
    for path, label in paths_with_labels:
        for raw_line in iter_raw_lines(path):
            yield (raw_line, int(label), split)


def build_dataset_from_paths(
    paths_with_labels: List[Tuple[str, int]],
    split: str,
    feature_base: str,
    n_workers: Optional[int] = None,
    progress_every: int = PROGRESS_EVERY,
    flush_every_rows: int = FLUSH_EVERY_ROWS,
) -> None:
    if n_workers is None:
        n_workers = max(cpu_count() - 1, 1)

    tasks = iter_tasks(paths_with_labels, split)
    pending_results: List[Tuple[Dict[str, Any], Dict[str, Any], str]] = []
    processed_records = 0
    total_rows = 0
    flushed_rows = 0
    started_at = time.time()
    with Pool(processes=n_workers, initializer=init_worker, initargs=(WINDOW_MS,)) as pool:
        for out in pool.imap_unordered(process_record_worker, tasks, chunksize=128):
            processed_records += 1
            if out:
                pending_results.extend(out)
                total_rows += len(out)

                if flush_every_rows > 0 and len(pending_results) >= flush_every_rows:
                    write_rows_grouped(pending_results, feature_base)
                    flushed_rows += len(pending_results)
                    pending_results.clear()

            if progress_every > 0 and processed_records % progress_every == 0:
                elapsed = max(time.time() - started_at, 1e-9)
                LOGGER.info(
                    "[progress][%s] records=%d rows=%d flushed=%d pending=%d elapsed=%.1fs rate=%.2f rec/s",
                    split,
                    processed_records,
                    total_rows,
                    flushed_rows,
                    len(pending_results),
                    elapsed,
                    processed_records / elapsed,
                )

    if pending_results:
        write_rows_grouped(pending_results, feature_base)
        flushed_rows += len(pending_results)
        pending_results.clear()

    elapsed = max(time.time() - started_at, 1e-9)
    LOGGER.info(
        "[done][%s] records=%d rows=%d flushed=%d elapsed=%.1fs rate=%.2f rec/s",
        split,
        processed_records,
        total_rows,
        flushed_rows,
        elapsed,
        processed_records / elapsed,
    )


def parse_args() -> argparse.Namespace:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description="Generate behavioral features from JSONL")
    parser.add_argument("--base", default=os.path.join(script_dir, "dataset"))
    parser.add_argument("--feature-base", default=os.path.join(script_dir, "features"))
    parser.add_argument("--workers", type=int, default=min(cpu_count() // 2, 8))
    parser.add_argument("--progress-every", type=int, default=PROGRESS_EVERY)
    parser.add_argument("--flush-every-rows", type=int, default=FLUSH_EVERY_ROWS)
    parser.add_argument("--split", choices=["all", "train", "test"], default="all")
    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
    )
    return parser.parse_args()


def main() -> None:
    mp.freeze_support()
    args = parse_args()
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s | %(levelname)s | %(message)s",
    )

    base = args.base
    n_workers = max(int(args.workers), 1)

    if args.split in ("all", "train"):
        clear_split_output(args.feature_base, "train")
        build_dataset_from_paths(
            [
                (os.path.join(base, "train", "benign.jsonl"), 0),
                (os.path.join(base, "train", "ransom"), 1),
            ],
            split="train",
            feature_base=args.feature_base,
            n_workers=n_workers,
            progress_every=args.progress_every,
            flush_every_rows=args.flush_every_rows,
        )
        print("[+] Train features generated")

    if args.split in ("all", "test"):
        clear_split_output(args.feature_base, "test")
        build_dataset_from_paths(
            [
                (os.path.join(base, "test", "benign.jsonl"), 0),
                (os.path.join(base, "test", "ransom"), 1),
            ],
            split="test",
            feature_base=args.feature_base,
            n_workers=n_workers,
            progress_every=args.progress_every,
            flush_every_rows=args.flush_every_rows,
        )
        print("[+] Test features generated")


if __name__ == "__main__":
    main()
