# type: ignore
import json
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Set, Tuple

import numpy as np
import pandas as pd


DEFAULT_DTYPE = np.float32


@dataclass(frozen=True)
class FeatureSelectionConfig:
    include: Tuple[str, ...]
    exclude: Tuple[str, ...]


@dataclass(frozen=True)
class FeatureReadPlan:
    feature_order: Tuple[str, ...]
    active_features: Tuple[str, ...]
    inactive_features: Tuple[str, ...]
    include: Tuple[str, ...]
    exclude: Tuple[str, ...]


def load_feature_list(feature_list_path: str) -> List[str]:
    with open(feature_list_path, "r", encoding="utf-8") as f:
        payload = json.load(f)

    if not isinstance(payload, list) or not payload:
        raise RuntimeError("feature_list.json is empty or invalid")

    return [str(item) for item in payload]


def load_feature_selection_config(config_path: Optional[str] = None) -> FeatureSelectionConfig:
    if not config_path:
        return FeatureSelectionConfig(include=(), exclude=())

    with open(config_path, "r", encoding="utf-8") as f:
        payload = json.load(f)

    if payload is None:
        payload = {}
    if not isinstance(payload, dict):
        raise RuntimeError("Feature config JSON must be an object with 'include'/'exclude'")

    include_raw = payload.get("include", [])
    exclude_raw = payload.get("exclude", [])

    if include_raw is None:
        include_raw = []
    if exclude_raw is None:
        exclude_raw = []

    if not isinstance(include_raw, list) or not isinstance(exclude_raw, list):
        raise RuntimeError("'include' and 'exclude' must be arrays")

    include = tuple(str(item) for item in include_raw if str(item).strip())
    exclude = tuple(str(item) for item in exclude_raw if str(item).strip())
    return FeatureSelectionConfig(include=include, exclude=exclude)


def resolve_active_features(
    feature_order: Sequence[str],
    config: Optional[FeatureSelectionConfig] = None,
) -> List[str]:
    cfg = config or FeatureSelectionConfig(include=(), exclude=())
    ordered = [str(name) for name in feature_order]
    ordered_set: Set[str] = set(ordered)

    include = [name for name in cfg.include if name in ordered_set]
    if include:
        return [name for name in ordered if name in set(include)]

    exclude_set = {name for name in cfg.exclude if name in ordered_set}
    return [name for name in ordered if name not in exclude_set]


def build_feature_read_plan(
    feature_order: Sequence[str],
    config_path: Optional[str] = None,
) -> FeatureReadPlan:
    config = load_feature_selection_config(config_path)
    ordered = tuple(str(name) for name in feature_order)
    active = tuple(resolve_active_features(ordered, config))
    active_set = set(active)
    inactive = tuple(name for name in ordered if name not in active_set)
    return FeatureReadPlan(
        feature_order=ordered,
        active_features=active,
        inactive_features=inactive,
        include=config.include,
        exclude=config.exclude,
    )


def _coerce_numeric_series(df: pd.DataFrame, column: str, dtype: np.dtype) -> pd.Series:
    if column not in df.columns:
        return pd.Series(np.zeros(len(df), dtype=dtype), index=df.index)

    return pd.to_numeric(df[column], errors="coerce").fillna(0.0).astype(dtype)


def build_feature_frame(
    df: pd.DataFrame,
    feature_order: Sequence[str],
    config_path: Optional[str] = None,
    dtype: np.dtype = DEFAULT_DTYPE,
) -> Tuple[pd.DataFrame, FeatureReadPlan]:
    plan = build_feature_read_plan(feature_order, config_path=config_path)
    active_set = set(plan.active_features)

    frame_dict: Dict[str, pd.Series] = {}
    for feature in plan.feature_order:
        if feature in active_set:
            frame_dict[feature] = _coerce_numeric_series(df, feature, dtype)
        else:
            frame_dict[feature] = pd.Series(np.zeros(len(df), dtype=dtype), index=df.index)

    feature_frame = pd.DataFrame(frame_dict, index=df.index)
    return feature_frame, plan


def build_feature_matrix(
    df: pd.DataFrame,
    feature_order: Sequence[str],
    config_path: Optional[str] = None,
    dtype: np.dtype = DEFAULT_DTYPE,
) -> Tuple[np.ndarray, FeatureReadPlan]:
    feature_frame, plan = build_feature_frame(
        df=df,
        feature_order=feature_order,
        config_path=config_path,
        dtype=dtype,
    )
    return feature_frame.to_numpy(dtype=dtype, copy=False), plan


def load_csv_feature_matrix(
    csv_path: str,
    feature_order: Sequence[str],
    config_path: Optional[str] = None,
    dtype: np.dtype = DEFAULT_DTYPE,
) -> Tuple[pd.DataFrame, np.ndarray, FeatureReadPlan]:
    df = pd.read_csv(csv_path)
    matrix, plan = build_feature_matrix(
        df=df,
        feature_order=feature_order,
        config_path=config_path,
        dtype=dtype,
    )
    return df, matrix, plan
