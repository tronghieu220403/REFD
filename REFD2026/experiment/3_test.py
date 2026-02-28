import argparse
import json
import os
from typing import Dict, List, Optional, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.metrics import (
    accuracy_score,
    confusion_matrix,
    f1_score,
    precision_score,
    recall_score,
    roc_curve,
    roc_auc_score,
)


LABEL_COL = "label"
META_COLS = {"name", "pid", "pid_path", "time_window_index", "window_start", "window_end", LABEL_COL}

def load_feature_columns(path: str) -> List[str]:
    with open(path, "r", encoding="utf-8") as f:
        cols = json.load(f)

    if not isinstance(cols, list) or not cols:
        raise RuntimeError("feature_list.json is empty or invalid")

    return [str(c) for c in cols]


def safe_metrics(
    y_true: np.ndarray,
    y_pred: np.ndarray,
    y_prob: Optional[np.ndarray] = None,
) -> Dict[str, object]:
    cm = confusion_matrix(y_true, y_pred, labels=[0, 1])
    tn, fp, fn, tp = cm.ravel()

    roc_auc = float("nan")
    if y_prob is not None and len(np.unique(y_true)) > 1:
        roc_auc = float(roc_auc_score(y_true, y_prob))

    metrics = {
        "confusion_matrix": cm.tolist(),
        "tn": int(tn),
        "fp": int(fp),
        "fn": int(fn),
        "tp": int(tp),
        "fpr": float(fp / (fp + tn)) if (fp + tn) > 0 else 0.0,
        "fnr": float(fn / (fn + tp)) if (fn + tp) > 0 else 0.0,
        "precision": float(precision_score(y_true, y_pred, zero_division=0)),
        "recall": float(recall_score(y_true, y_pred, zero_division=0)),
        "f1": float(f1_score(y_true, y_pred, zero_division=0)),
        "accuracy": float(accuracy_score(y_true, y_pred)),
        "roc_auc": None if not np.isfinite(roc_auc) else roc_auc,
    }
    return metrics


def predict_scores(
    booster: xgb.Booster,
    df: pd.DataFrame,
    feature_cols: List[str],
) -> np.ndarray:
    for col in feature_cols:
        if col not in df.columns:
            df[col] = 0.0

    X = df[feature_cols].apply(pd.to_numeric, errors="coerce").fillna(0.0).astype(np.float32)
    dmat = xgb.DMatrix(X, feature_names=feature_cols)
    return booster.predict(dmat)


def collect_test_predictions(
    feature_base: str,
    include_train_set: bool,
    booster: xgb.Booster,
    feature_cols: List[str],
) -> Tuple[np.ndarray, np.ndarray, List[str], np.ndarray]:
    benign_dir = os.path.join(feature_base, "test", "benign")
    ransom_dir = os.path.join(feature_base, "test", "ransom")
    dir_and_label_pairs = ((benign_dir, 0), (ransom_dir, 1))
    if include_train_set == True:
        benign_dir_train = os.path.join(feature_base, "train", "benign")
        ransom_dir_train = os.path.join(feature_base, "train", "ransom")
        dir_and_label_pairs = ((benign_dir, 0), (ransom_dir, 1), (benign_dir_train, 0), (ransom_dir_train, 1))

    all_probs: List[float] = []
    all_true: List[int] = []
    all_process_keys: List[str] = []
    all_time_windows: List[float] = []

    for dir_path, true_label in dir_and_label_pairs:
        if not os.path.isdir(dir_path):
            continue

        for fn in sorted(os.listdir(dir_path)):
            if not fn.lower().endswith(".csv"):
                continue

            path = os.path.join(dir_path, fn)

            try:
                df = pd.read_csv(path)
            except Exception as exc:
                print(f"[!] Failed to read {path}: {exc}")
                continue

            if df.empty:
                continue

            scores = predict_scores(booster, df, feature_cols)

            fallback_name = os.path.splitext(fn)[0]
            name_series = df["name"].fillna("").astype(str) if "name" in df.columns else pd.Series([fallback_name] * len(df))
            pid_series = pd.to_numeric(df["pid"], errors="coerce").fillna(-1).astype(int) if "pid" in df.columns else pd.Series([-1] * len(df))
            pid_path_series = df["pid_path"].fillna("").astype(str) if "pid_path" in df.columns else pd.Series([""] * len(df))

            process_keys = [
                f"{(n if n else fallback_name)}||{int(p)}||{pp}"
                for n, p, pp in zip(name_series.tolist(), pid_series.tolist(), pid_path_series.tolist())
            ]
            if "time_window_index" in df.columns:
                tw_series = pd.to_numeric(df["time_window_index"], errors="coerce").fillna(np.inf).astype(float)
            elif "window_start" in df.columns:
                tw_series = pd.to_numeric(df["window_start"], errors="coerce").fillna(np.inf).astype(float)
            else:
                tw_series = pd.Series(np.arange(len(df)), dtype=np.float64)

            all_probs.extend(scores.tolist())
            all_true.extend([true_label] * len(scores))
            all_process_keys.extend(process_keys)
            all_time_windows.extend(tw_series.tolist())

    if not all_probs:
        raise RuntimeError("No test feature rows found")

    return (
        np.asarray(all_true, dtype=np.int32),
        np.asarray(all_probs, dtype=np.float64),
        all_process_keys,
        np.asarray(all_time_windows, dtype=np.float64),
    )


