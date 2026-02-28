# type: ignore
import argparse
import glob
import json
import os
from typing import Dict, List, Optional, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import shap
import xgboost as xgb

from feature_vector_utils import build_feature_frame, load_feature_list


def parse_args() -> argparse.Namespace:
    script_dir = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description="Analyze feature importance with SHAP and XGBoost gain")
    parser.add_argument(
        "--model-path",
        default=os.path.join(script_dir, "model", "model.json"),
        help="Path to model.json",
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
        "--input-glob",
        nargs="+",
        default=[
            os.path.join(script_dir, "features", "train", "benign", "*.csv"),
            os.path.join(script_dir, "features", "train", "ransom", "*.csv"),
        ],
        help="Training feature CSV patterns",
    )
    parser.add_argument("--sample-size", type=int, default=5000, help="Max sampled rows")
    parser.add_argument("--random-seed", type=int, default=42)
    parser.add_argument(
        "--output-dir",
        default=os.path.join(script_dir, "model"),
        help="Directory for outputs",
    )
    return parser.parse_args()


def expand_input_paths(patterns: List[str]) -> List[str]:
    files: List[str] = []

    for pattern in patterns:
        if os.path.isdir(pattern):
            files.extend(glob.glob(os.path.join(pattern, "*.csv")))
            continue

        matches = glob.glob(pattern)
        if matches:
            files.extend(matches)
            continue

        if os.path.isfile(pattern) and pattern.lower().endswith(".csv"):
            files.append(pattern)

    return sorted(set(files))


def load_training_sample(
    csv_files: List[str],
    feature_cols: List[str],
    sample_size: int,
    random_seed: int,
    config_path: Optional[str],
) -> Tuple[pd.DataFrame, object]:
    frames = []

    for path in csv_files:
        try:
            df = pd.read_csv(path)
        except Exception as exc:
            print(f"[!] Failed to read {path}: {exc}")
            continue

        if df.empty:
            continue

        frames.append(df)

    if not frames:
        raise RuntimeError("No valid feature rows loaded for analysis")

    all_df = pd.concat(frames, ignore_index=True)
    if len(all_df) > sample_size:
        all_df = all_df.sample(n=sample_size, random_state=random_seed)

    feature_frame, feature_plan = build_feature_frame(
        df=all_df,
        feature_order=feature_cols,
        config_path=config_path,
    )
    return feature_frame, feature_plan


def map_gain_key(key: str, feature_cols: List[str]) -> str:
    if key in feature_cols:
        return key

    if key.startswith("f") and key[1:].isdigit():
        idx = int(key[1:])
        if 0 <= idx < len(feature_cols):
            return feature_cols[idx]

    return key


def save_json(path: str, obj: object) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=2)


