# type: ignore
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
    roc_auc_score,
    roc_curve,
)

from feature_vector_utils import build_feature_matrix, load_feature_list


LABEL_COL = "label"


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

    return {
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


def predict_scores(
    booster: xgb.Booster,
    df: pd.DataFrame,
    feature_cols: List[str],
    config_path: Optional[str],
) -> Tuple[np.ndarray, object]:
    matrix, feature_plan = build_feature_matrix(
        df=df,
        feature_order=feature_cols,
        config_path=config_path,
    )
    dmat = xgb.DMatrix(matrix, feature_names=list(feature_plan.feature_order))
    scores = booster.predict(dmat)
    return np.asarray(scores, dtype=np.float64), feature_plan


def resolve_splits(include_train_set: bool) -> List[str]:
    return ["train", "test"] if include_train_set else ["test"]


def collect_predictions(
    feature_base: str,
    splits: List[str],
    booster: xgb.Booster,
    feature_cols: List[str],
    config_path: Optional[str],
) -> Tuple[np.ndarray, np.ndarray, List[str], List[str], np.ndarray, object]:
    all_probs: List[float] = []
    all_true: List[int] = []
    all_entity_keys: List[str] = []
    all_ransom_names: List[str] = []
    all_time_windows: List[float] = []
    feature_plan = None

    for split in splits:
        for class_name, true_label in (("benign", 0), ("ransom", 1)):
            dir_path = os.path.join(feature_base, split, class_name)
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

                scores, current_plan = predict_scores(
                    booster=booster,
                    df=df,
                    feature_cols=feature_cols,
                    config_path=config_path,
                )
                if feature_plan is None:
                    feature_plan = current_plan

                fallback_name = os.path.splitext(fn)[0]
                name_series = df["name"].fillna("").astype(str) if "name" in df.columns else pd.Series([fallback_name] * len(df))
                pid_series = pd.to_numeric(df["pid"], errors="coerce").fillna(-1).astype(int) if "pid" in df.columns else pd.Series([-1] * len(df))
                pid_path_series = df["pid_path"].fillna("").astype(str) if "pid_path" in df.columns else pd.Series([""] * len(df))

                names = [(name if name else fallback_name) for name in name_series.tolist()]
                entity_keys = [
                    name if true_label == 1 else f"{split}||{name}||{int(pid)}||{pid_path}"
                    for name, pid, pid_path in zip(
                        names,
                        pid_series.tolist(),
                        pid_path_series.tolist(),
                    )
                ]

                if "time_window_index" in df.columns:
                    tw_series = pd.to_numeric(df["time_window_index"], errors="coerce").fillna(np.inf).astype(float)
                elif "window_start" in df.columns:
                    tw_series = pd.to_numeric(df["window_start"], errors="coerce").fillna(np.inf).astype(float)
                else:
                    tw_series = pd.Series(np.arange(len(df)), dtype=np.float64)

                all_probs.extend(scores.tolist())
                all_true.extend([true_label] * len(scores))
                all_entity_keys.extend(entity_keys)
                all_ransom_names.extend(names)
                all_time_windows.extend(tw_series.tolist())

    if not all_probs:
        raise RuntimeError("No feature rows found for the selected evaluation splits")

    return (
        np.asarray(all_true, dtype=np.int32),
        np.asarray(all_probs, dtype=np.float64),
        all_entity_keys,
        all_ransom_names,
        np.asarray(all_time_windows, dtype=np.float64),
        feature_plan,
    )


def build_name_level(
    y_true: np.ndarray,
    y_prob: np.ndarray,
    entity_keys: List[str],
    threshold: float,
) -> Tuple[np.ndarray, np.ndarray]:
    entity_max_prob: Dict[str, float] = {}
    entity_true: Dict[str, int] = {}

    for entity_key, y_true_item, y_prob_item in zip(entity_keys, y_true.tolist(), y_prob.tolist()):
        if entity_key not in entity_max_prob or y_prob_item > entity_max_prob[entity_key]:
            entity_max_prob[entity_key] = float(y_prob_item)

        if entity_key not in entity_true:
            entity_true[entity_key] = int(y_true_item)
        else:
            entity_true[entity_key] = max(entity_true[entity_key], int(y_true_item))

    ordered_keys = sorted(entity_max_prob.keys())
    y_true_name = np.asarray([entity_true[key] for key in ordered_keys], dtype=np.int32)
    y_prob_name = np.asarray([entity_max_prob[key] for key in ordered_keys], dtype=np.float64)
    y_pred_name = (y_prob_name >= threshold).astype(np.int32)
    return y_true_name, y_pred_name


def ransomware_detection_time_stats(
    y_true_window: np.ndarray,
    y_pred_window: np.ndarray,
    ransom_names: List[str],
    time_windows: np.ndarray,
) -> Dict[str, object]:
    ransom_name_set = set()
    first_detect_time: Dict[str, float] = {}

    for ransom_name, y_true_item, y_pred_item, time_window in zip(
        ransom_names,
        y_true_window.tolist(),
        y_pred_window.tolist(),
        time_windows.tolist(),
    ):
        if int(y_true_item) != 1:
            continue
        ransom_name_set.add(ransom_name)
        if int(y_pred_item) == 1:
            if ransom_name not in first_detect_time or time_window < first_detect_time[ransom_name]:
                first_detect_time[ransom_name] = float(time_window)

    ordered_ransom_names = sorted(ransom_name_set)
    detected_times = [first_detect_time[name] for name in ordered_ransom_names if name in first_detect_time]

    if not detected_times:
        return {
            "detected_ransom_names": 0,
            "total_ransom_names": len(ordered_ransom_names),
            "avg_detection_time_window": None,
            "median_detection_time_window": None,
            "min_detection_time_window": None,
            "max_detection_time_window": None,
            "top_detection_time_windows_desc": [],
        }

    arr = np.asarray(detected_times, dtype=np.float64)
    return {
        "detected_ransom_names": int(arr.size),
        "total_ransom_names": len(ordered_ransom_names),
        "avg_detection_time_window": float(np.mean(arr)),
        "median_detection_time_window": float(np.median(arr)),
        "min_detection_time_window": float(np.min(arr)),
        "max_detection_time_window": float(np.max(arr)),
        "top_detection_time_windows_desc": sorted((float(x) for x in detected_times), reverse=True)[:10],
    }


def parse_args() -> argparse.Namespace:
    script_dir = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description="Evaluate trained model on feature CSVs")
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
        "--feature-config-path",
        default=None,
        help="Optional JSON config with include/exclude feature lists",
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
        default=[0.997],
        help="Optional list of thresholds to evaluate (e.g. --thresholds 0.3 0.5 0.7)",
    )
    parser.add_argument(
        "--report-path",
        default=os.path.join(script_dir, "result", "test_report.json"),
        help="Output test report JSON path",
    )
    parser.add_argument(
        "--roc-plot-path",
        default=os.path.join(script_dir, "result", "test_roc_curve.png"),
        help="Output ROC curve plot path",
    )
    parser.add_argument(
        "--include-train-set",
        dest="include_train_set",
        action="store_true",
        default=True,
        help="Evaluate on both train and test splits",
    )
    parser.add_argument(
        "--test-only",
        dest="include_train_set",
        action="store_false",
        help="Evaluate on the test split only",
    )

    return parser.parse_args()