def build_name_level(
    y_true: np.ndarray,
    y_prob: np.ndarray,
    process_keys: List[str],
    threshold: float,
) -> Tuple[np.ndarray, np.ndarray]:
    name_max_prob: Dict[str, float] = {}
    name_true: Dict[str, int] = {}

    for n, yt, yp in zip(process_keys, y_true.tolist(), y_prob.tolist()):
        if n not in name_max_prob or yp > name_max_prob[n]:
            name_max_prob[n] = float(yp)

        if n not in name_true:
            name_true[n] = int(yt)
        else:
            name_true[n] = max(name_true[n], int(yt))

    ordered_names = sorted(name_max_prob.keys())
    y_true_name = np.asarray([name_true[n] for n in ordered_names], dtype=np.int32)
    y_prob_name = np.asarray([name_max_prob[n] for n in ordered_names], dtype=np.float64)
    y_pred_name = (y_prob_name >= threshold).astype(np.int32)

    return y_true_name, y_pred_name

def ransomware_detection_time_stats(
    y_true_window: np.ndarray,
    y_pred_window: np.ndarray,
    process_keys: List[str],
    time_windows: np.ndarray,
) -> Dict[str, object]:
    process_true_label: Dict[str, int] = {}
    first_detect_time: Dict[str, float] = {}

    for k, yt, yp, tw in zip(process_keys, y_true_window.tolist(), y_pred_window.tolist(), time_windows.tolist()):
        process_true_label[k] = max(process_true_label.get(k, 0), int(yt))
        if int(yt) == 1 and int(yp) == 1:
            if k not in first_detect_time or tw < first_detect_time[k]:
                first_detect_time[k] = float(tw)

    ransom_processes = [k for k, v in process_true_label.items() if v == 1]
    detected_times = [first_detect_time[k] for k in ransom_processes if k in first_detect_time]

    if not detected_times:
        return {
            "detected_ransom_processes": 0,
            "total_ransom_processes": len(ransom_processes),
            "avg_detection_time_window": None,
            "median_detection_time_window": None,
            "min_detection_time_window": None,
            "max_detection_time_window": None,
            "top_detection_time_windows_desc": [],
        }

    arr = np.asarray(detected_times, dtype=np.float64)
    top_detection_times = sorted((float(x) for x in detected_times), reverse=True)[:10]
    return {
        "detected_ransom_processes": int(arr.size),
        "total_ransom_processes": len(ransom_processes),
        "avg_detection_time_window": float(np.mean(arr)),
        "median_detection_time_window": float(np.median(arr)),
        "min_detection_time_window": float(np.min(arr)),
        "max_detection_time_window": float(np.max(arr)),
        "top_detection_time_windows_desc": top_detection_times,
    }

def parse_args() -> argparse.Namespace:
    script_dir = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description="Evaluate trained model on test feature CSVs")
    parser.add_argument(
        "--feature-base",
        default=os.path.join(script_dir, "features"),
        help="Feature base directory",
    )
    parser.add_argument(
        "--model-path",
        default=os.path.join(script_dir, "model", "model.json"),
        help="Path to trained XGBoost model",
    )
    parser.add_argument(
        "--feature-list-path",
        default=os.path.join(script_dir, "model", "feature_list.json"),
        help="Path to feature_list.json",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=0.5,
        help="Decision threshold",
    )
    parser.add_argument(
        "--thresholds",
        nargs="+",
        type=float,
        default=[0.999],
        help="Optional list of thresholds to evaluate (e.g. --thresholds 0.3 0.5 0.7)",
    )
    parser.add_argument(
        "--report-path",
        default=os.path.join(script_dir, "test_report.json"),
        help="Output test report JSON path",
    )
    parser.add_argument(
        "--roc-plot-path",
        default=os.path.join(script_dir, "model", "test_roc_curve.png"),
        help="Output ROC curve plot path",
    )
    parser.add_argument(
        "--include-train-set",
        default=True,
        help="Include train set to evaluate",
    )

    return parser.parse_args()

