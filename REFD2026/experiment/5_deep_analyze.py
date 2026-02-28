# type: ignore
import argparse
import glob
import json
import os
from typing import Dict, List, Optional, Tuple

import numpy as np
import pandas as pd
import xgboost as xgb

from feature_vector_utils import build_feature_frame, load_feature_list


def parse_args() -> argparse.Namespace:
    script_dir = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(
        description="Deep analyze window-level false positives with XAI feature contributions"
    )
    parser.add_argument(
        "--feature-base",
        default=os.path.join(script_dir, "features"),
        help="Feature base directory",
    )
    parser.add_argument(
        "--model-path",
        default=os.path.join(script_dir, "model", "model.json"),
        help="Path to XGBoost model",
    )
    parser.add_argument(
        "--feature-list-path",
        default=os.path.join(script_dir, "model", "feature_list.json"),
        help="Path to feature_list.json",
    )
    parser.add_argument(
        "--feature-config-path",
        default=None,
        help="Optional JSON config with include/exclude feature lists",
    )
    parser.add_argument(
        "--splits",
        nargs="+",
        default=["train", "test"],
        help="Splits to analyze (default: train test)",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=0.997,
        help="FPR threshold X: analyze benign windows with score >= X",
    )
    parser.add_argument(
        "--report-path",
        default=os.path.join(script_dir, "result", "fpr_deep_analysis.json"),
        help="Output JSON report path",
    )
    return parser.parse_args()


def load_benign_rows(feature_base: str, splits: List[str]) -> pd.DataFrame:
    frames: List[pd.DataFrame] = []

    for split in splits:
        benign_dir = os.path.join(feature_base, split, "benign")
        if not os.path.isdir(benign_dir):
            continue

        for path in sorted(glob.glob(os.path.join(benign_dir, "*.csv"))):
            try:
                df = pd.read_csv(path)
            except Exception as exc:
                print(f"[!] Failed to read {path}: {exc}")
                continue

            if df.empty:
                continue

            df = df.copy()
            df["__source_file"] = path
            df["__split"] = split
            frames.append(df)

    if not frames:
        raise RuntimeError("No benign feature rows found for the selected splits")

    return pd.concat(frames, ignore_index=True)


def build_scoring_frame(
    df: pd.DataFrame,
    feature_cols: List[str],
    config_path: Optional[str],
) -> Tuple[pd.DataFrame, object]:
    return build_feature_frame(
        df=df,
        feature_order=feature_cols,
        config_path=config_path,
    )


def predict_scores_and_contribs(
    booster: xgb.Booster,
    feature_frame: pd.DataFrame,
    feature_cols: List[str],
) -> Tuple[np.ndarray, np.ndarray]:
    dmat = xgb.DMatrix(feature_frame.to_numpy(dtype=np.float32, copy=False), feature_names=feature_cols)
    scores = booster.predict(dmat)
    contribs = booster.predict(dmat, pred_contribs=True)
    return np.asarray(scores, dtype=np.float64), np.asarray(contribs, dtype=np.float64)


def get_case_metadata(row: pd.Series) -> Dict[str, object]:
    return {
        "name": str(row.get("name", "")),
        "pid": int(pd.to_numeric(pd.Series([row.get("pid", -1)]), errors="coerce").fillna(-1).iloc[0]),
        "pid_path": str(row.get("pid_path", "")),
        "time_window_index": int(pd.to_numeric(pd.Series([row.get("time_window_index", -1)]), errors="coerce").fillna(-1).iloc[0]),
        "window_start": int(pd.to_numeric(pd.Series([row.get("window_start", -1)]), errors="coerce").fillna(-1).iloc[0]),
        "window_end": int(pd.to_numeric(pd.Series([row.get("window_end", -1)]), errors="coerce").fillna(-1).iloc[0]),
        "split": str(row.get("__split", "")),
        "source_file": str(row.get("__source_file", "")),
    }


def build_feature_impacts(
    feature_row: pd.Series,
    feature_cols: List[str],
    contrib_row: np.ndarray,
) -> List[Dict[str, object]]:
    impacts = []
    for idx, feature in enumerate(feature_cols):
        contribution = float(contrib_row[idx])
        impacts.append(
            {
                "feature": feature,
                "feature_value": float(feature_row[feature]),
                "contribution": contribution,
                "direction": "increase" if contribution > 0 else ("decrease" if contribution < 0 else "neutral"),
            }
        )

    impacts.sort(key=lambda item: item["contribution"], reverse=True)
    return impacts


def aggregate_feature_impacts(
    cases: List[Dict[str, object]],
    feature_cols: List[str],
) -> Dict[str, List[Dict[str, object]]]:
    bucket: Dict[str, List[float]] = {feature: [] for feature in feature_cols}

    for case in cases:
        for impact in case["feature_impacts"]:
            bucket[str(impact["feature"])].append(float(impact["contribution"]))

    overall = []
    for feature in feature_cols:
        values = np.asarray(bucket[feature], dtype=np.float64)
        if values.size == 0:
            continue
        overall.append(
            {
                "feature": feature,
                "mean_contribution": float(np.mean(values)),
                "mean_abs_contribution": float(np.mean(np.abs(values))),
                "max_contribution": float(np.max(values)),
                "min_contribution": float(np.min(values)),
                "positive_case_count": int(np.sum(values > 0)),
                "negative_case_count": int(np.sum(values < 0)),
            }
        )

    overall_desc = sorted(overall, key=lambda item: item["mean_abs_contribution"], reverse=True)
    positive_desc = sorted(overall, key=lambda item: item["mean_contribution"], reverse=True)
    negative_desc = sorted(overall, key=lambda item: item["mean_contribution"])

    return {
        "overall_influence_desc": overall_desc,
        "increase_score_desc": positive_desc,
        "decrease_score_desc": negative_desc,
    }


