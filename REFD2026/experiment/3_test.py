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
META_COLS = {"name", "pid", "pid_path", "window_start", "window_end", LABEL_COL}


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
        default=[0.5, 0.7, 0.9, 0.95, 0.97, 0.99, 0.993, 0.995, 0.997],
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
    return parser.parse_args()


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
    booster: xgb.Booster,
    feature_cols: List[str],
) -> Tuple[np.ndarray, np.ndarray, List[str]]:
    benign_dir = os.path.join(feature_base, "test", "benign")
    ransom_dir = os.path.join(feature_base, "test", "ransom")

    all_probs: List[float] = []
    all_true: List[int] = []
    all_names: List[str] = []

    for dir_path, true_label in ((benign_dir, 0), (ransom_dir, 1)):
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

            if "name" in df.columns:
                names = df["name"].fillna("").astype(str).tolist()
                fallback_name = os.path.splitext(fn)[0]
                names = [n if n else fallback_name for n in names]
            else:
                names = [os.path.splitext(fn)[0]] * len(scores)

            all_probs.extend(scores.tolist())
            all_true.extend([true_label] * len(scores))
            all_names.extend(names)

    if not all_probs:
        raise RuntimeError("No test feature rows found")

    return np.asarray(all_true, dtype=np.int32), np.asarray(all_probs, dtype=np.float64), all_names


def build_name_level(
    y_true: np.ndarray,
    y_prob: np.ndarray,
    names: List[str],
    threshold: float,
) -> Tuple[np.ndarray, np.ndarray]:
    name_max_prob: Dict[str, float] = {}
    name_true: Dict[str, int] = {}

    for n, yt, yp in zip(names, y_true.tolist(), y_prob.tolist()):
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


def main() -> None:
    args = parse_args()

    feature_cols = load_feature_columns(args.feature_list_path)

    booster = xgb.Booster()
    booster.load_model(args.model_path)

    print(f"[+] Loaded model: {args.model_path}")

    y_true_window, y_prob_window, names = collect_test_predictions(
        feature_base=args.feature_base,
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

        y_true_name, y_pred_name = build_name_level(
            y_true=y_true_window,
            y_prob=y_prob_window,
            names=names,
            threshold=threshold,
        )
        name_metrics = safe_metrics(y_true_name, y_pred_name, y_prob=None)
        name_metrics.pop("roc_auc", None)

        if idx == 0:
            first_window_metrics = window_metrics
            first_name_metrics = name_metrics

        print(f"[+] Threshold         : {threshold:.6f}")

        print("============================================================")
        print("[+] WINDOW-LEVEL METRICS")
        print(f"[+] Confusion matrix  : {window_metrics['confusion_matrix']}")
        # print(f"[+] Precision         : {window_metrics['precision']:.6f}")
        # print(f"[+] Recall            : {window_metrics['recall']:.6f}")
        # print(f"[+] F1                : {window_metrics['f1']:.6f}")
        # print(f"[+] Accuracy          : {window_metrics['accuracy']:.6f}")
        print(f"[+] FPR               : {window_metrics['fpr']:.6f}")
        print(f"[+] FNR               : {window_metrics['fnr']:.6f}")
        print("[+] NAME-LEVEL METRICS (OR AGGREGATION BY NAME)")
        print(f"[+] Confusion matrix  : {name_metrics['confusion_matrix']}")
        # print(f"[+] Precision         : {name_metrics['precision']:.6f}")
        # print(f"[+] Recall            : {name_metrics['recall']:.6f}")
        # print(f"[+] F1                : {name_metrics['f1']:.6f}")
        # print(f"[+] Accuracy          : {name_metrics['accuracy']:.6f}")
        print(f"[+] FPR               : {name_metrics['fpr']:.6f}")
        print(f"[+] FNR               : {name_metrics['fnr']:.6f}")
        print("============================================================")

        threshold_reports.append(
            {
                "threshold": threshold,
                "window_metrics": window_metrics,
                "name_metrics": name_metrics,
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
    print(f"[+] Saved report: {args.report_path}")
    if roc_plot_path is not None:
        print(f"[+] Saved ROC plot: {roc_plot_path}")


if __name__ == "__main__":
    main()
