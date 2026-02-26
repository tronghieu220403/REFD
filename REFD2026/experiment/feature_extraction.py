from dataclasses import dataclass
from typing import List, Optional, Dict, Set
import re
from collections import Counter, defaultdict
import numpy as np


@dataclass
class Event:
    operation: str
    path: str
    time: float
    new_path: Optional[str] = None


class FileBehaviorFeatureExtractor:
    EPS = 1e-9
    TIME_BINS = 10
    _PATH_RULES = [
        ("temp/cache", re.compile(r"^c:\\users\\[^\\]+\\appdata\\local\\temp(\\|$)")),
        ("program", re.compile(r"^c:\\users\\[^\\]+\\appdata\\roaming(\\|$)")),
        ("program", re.compile(r"^c:\\users\\[^\\]+\\appdata\\locallow(\\|$)")),
        ("program", re.compile(r"^c:\\users\\[^\\]+\\appdata\\local(\\|$)")),
        ("program", re.compile(r"^c:\\program files \(x86\)(\\|$)")),
        ("program", re.compile(r"^c:\\program files(\\|$)")),
        ("program", re.compile(r"^c:\\programdata(\\|$)")),
        ("system", re.compile(r"^c:\\windows(\\|$)")),
        ("user", re.compile(r"^c:\\users\\public(\\|$)")),
        ("user", re.compile(r"^c:\\users\\[^\\]+(\\|$)")),
        ("user", re.compile(r"^\\\\(\\|$)")),
    ]

    _DOC_EXTS = {
        "doc", "docx", "xls", "xlsx", "ppt", "pptx", "pdf", "txt", "rtf", "odt", "ods", "odp", "csv"
    }
    _EXE_EXTS = {"exe", "dll", "sys", "scr", "com", "bat", "cmd", "msi"}
    _ARCHIVE_EXTS = {"zip", "rar", "7z", "tar", "gz", "bz2", "xz", "cab", "iso"}
    _MEDIA_EXTS = {"mp3", "wav", "flac", "aac", "mp4", "avi", "mkv", "mov", "wmv"}
    _IMAGE_EXTS = {"jpg", "jpeg", "png", "bmp", "gif", "tiff", "webp", "svg", "heic"}
    _CODE_EXTS = {
        "py", "pyw", "js", "ts", "java", "c", "cpp", "h", "hpp", "cs", "go", "rs", "php", "rb",
        "swift", "kt", "json", "xml", "yml", "yaml", "html", "css", "sql", "sh", "ps1", "md"
    }

    _FEATURE_NAMES = [
        "f_total_events",
        "f_create_count",
        "f_write_count",
        "f_delete_count",
        "f_rename_count",
        "f_total_event_rate",
        "f_write_ratio",
        "f_delete_ratio",
        "f_rename_ratio",
        "f_op_type_entropy",
        "f_interarrival_mean",
        "f_interarrival_cv",
        "f_interarrival_p90",
        "f_vmr_10bins",
        "f_max_bin_ratio",
        "f_burstiness_B",
        "f_inactivity_bin_fraction",
        "f_half_window_imbalance",
        "f_user_data_event_count",
        "f_user_data_write_count",
        "f_user_data_delete_count",
        "f_user_data_rename_count",
        "f_appdata_event_count",
        "f_temp_event_count",
        "f_system_event_count",
        "f_unique_root_count",
        "f_root_entropy",
        "f_doclike_write_count",
        "f_exelike_write_count",
        "f_write_ext_group_entropy",
        "f_unique_file_count",
        "f_unique_dir_count",
        "f_unique_ext_count",
        "f_events_per_file_mean",
        "f_file_event_gini",
        "f_dir_entropy",
        "f_ext_entropy",
        "f_path_depth_mean",
        "f_path_depth_std",
        "f_dominant_dir_ratio",
        "f_adjacent_same_dir_ratio",
        "f_adjacent_same_path_ratio",
        "f_transition_create_to_write",
        "f_transition_write_to_rename",
        "f_transition_write_to_delete",
        "f_longest_same_op_run",
        "f_rename_ext_change_ratio",
        "f_rename_dominant_new_ext_ratio",
        "f_rename_filename_prefix_similarity",
        "f_rename_same_dir_ratio",
        "f_create_filename_replication",
    ]

    def __init__(self, window_seconds: float):
        self.window_seconds = float(window_seconds)

    @classmethod
    def feature_names(cls) -> List[str]:
        return list(cls._FEATURE_NAMES)

    def extract(self, events: List[Event]) -> np.ndarray:
        prepared = []
        for e in events:
            op = self._normalize_operation(e.operation)
            p = self._normalize_path(e.path)
            p_new = self._normalize_path(e.new_path) if e.new_path is not None else ""
            prepared.append((float(e.time), op, p, p_new))

        prepared.sort(key=lambda x: x[0])

        times = [x[0] for x in prepared]
        ops = [x[1] for x in prepared]
        paths = [x[2] for x in prepared]
        new_paths = [x[3] for x in prepared]

        dirs = [self._dirname(p) for p in paths]
        exts = [self._extension(p) for p in paths]
        roots = [self._extract_root(p) for p in paths]
        depths = [self._depth(p) for p in paths]

        N = len(prepared)
        eps = self.EPS
        delta = self.window_seconds

        c_count = sum(1 for o in ops if o == "C")
        w_count = sum(1 for o in ops if o == "W")
        d_count = sum(1 for o in ops if o == "D")
        r_count = sum(1 for o in ops if o == "R")

        f_total_events = float(N)
        f_create_count = float(c_count)
        f_write_count = float(w_count)
        f_delete_count = float(d_count)
        f_rename_count = float(r_count)
        f_total_event_rate = N / (delta + eps)
        f_write_ratio = w_count / (N + eps)
        f_delete_ratio = d_count / (N + eps)
        f_rename_ratio = r_count / (N + eps)

        op_counts = [c_count, w_count, d_count, r_count]
        f_op_type_entropy = self._entropy_from_counts(op_counts, N)

        if N >= 2:
            tau = np.diff(np.asarray(times, dtype=np.float64))
            mu_tau = float(np.mean(tau))
            sigma_tau = float(np.std(tau))
            f_interarrival_mean = float(np.sum(tau) / max(N - 1, 1))
            f_interarrival_cv = sigma_tau / (mu_tau + eps)
            f_interarrival_p90 = float(np.percentile(tau, 90))
            f_burstiness_B = (sigma_tau - mu_tau) / (sigma_tau + mu_tau + eps)
        else:
            f_interarrival_mean = 0.0
            f_interarrival_cv = 0.0
            f_interarrival_p90 = 0.0
            f_burstiness_B = 0.0

        ts = times[0] if N > 0 else 0.0
        bins = [0] * self.TIME_BINS
        if N > 0:
            if delta > 0:
                for t in times:
                    rel = t - ts
                    idx = int((rel / delta) * self.TIME_BINS)
                    if idx < 0:
                        idx = 0
                    elif idx >= self.TIME_BINS:
                        idx = self.TIME_BINS - 1
                    bins[idx] += 1
            else:
                bins[0] = N

        bins_arr = np.asarray(bins, dtype=np.float64)
        bar_n = float(np.mean(bins_arr))
        s2_n = float(np.mean((bins_arr - bar_n) ** 2))
        f_vmr_10bins = s2_n / (bar_n + eps)
        f_max_bin_ratio = float(np.max(bins_arr)) / (N + eps)
        f_inactivity_bin_fraction = float(np.sum(bins_arr == 0)) / float(self.TIME_BINS)

        half_t = ts + (delta / 2.0)
        n1 = sum(1 for t in times if t < half_t)
        n2 = N - n1
        f_half_window_imbalance = abs(n1 - n2) / (N + eps)

        flags_user_data = [self._is_user_data(p) for p in paths]
        flags_appdata = [self._is_appdata(p) for p in paths]
        flags_temp = [self._is_temp(p) for p in paths]
        flags_system = [self._is_system(p) for p in paths]

        f_user_data_event_count = float(sum(flags_user_data))
        f_user_data_write_count = float(sum(1 for i in range(N) if ops[i] == "W" and flags_user_data[i]))
        f_user_data_delete_count = float(sum(1 for i in range(N) if ops[i] == "D" and flags_user_data[i]))
        f_user_data_rename_count = float(sum(1 for i in range(N) if ops[i] == "R" and flags_user_data[i]))
        f_appdata_event_count = float(sum(flags_appdata))
        f_temp_event_count = float(sum(flags_temp))
        f_system_event_count = float(sum(flags_system))

        root_counter = Counter(roots)
        f_unique_root_count = float(len(root_counter))
        f_root_entropy = self._entropy_from_counts(list(root_counter.values()), N)

        write_groups = [self._extension_group(exts[i]) for i in range(N) if ops[i] == "W"]
        f_doclike_write_count = float(sum(1 for g in write_groups if g == "doc"))
        f_exelike_write_count = float(sum(1 for g in write_groups if g == "exe"))
        group_order = ["doc", "exe", "archive", "media", "image", "code", "other"]
        write_group_counter = Counter(write_groups)
        f_wege_counts = [write_group_counter.get(g, 0) for g in group_order]
        f_write_ext_group_entropy = self._entropy_from_counts(f_wege_counts, w_count)

        file_counter = Counter(paths)
        dir_counter = Counter(dirs)
        ext_counter = Counter(exts)

        f_unique_file_count = float(len(file_counter))
        f_unique_dir_count = float(len(dir_counter))
        f_unique_ext_count = float(len(ext_counter))
        f_events_per_file_mean = N / (len(file_counter) + eps)
        f_file_event_gini = self._gini_from_counts(list(file_counter.values()))
        f_dir_entropy = self._entropy_from_counts(list(dir_counter.values()), N)
        f_ext_entropy = self._entropy_from_counts(list(ext_counter.values()), N)

        f_path_depth_mean = float(np.sum(np.asarray(depths, dtype=np.float64)) / (N + eps))
        if N > 0:
            depth_arr = np.asarray(depths, dtype=np.float64)
            f_path_depth_std = float(np.sqrt(np.sum((depth_arr - f_path_depth_mean) ** 2) / (N + eps)))
        else:
            f_path_depth_std = 0.0
        f_dominant_dir_ratio = (float(max(dir_counter.values())) / (N + eps)) if N > 0 else 0.0

        if N >= 2:
            same_dir_adj = sum(1 for i in range(N - 1) if dirs[i] == dirs[i + 1])
            same_path_adj = sum(1 for i in range(N - 1) if paths[i] == paths[i + 1])
            c_to_w = sum(1 for i in range(N - 1) if ops[i] == "C" and ops[i + 1] == "W")
            w_to_r = sum(1 for i in range(N - 1) if ops[i] == "W" and ops[i + 1] == "R")
            w_to_d = sum(1 for i in range(N - 1) if ops[i] == "W" and ops[i + 1] == "D")

            f_adjacent_same_dir_ratio = same_dir_adj / float(N - 1)
            f_adjacent_same_path_ratio = same_path_adj / float(N - 1)
            f_transition_create_to_write = c_to_w / (N - 1 + eps)
            f_transition_write_to_rename = w_to_r / (N - 1 + eps)
            f_transition_write_to_delete = w_to_d / (N - 1 + eps)
        else:
            f_adjacent_same_dir_ratio = 0.0
            f_adjacent_same_path_ratio = 0.0
            f_transition_create_to_write = 0.0
            f_transition_write_to_rename = 0.0
            f_transition_write_to_delete = 0.0

        f_longest_same_op_run = float(self._longest_run(ops))

        rename_indices = [i for i, o in enumerate(ops) if o == "R"]
        if r_count > 0:
            rename_old_exts = [exts[i] for i in rename_indices]
            rename_new_exts = [self._extension(new_paths[i]) for i in rename_indices]
            ext_changed = sum(1 for i in range(r_count) if rename_old_exts[i] != rename_new_exts[i])
            new_ext_counter = Counter(rename_new_exts)
            same_dir_rename = sum(
                1 for idx in rename_indices if self._dirname(paths[idx]) == self._dirname(new_paths[idx])
            )
            lcp_scores = []
            for idx in rename_indices:
                a = self._filename(paths[idx])
                b = self._filename(new_paths[idx])
                lcp_scores.append(self._lcp_len(a, b) / max(len(a), 1))

            f_rename_ext_change_ratio = ext_changed / (r_count + eps)
            f_rename_dominant_new_ext_ratio = (max(new_ext_counter.values()) if new_ext_counter else 0.0) / (r_count + eps)
            f_rename_filename_prefix_similarity = float(np.mean(np.asarray(lcp_scores, dtype=np.float64))) if lcp_scores else 0.0
            f_rename_same_dir_ratio = same_dir_rename / (r_count + eps)
        else:
            f_rename_ext_change_ratio = 0.0
            f_rename_dominant_new_ext_ratio = 0.0
            f_rename_filename_prefix_similarity = 0.0
            f_rename_same_dir_ratio = 0.0

        create_name_dirs: Dict[str, Set[str]] = defaultdict(set)
        for i in range(N):
            if ops[i] == "C":
                create_name_dirs[self._filename(paths[i])].add(dirs[i])
        f_create_filename_replication = float(max((len(v) for v in create_name_dirs.values()), default=0))

        feats = np.asarray([

            f_total_events, f_create_count, f_write_count, f_delete_count, f_rename_count, f_total_event_rate, f_write_ratio, f_delete_ratio, f_rename_ratio, f_op_type_entropy,
            
            f_interarrival_mean, f_interarrival_cv, f_interarrival_p90, f_vmr_10bins, f_max_bin_ratio, f_burstiness_B, f_inactivity_bin_fraction, f_half_window_imbalance,
            
            f_user_data_event_count, f_user_data_write_count, f_user_data_delete_count, f_user_data_rename_count, f_appdata_event_count, f_temp_event_count, f_system_event_count, f_unique_root_count, f_root_entropy, f_doclike_write_count, f_exelike_write_count, f_write_ext_group_entropy,
            
            f_unique_file_count, f_unique_dir_count, f_unique_ext_count, f_events_per_file_mean, f_file_event_gini, f_dir_entropy, f_ext_entropy, f_path_depth_mean, f_path_depth_std, f_dominant_dir_ratio,
            
            f_adjacent_same_dir_ratio, f_adjacent_same_path_ratio, f_transition_create_to_write, f_transition_write_to_rename, f_transition_write_to_delete, f_longest_same_op_run,
            
            f_rename_ext_change_ratio, f_rename_dominant_new_ext_ratio, f_rename_filename_prefix_similarity, f_rename_same_dir_ratio, f_create_filename_replication,

        ], dtype=np.float32)
        return feats

    @classmethod
    def _normalize_operation(cls, operation: str) -> str:
        op = (operation or "").strip().lower()
        if op in {"create", "c"}:
            return "C"
        if op in {"write", "modify", "w"}:
            return "W"
        if op in {"delete", "d"}:
            return "D"
        if op in {"rename", "r"}:
            return "R"
        return op.upper()[:1] if op else ""

    @classmethod
    def _normalize_path(cls, path: Optional[str]) -> str:
        if path is None:
            return ""
        p = str(path).strip().lower()
        p = p.replace("/", "\\")

        # ------------------------------
        # Convert \Device\HarddiskVolumeX to drive letter
        # ------------------------------
        m = re.match(r"^\\device\\harddiskvolume(\d+)(\\.*)?", p)
        if m:
            volume_index = int(m.group(1))
            suffix = m.group(2) or ""

            # A + (X - 1)
            drive_letter = chr(ord('a') + volume_index - 1)
            p = f"{drive_letter}:{suffix}"

        if p.startswith("\\\\?\\"):
            p = p[4:]
            if p.startswith("unc\\"):
                p = "\\\\" + p[4:]

        if p.startswith("\\\\"):
            prefix = "\\\\"
            rest = p[2:]
        else:
            prefix = ""
            rest = p

        rest = re.sub(r"\\+", r"\\", rest)
        return prefix + rest

    @classmethod
    def _extract_root(cls, path: str) -> str:
        if len(path) >= 2 and path[1] == ":" and path[0].isalpha():
            return path[:2]
        if path.startswith("\\\\"):
            parts = [seg for seg in path[2:].split("\\") if seg]
            if len(parts) >= 2:
                return "\\\\" + parts[0] + "\\" + parts[1]
            if len(parts) == 1:
                return "\\\\" + parts[0]
            return "\\\\"
        return ""

    @classmethod
    def _strip_trailing_separators(cls, path: str) -> str:
        p = path
        if not p:
            return ""
        while p.endswith("\\"):
            root = cls._extract_root(p)
            if p == root:
                break
            p = p[:-1]
            if not p:
                break
        return p

    @classmethod
    def _dirname(cls, path: str) -> str:
        p = cls._strip_trailing_separators(path)
        if not p:
            return ""
        root = cls._extract_root(p)
        if p == root:
            return root
        idx = p.rfind("\\")
        if idx < 0:
            return ""
        d = p[:idx]
        if not d and root:
            return root
        if root and len(d) < len(root):
            return root
        return d

    @classmethod
    def _filename(cls, path: str) -> str:
        p = cls._strip_trailing_separators(path)
        if not p:
            return ""
        root = cls._extract_root(p)
        if p == root:
            return ""
        idx = p.rfind("\\")
        if idx < 0:
            return p
        return p[idx + 1:]

    @classmethod
    def _extension(cls, path: str) -> str:
        name = cls._filename(path)
        if not name:
            return ""
        idx = name.rfind(".")
        if idx < 0 or idx == len(name) - 1:
            return ""
        return name[idx + 1:]

    @classmethod
    def _depth(cls, path: str) -> int:
        d = cls._dirname(path)
        root = cls._extract_root(d)
        if not d or d == root:
            return 0
        rel = d[len(root):] if root else d
        rel = rel.lstrip("\\")
        if not rel:
            return 0
        return len([seg for seg in rel.split("\\") if seg])

    @classmethod
    def _relative_after_root(cls, path: str) -> str:
        root = cls._extract_root(path)
        rel = path[len(root):] if root else path
        return rel.lstrip("\\")

    @classmethod
    def _path_segments(cls, path: str) -> List[str]:
        rel = cls._relative_after_root(path)
        if not rel:
            return []
        return [seg for seg in rel.split("\\") if seg]

    @classmethod
    def _is_user_data(cls, path: str) -> bool:
        return cls._classify_path(path) == "user"

    @classmethod
    def _is_appdata(cls, path: str) -> bool:
        return cls._classify_path(path) == "program"

    @classmethod
    def _is_temp(cls, path: str) -> bool:
        return cls._classify_path(path) == "temp/cache"

    @classmethod
    def _is_system(cls, path: str) -> bool:
        return cls._classify_path(path) == "system"

    @classmethod
    def _classify_path(cls, path: str) -> str:
        p = cls._normalize_path(path)
        if not p:
            return "user"

        for rule_type, pattern in cls._PATH_RULES:
            if pattern.match(p):
                return rule_type

        return "user"

    @classmethod
    def _extension_group(cls, ext: str) -> str:
        if ext in cls._DOC_EXTS:
            return "doc"
        if ext in cls._EXE_EXTS:
            return "exe"
        if ext in cls._ARCHIVE_EXTS:
            return "archive"
        if ext in cls._MEDIA_EXTS:
            return "media"
        if ext in cls._IMAGE_EXTS:
            return "image"
        if ext in cls._CODE_EXTS:
            return "code"
        return "other"

    @classmethod
    def _entropy_from_counts(cls, counts: List[int], denom: int) -> float:
        eps = cls.EPS
        val = 0.0
        for c in counts:
            p = c / (denom + eps)
            val -= p * float(np.log2(p + eps))
        return val

    @classmethod
    def _gini_from_counts(cls, counts: List[int]) -> float:
        eps = cls.EPS
        if not counts:
            return 0.0
        x = np.sort(np.asarray(counts, dtype=np.float64))
        k = x.size
        s = float(np.sum(x))
        if s <= 0:
            return 0.0
        idx = np.arange(1, k + 1, dtype=np.float64)
        pairwise_abs_sum = float(2.0 * np.sum((2.0 * idx - k - 1.0) * x))
        g = pairwise_abs_sum / (2.0 * k * s + eps)
        if g < 0:
            return 0.0
        if g > 1:
            return 1.0
        return float(g)

    @staticmethod
    def _lcp_len(a: str, b: str) -> int:
        n = min(len(a), len(b))
        i = 0
        while i < n and a[i] == b[i]:
            i += 1
        return i

    @staticmethod
    def _longest_run(ops: List[str]) -> int:
        if not ops:
            return 0
        best = 1
        cur = 1
        for i in range(1, len(ops)):
            if ops[i] == ops[i - 1]:
                cur += 1
                if cur > best:
                    best = cur
            else:
                cur = 1
        return best