def print_case_report(case_index: int, case: Dict[str, object]) -> None:
    print("============================================================")
    print(f"[FPR CASE {case_index}]")
    print(f"[+] score             : {case['score']:.6f}")
    print(f"[+] name              : {case['name']}")
    print(f"[+] pid               : {case['pid']}")
    print(f"[+] pid_path          : {case['pid_path']}")
    print(f"[+] split             : {case['split']}")
    print(f"[+] source_file       : {case['source_file']}")
    print(f"[+] time_window_index : {case['time_window_index']}")
    print(f"[+] window_start      : {case['window_start']}")
    print(f"[+] window_end        : {case['window_end']}")
    print("[+] FEATURE IMPACTS (increase score -> decrease score)")
    for impact in case["feature_impacts"]:
        print(
            f"    {impact['contribution']:+.6f} | {impact['feature']} | "
            f"value={impact['feature_value']}"
        )


def print_overall_report(summary: Dict[str, List[Dict[str, object]]]) -> None:
    print("============================================================")
    print("[+] OVERALL FEATURE INFLUENCE ON FPR CASES (DESC BY MEAN ABS CONTRIBUTION)")
    for rank, item in enumerate(summary["overall_influence_desc"], start=1):
        print(
            f"    {rank:03d}. {item['feature']} | "
            f"mean_abs={item['mean_abs_contribution']:.6f} | "
            f"mean={item['mean_contribution']:+.6f} | "
            f"pos={item['positive_case_count']} | neg={item['negative_case_count']}"
        )

    print("============================================================")
    print("[+] FEATURES THAT MOST INCREASE FALSE POSITIVE SCORE")
    for rank, item in enumerate(summary["increase_score_desc"], start=1):
        print(f"    {rank:03d}. {item['feature']} | mean={item['mean_contribution']:+.6f}")

    print("============================================================")
    print("[+] FEATURES THAT MOST DECREASE FALSE POSITIVE SCORE")
    for rank, item in enumerate(summary["decrease_score_desc"], start=1):
        print(f"    {rank:03d}. {item['feature']} | mean={item['mean_contribution']:+.6f}")


def main() -> None:
    args = parse_args()

    feature_cols = load_feature_list(args.feature_list_path)

    booster = xgb.Booster()
    booster.load_model(args.model_path)
    print(f"[+] Loaded model: {args.model_path}")

    benign_df = load_benign_rows(args.feature_base, args.splits)
    feature_frame, feature_plan = build_scoring_frame(
        df=benign_df,
        feature_cols=feature_cols,
        config_path=args.feature_config_path,
    )
    feature_order = list(feature_plan.feature_order)

    print(f"[+] Loaded benign rows: {len(benign_df)}")
    print(f"[+] Active features   : {len(feature_plan.active_features)}/{len(feature_plan.feature_order)}")

    scores, contribs = predict_scores_and_contribs(booster, feature_frame, feature_order)
    fp_indices = np.flatnonzero(scores >= float(args.threshold))

    print(f"[+] Threshold X      : {float(args.threshold):.6f}")
    print(f"[+] FPR case count   : {int(fp_indices.size)}")

    fp_cases: List[Dict[str, object]] = []
    for rank, row_idx in enumerate(fp_indices.tolist(), start=1):
        raw_row = benign_df.iloc[row_idx]
        masked_row = feature_frame.iloc[row_idx]
        case = get_case_metadata(raw_row)
        case["score"] = float(scores[row_idx])
        case["bias"] = float(contribs[row_idx][-1]) if contribs.shape[1] == len(feature_order) + 1 else None
        case["feature_impacts"] = build_feature_impacts(
            feature_row=masked_row,
            feature_cols=feature_order,
            contrib_row=contribs[row_idx][: len(feature_order)],
        )
        fp_cases.append(case)
        print_case_report(rank, case)

    overall_summary = aggregate_feature_impacts(fp_cases, feature_order) if fp_cases else {
        "overall_influence_desc": [],
        "increase_score_desc": [],
        "decrease_score_desc": [],
    }
    print_overall_report(overall_summary)

    report = {
        "threshold": float(args.threshold),
        "splits_analyzed": args.splits,
        "feature_config_path": args.feature_config_path,
        "feature_selection": {
            "active_features": list(feature_plan.active_features),
            "inactive_features": list(feature_plan.inactive_features),
            "include": list(feature_plan.include),
            "exclude": list(feature_plan.exclude),
        },
        "total_benign_rows": int(len(benign_df)),
        "fpr_case_count": int(fp_indices.size),
        "cases": fp_cases,
        "overall_summary": overall_summary,
    }

    report_dir = os.path.dirname(os.path.abspath(args.report_path))
    os.makedirs(report_dir, exist_ok=True)
    with open(args.report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print("============================================================")
    print(f"[+] Saved report: {args.report_path}")


if __name__ == "__main__":
    main()
