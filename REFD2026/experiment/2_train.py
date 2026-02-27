import argparse
import glob
import json
import os
import random
from typing import List, Optional, Tuple

import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import GroupShuffleSplit, train_test_split


LABEL_COL = "label"
META_COLS = {"name", "pid", "pid_path", "time_window_index", "window_start", "window_end", LABEL_COL}

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


def load_dataframe(csv_files: List[str]) -> pd.DataFrame:
    frames = []

    for path in csv_files:
        try:
            df = pd.read_csv(path)
        except Exception as exc:
            print(f"[!] Failed to read {path}: {exc}")
            continue

        if df.empty:
            continue

        df["__source_file"] = path
        frames.append(df)

    if not frames:
        raise RuntimeError("No valid training CSV files found")

    out = pd.concat(frames, ignore_index=True)

    if LABEL_COL not in out.columns:
        raise RuntimeError(f"Missing required label column: {LABEL_COL}")

    out[LABEL_COL] = pd.to_numeric(out[LABEL_COL], errors="coerce").fillna(0).astype(int)
    out[LABEL_COL] = (out[LABEL_COL] == 1).astype(int)
    return out


def infer_feature_columns(df: pd.DataFrame) -> List[str]:
    candidate = [c for c in df.columns if c not in META_COLS and c != "__source_file"]
    if not candidate:
        raise RuntimeError("No candidate feature columns found")

    for col in candidate:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    feature_cols = [c for c in candidate if pd.api.types.is_numeric_dtype(df[c])]
    if not feature_cols:
        raise RuntimeError("No numeric feature columns found after type coercion")

    return feature_cols


def split_train_valid(
    df: pd.DataFrame,
    random_seed: int,
    val_size: float,
) -> Tuple[np.ndarray, np.ndarray, str]:
    y = df[LABEL_COL].values

    if "name" in df.columns and df["name"].notna().any():
        groups = df["name"].fillna("__missing__").astype(str)
        if groups.nunique() >= 2:
            splitter = GroupShuffleSplit(n_splits=1, test_size=val_size, random_state=random_seed)
            train_idx, valid_idx = next(splitter.split(df, y=y, groups=groups)) # type:ignore
            return train_idx, valid_idx, "group_by_name"

    stratify = y if len(np.unique(y)) > 1 else None # type:ignore
    idx = np.arange(len(df))
    train_idx, valid_idx = train_test_split(
        idx,
        test_size=val_size,
        random_state=random_seed,
        stratify=stratify, # type:ignore
    )
    return train_idx, valid_idx, "stratified_random"

def parse_args() -> argparse.Namespace:
    script_dir = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description="Train XGBoost malware detector")
    parser.add_argument(
        "--input-glob",
        nargs="+",
        default=[
            os.path.join(script_dir, "features", "train", "benign", "*.csv"),
            os.path.join(script_dir, "features", "train", "ransom", "*.csv"),
        ],
        help="CSV glob patterns or directories",
    )
    parser.add_argument(
        "--model-dir",
        default=os.path.join(script_dir, "model"),
        help="Directory to save model artifacts",
    )
    parser.add_argument("--random-seed", type=int, default=42, help="Random seed")
    parser.add_argument("--val-size", type=float, default=0.2, help="Validation split ratio")
    parser.add_argument(
        "--max-ransom-time-window",
        type=int,
        default=100000,
        help="Keep ransomware rows with time_window_index < this value",
    )

    parser.add_argument("--n-estimators", type=int, default=2000)
    parser.add_argument("--learning-rate", type=float, default=0.05)
    parser.add_argument("--max-depth", type=int, default=6)
    parser.add_argument("--subsample", type=float, default=0.9)
    parser.add_argument("--colsample-bytree", type=float, default=0.9)
    parser.add_argument("--min-child-weight", type=float, default=1.0)
    parser.add_argument("--reg-lambda", type=float, default=1.0)
    parser.add_argument("--early-stopping-rounds", type=int, default=50)
    parser.add_argument("--n-jobs", type=int, default=1, help="Set 1 for max determinism")

    return parser.parse_args()

def main() -> None:
    args = parse_args()

    random.seed(args.random_seed)
    np.random.seed(args.random_seed)

    files = expand_input_paths(args.input_glob)
    if not files:
        raise RuntimeError("No CSV files matched --input-glob")

    df = load_dataframe(files)

    if "time_window_index" in df.columns:
        tw = pd.to_numeric(df["time_window_index"], errors="coerce").fillna(10**9).astype(int)
        mask_keep = ~((df[LABEL_COL] == 1) & (tw >= int(args.max_ransom_time_window)))
        removed = int((~mask_keep).sum())
        if removed > 0:
            print(f"[+] Filtered ransomware rows by time_window_index: removed={removed}")
        df = df.loc[mask_keep].copy()

    feature_cols = infer_feature_columns(df)

    for col in feature_cols:
        df[col] = pd.to_numeric(df[col], errors="coerce").fillna(0.0)

    train_idx, valid_idx, split_method = split_train_valid(
        df=df,
        random_seed=args.random_seed,
        val_size=args.val_size,
    )

    train_df = df.iloc[train_idx].copy()
    valid_df = df.iloc[valid_idx].copy()

    X_train = train_df[feature_cols].astype(np.float32)
    y_train = train_df[LABEL_COL].astype(int).values

    X_valid = valid_df[feature_cols].astype(np.float32)
    y_valid = valid_df[LABEL_COL].astype(int).values

    params = {
        "objective": "binary:logistic",
        "eval_metric": ["auc", "logloss"],
        "n_estimators": args.n_estimators,
        "learning_rate": args.learning_rate,
        "max_depth": args.max_depth,
        "subsample": args.subsample,
        "colsample_bytree": args.colsample_bytree,
        "min_child_weight": args.min_child_weight,
        "reg_lambda": args.reg_lambda,
        "random_state": args.random_seed,
        "n_jobs": args.n_jobs,
        "tree_method": "hist",
        "early_stopping_rounds": args.early_stopping_rounds,
    }

    model = xgb.XGBClassifier(**params)
    model.fit(
        X_train,
        y_train,
        eval_set=[(X_valid, y_valid)],
        verbose=False,
    )

    os.makedirs(args.model_dir, exist_ok=True)

    model_path = os.path.join(args.model_dir, "model.json")
    meta_path = os.path.join(args.model_dir, "model_meta.json")
    feature_path = os.path.join(args.model_dir, "feature_list.json")

    booster = model.get_booster()
    booster.save_model(model_path)

    with open(feature_path, "w", encoding="utf-8") as f:
        json.dump(feature_cols, f, indent=2)

    meta = {
        "model_path": model_path,
        "feature_order": feature_cols,
        "random_seed": int(args.random_seed),
        "split_method": split_method,
        "train_rows": int(len(train_df)),
        "valid_rows": int(len(valid_df)),
        "params": params,
        "best_iteration": int(model.best_iteration) if getattr(model, "best_iteration", None) is not None else None,
    }

    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)

    print("============================================================")
    print(f"[+] Saved model      : {model_path}")
    print(f"[+] Saved meta       : {meta_path}")
    print(f"[+] Saved feature list: {feature_path}")


if __name__ == "__main__":
    main()