def main() -> None:
    args = parse_args()

    feature_cols = load_feature_list(args.feature_list_path)

    booster = xgb.Booster()
    booster.load_model(args.model_path)

    splits = resolve_splits(args.include_train_set)
    print(f"[+] Loaded model: {args.model_path}")
    print(f"[+] Evaluation splits: {splits}")

    y_true_window, y_prob_window, entity_keys, ransom_names, time_windows, feature_plan = collect_predictions(
        feature_base=args.feature_base,
        splits=splits,
        booster=booster,
        feature_cols=feature_cols,
        config_path=args.feature_config_path,
    )

    thresholds = args.thresholds if args.thresholds else [float(args.threshold)]
    thresholds = sorted(set(float(item) for item in thresholds))

    roc_plot_path = None
    roc_auc_value = None
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
        plt.title("Evaluation ROC Curve")
        plt.legend(loc="lower right")
        plt.tight_layout()
        plt.savefig(roc_plot_path, dpi=200, bbox_inches="tight")
        plt.close()

    first_window_metrics = None
    first_name_metrics = None
    threshold_reports = []

    for idx, threshold in enumerate(thresholds):
        y_pred_window = (y_prob_window >= threshold).astype(np.int32)
        window_metrics = safe_metrics(y_true_window, y_pred_window, y_prob=y_prob_window)
        detection_stats = ransomware_detection_time_stats(
            y_true_window=y_true_window,
            y_pred_window=y_pred_window,
            ransom_names=ransom_names,
            time_windows=time_windows,
        )

        y_true_name, y_pred_name = build_name_level(
            y_true=y_true_window,
            y_prob=y_prob_window,
            entity_keys=entity_keys,
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
        print("[X] ENTITY-LEVEL METRICS (benign: split+name+pid+pid_path, ransom: name)")
        print(f"   [+] Confusion matrix  : {name_metrics['confusion_matrix']}")
        # print(f"   [+] Precision         : {name_metrics['precision']:.6f}")
        # print(f"   [+] Recall            : {name_metrics['recall']:.6f}")
        # print(f"   [+] F1                : {name_metrics['f1']:.6f}")
        # print(f"   [+] Accuracy          : {name_metrics['accuracy']:.6f}")
        # print(f"   [+] FPR               : {name_metrics['fpr']:.6f}")
        print(f"   [+] TPR               : {name_metrics['recall']:.6f}")
        # print(f"   [+] FNR               : {name_metrics['fnr']:.6f}")
        print("[X] RANSOM DETECTION TIME (time_window_index)")
        print(f"   [+] Detected/Total    : {detection_stats['detected_ransom_names']}/{detection_stats['total_ransom_names']}")
        print(f"   [+] Avg               : {detection_stats['avg_detection_time_window']}")
        print(f"   [+] Median, Min, Max  : {detection_stats['median_detection_time_window']}, {detection_stats['min_detection_time_window']}, {detection_stats['max_detection_time_window']}")
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
        "splits_evaluated": splits,
        "thresholds_used": thresholds,
        "feature_config_path": args.feature_config_path,
        "feature_selection": {
            "active_features": list(feature_plan.active_features),
            "inactive_features": list(feature_plan.inactive_features),
            "include": list(feature_plan.include),
            "exclude": list(feature_plan.exclude),
        } if feature_plan is not None else None,
        "results_by_threshold": threshold_reports,
        "roc_plot_path": roc_plot_path,
        "threshold_used": thresholds[0],
        "window_metrics": first_window_metrics,
        "name_metrics": first_name_metrics,
    }

    report_dir = os.path.dirname(os.path.abspath(args.report_path))
    os.makedirs(report_dir, exist_ok=True)
    with open(args.report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print("============================================================")
    if feature_plan is not None:
        print(f"[+] Active features: {len(feature_plan.active_features)}/{len(feature_plan.feature_order)}")
    if roc_auc_value is not None:
        print(f"ROC (AUC = {roc_auc_value:.4f})")
    print(f"[+] Saved report: {args.report_path}")
    if roc_plot_path is not None:
        print(f"[+] Saved ROC plot: {roc_plot_path}")


if __name__ == "__main__":
    main()