def main() -> None:
    args = parse_args()

    feature_cols = load_feature_columns(args.feature_list_path)

    booster = xgb.Booster()
    booster.load_model(args.model_path)

    print(f"[+] Loaded model: {args.model_path}")

    y_true_window, y_prob_window, process_keys, time_windows = collect_test_predictions(
        feature_base=args.feature_base,
        include_train_set=args.include_train_set,
        booster=booster,
        feature_cols=feature_cols,
    )

    thresholds = args.thresholds if args.thresholds is not None else [float(args.threshold)]
    thresholds = sorted(set(float(t) for t in thresholds))

    roc_plot_path = None
    first_window_metrics = None
    first_name_metrics = None

    if len(np.unique(y_true_window)) > 1:
        fpr_curve, tpr_curve, _ = roc_curve(y_true_window, y_prob_window)
        roc_auc_value = float(roc_auc_score(y_true_window, y_prob_window))
        roc_plot_path = args.roc_plot_path
        roc_plot_dir = os.path.dirname(os.path.abspath(roc_plot_path))
        os.makedirs(roc_plot_dir, exist_ok=True)
        plt.figure(figsize=(7, 6))
        plt.plot(fpr_curve, tpr_curve, label=f"ROC (AUC = {roc_auc_value:.4f})")
        plt.plot([0, 1], [0, 1], linestyle="--", color="gray", label="Random")
        plt.xlim(0.0, 1.0)
        plt.ylim(0.0, 1.0)
        plt.xlabel("False Positive Rate")
        plt.ylabel("True Positive Rate")
        plt.title("Test ROC Curve")
        plt.legend(loc="lower right")
        plt.tight_layout()
        plt.savefig(roc_plot_path, dpi=200, bbox_inches="tight")
        plt.close()

    threshold_reports = []
    for idx, threshold in enumerate(thresholds):
        y_pred_window = (y_prob_window >= threshold).astype(np.int32)
        window_metrics = safe_metrics(y_true_window, y_pred_window, y_prob=y_prob_window)
        detection_stats = ransomware_detection_time_stats(
            y_true_window=y_true_window,
            y_pred_window=y_pred_window,
            process_keys=process_keys,
            time_windows=time_windows,
        )

        y_true_name, y_pred_name = build_name_level(
            y_true=y_true_window,
            y_prob=y_prob_window,
            process_keys=process_keys,
            threshold=threshold,
        )
        name_metrics = safe_metrics(y_true_name, y_pred_name, y_prob=None)
        name_metrics.pop("roc_auc", None)

        if idx == 0:
            first_window_metrics = window_metrics
            first_name_metrics = name_metrics

        print("\n============================================================")
        print(f"[+] Threshold         : {threshold:.6f}")
        print("[X] WINDOW-LEVEL METRICS")
        print(f"   [+] Confusion matrix  : {window_metrics['confusion_matrix']}")
        # print(f"   [+] Precision         : {window_metrics['precision']:.6f}")
        # print(f"   [+] Recall            : {window_metrics['recall']:.6f}")
        # print(f"   [+] F1                : {window_metrics['f1']:.6f}")
        # print(f"[+] Accuracy          : {window_metrics['accuracy']:.6f}")
        print(f"   [+] FPR               : {window_metrics['fpr']:.6f}")
        # print(f"   [+] TPR               : {name_metrics['recall']:.6f}")
        # print(f"   [+] FNR               : {window_metrics['fnr']:.6f}")
        print("[X] PID-LEVEL METRICS (OR AGGREGATION BY <name+pid+pid_path>)")
        print(f"   [+] Confusion matrix  : {name_metrics['confusion_matrix']}")
        # print(f"   [+] Precision         : {name_metrics['precision']:.6f}")
        # print(f"   [+] Recall            : {name_metrics['recall']:.6f}")
        # print(f"   [+] F1                : {name_metrics['f1']:.6f}")
        # print(f"   [+] Accuracy          : {name_metrics['accuracy']:.6f}")
        # print(f"   [+] FPR               : {name_metrics['fpr']:.6f}")
        print(f"   [+] TPR               : {name_metrics['recall']:.6f}")
        # print(f"   [+] FNR               : {name_metrics['fnr']:.6f}")
        print("[X] RANSOM DETECTION TIME (time_window_index)")
        print(f"   [+] Detected/Total    : {detection_stats['detected_ransom_processes']}/{detection_stats['total_ransom_processes']}")
        print(f"   [+] Avg               : {detection_stats['avg_detection_time_window']}")
        print(f"   [+] Max               : {detection_stats['max_detection_time_window']}")
        print(f"   [+] Top-10 desc       : {detection_stats['top_detection_time_windows_desc']}")
        print("============================================================\n")

        threshold_reports.append(
            {
                "threshold": threshold,
                "window_metrics": window_metrics,
                "name_metrics": name_metrics,
                "ransom_detection_time_stats": detection_stats,
            }
        )

    report = {
        "thresholds_used": thresholds,
        "results_by_threshold": threshold_reports,
        "roc_plot_path": roc_plot_path,
        # Backward-compat fields: keep first-threshold summary if downstream code expects it.
        "threshold_used": thresholds[0],
        "window_metrics": first_window_metrics,
        "name_metrics": first_name_metrics,
    }

    report_dir = os.path.dirname(os.path.abspath(args.report_path))
    os.makedirs(report_dir, exist_ok=True)

    with open(args.report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print("============================================================")
    print(f"ROC (AUC = {roc_auc_value:.4f})") # type: ignore
    print(f"[+] Saved report: {args.report_path}")
    if roc_plot_path is not None:
        print(f"[+] Saved ROC plot: {roc_plot_path}")


if __name__ == "__main__":
    main()