def main() -> None:
    args = parse_args()
    np.random.seed(args.random_seed)

    os.makedirs(args.output_dir, exist_ok=True)

    csv_files = expand_input_paths(args.input_glob)
    if not csv_files:
        raise RuntimeError("No CSV files matched --input-glob")

    feature_cols = load_feature_list(args.feature_list_path)

    booster = xgb.Booster()
    booster.load_model(args.model_path)

    X, feature_plan = load_training_sample(
        csv_files=csv_files,
        feature_cols=feature_cols,
        sample_size=max(1, int(args.sample_size)),
        random_seed=args.random_seed,
        config_path=args.feature_config_path,
    )

    feature_order = list(feature_plan.feature_order)
    explainer = shap.TreeExplainer(booster)
    shap_values = explainer.shap_values(X)

    if isinstance(shap_values, list):
        shap_values = shap_values[0]

    shap_values = np.asarray(shap_values)
    if shap_values.ndim != 2:
        raise RuntimeError(f"Unexpected SHAP shape: {shap_values.shape}")

    if shap_values.shape[1] == len(feature_order) + 1:
        shap_core = shap_values[:, : len(feature_order)]
    elif shap_values.shape[1] == len(feature_order):
        shap_core = shap_values
    else:
        raise RuntimeError(
            f"SHAP feature dimension mismatch: {shap_values.shape[1]} vs {len(feature_order)}"
        )

    mean_abs_shap = np.mean(np.abs(shap_core), axis=0)
    shap_rank = [
        {"feature": feature_order[idx], "mean_abs_shap": float(mean_abs_shap[idx])}
        for idx in range(len(feature_order))
    ]
    shap_rank.sort(key=lambda item: item["mean_abs_shap"], reverse=True)

    top_30 = shap_rank[:30]
    bottom_30 = list(reversed(shap_rank[-30:]))

    top_path = os.path.join(args.output_dir, "top_30_features.json")
    bottom_path = os.path.join(args.output_dir, "bottom_30_features.json")
    save_json(top_path, top_30)
    save_json(bottom_path, bottom_30)

    plt.figure(figsize=(10, 8))
    shap.summary_plot(
        shap_core,
        X,
        plot_type="bar",
        max_display=min(30, len(feature_order)),
        show=False,
    )
    plt.tight_layout()
    shap_bar_path = os.path.join(args.output_dir, "shap_summary_bar.png")
    plt.savefig(shap_bar_path, dpi=200, bbox_inches="tight")
    plt.close()

    shap_beeswarm_path = os.path.join(args.output_dir, "shap_beeswarm.png")
    try:
        plt.figure(figsize=(10, 8))
        shap.summary_plot(
            shap_core,
            X,
            max_display=min(30, len(feature_order)),
            show=False,
        )
        plt.tight_layout()
        plt.savefig(shap_beeswarm_path, dpi=200, bbox_inches="tight")
        plt.close()
    except Exception as exc:
        plt.close("all")
        print(f"[!] SHAP beeswarm skipped: {exc}")

    gain_raw = booster.get_score(importance_type="gain")
    gain_map: Dict[str, float] = {feature: 0.0 for feature in feature_order}
    for key, value in gain_raw.items():
        mapped = map_gain_key(str(key), feature_order)
        if mapped in gain_map:
            gain_map[mapped] = float(value)

    full_ranking_path = os.path.join(args.output_dir, "full_feature_ranking_shap.json")
    save_json(full_ranking_path, shap_rank)

    gain_path = os.path.join(args.output_dir, "importance_gain_raw.json")
    save_json(gain_path, gain_map)

    gain_sorted = sorted(gain_map.items(), key=lambda item: item[1], reverse=True)[:30]
    labels = [item[0] for item in gain_sorted][::-1]
    values = [item[1] for item in gain_sorted][::-1]

    plt.figure(figsize=(10, 8))
    plt.barh(labels, values)
    plt.xlabel("Gain")
    plt.ylabel("Feature")
    plt.title("XGBoost Gain Importance (Top 30)")
    plt.tight_layout()
    gain_plot_path = os.path.join(args.output_dir, "xgb_gain_importance.png")
    plt.savefig(gain_plot_path, dpi=200, bbox_inches="tight")
    plt.close()

    print("============================================================")
    print(f"[+] Active features: {len(feature_plan.active_features)}/{len(feature_plan.feature_order)}")
    print("[+] FULL FEATURE RANKING (SHAP, HIGH -> LOW)")
    for rank, item in enumerate(shap_rank, start=1):
        print(f"    {rank:03d}. {item['feature']}: {item['mean_abs_shap']:.6f}")

    print("============================================================")
    print(f"[+] Saved: {shap_bar_path}")
    print(f"[+] Saved: {shap_beeswarm_path} (if generated)")
    print(f"[+] Saved: {gain_plot_path}")
    print(f"[+] Saved: {top_path}")
    print(f"[+] Saved: {bottom_path}")
    print(f"[+] Saved: {full_ranking_path}")
    print(f"[+] Saved: {gain_path}")


if __name__ == "__main__":
    main()
