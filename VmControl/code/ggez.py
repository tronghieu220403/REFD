#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ggez.py

Đọc các file TRiD reports từ các thư mục hoặc file đã định nghĩa sẵn và tạo cấu trúc:
    accepted_types: dict[str, set[str]]
với mỗi extension gốc, chỉ giữ những TRiD-type xuất hiện ≥ MIN_COUNT lần.
"""
import os
import json
import sys
from collections import defaultdict, Counter

# Ngưỡng số lần xuất hiện để chấp nhận một loại TRiD
MIN_COUNT = 500

# Đường dẫn thư mục hoặc file TRiD reports (giữ nguyên như cấu trúc cũ)
INPUT_PATHS = [
    r"E:\Graduation_Project\custom_dataset\code\report_napier_clean\trid",
    r"E:\Graduation_Project\custom_dataset\code\report_napier_clean_crop\trid",
    r'napier_dataset_custom_trid.txt',
    r'napier_dataset_victim_trid.txt',
    # r"E:\path\to\single_report.trid",
]

# ---------------------- BUILD accepted_types -------------------------------

def build_accepted_types(input_paths, min_count=MIN_COUNT):
    """
    Trả về accepted_types: dict[ori_ext:str, set[trid_type:str]]
    Chỉ giữ những trid_type có count >= min_count.
    Input có thể là file hoặc thư mục.
    """
    counter_by_ext: dict[str, Counter] = defaultdict(Counter)

    def process_file(fpath: str):
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                lines = [line.strip().lower() for line in f if line.strip()]
        except Exception as e:
            print(f"[Error] Không thể đọc file {fpath}: {e}", file=sys.stderr)
            return
        for i in range(0, len(lines), 2):
            if i + 1 >= len(lines):
                break
            full_path = lines[i]
            trid_content = lines[i+1]
            base = os.path.basename(full_path)
            ori_ext = base.rsplit('.', 1)[1] if '.' in base else ''
            if trid_content.endswith(';'):
                trid_content = trid_content[:-1]
            for t in trid_content.split(';'):
                t = t.strip()
                if not t:
                    continue
                counter_by_ext[ori_ext][t] += 1

    for path in input_paths:
        if os.path.isdir(path):
            for fname in os.listdir(path):
                fpath = os.path.join(path, fname)
                if os.path.isfile(fpath):
                    process_file(fpath)
        elif os.path.isfile(path):
            process_file(path)
        else:
            print(f"[Warning] Đường dẫn không tồn tại: {path}", file=sys.stderr)

    accepted_types: dict[str, list[str]] = defaultdict(list[str])
    for ext, ctr in counter_by_ext.items():
        for t, cnt in ctr.items():
            if cnt >= min_count:
                if t not in accepted_types[ext]:
                    accepted_types[ext].append(t)
    return accepted_types

# --------------------------------------------------------------------------- #

if __name__ == "__main__":
    # Sử dụng INPUT_PATHS đã định nghĩa sẵn
    accepted_types = build_accepted_types(INPUT_PATHS)
    
    # Xuất kết quả ra JSON
    with open("accepted_types.json", "w", encoding="utf-8") as f:
        json.dump(accepted_types, f, indent=2, ensure_ascii=False)

    # In bảng LaTeX
    with open("accepted_types_table.tex", "w", encoding="utf-8") as tex:
        tex.write("\\begin{tabular}{|l|p{10cm}|}\n")
        tex.write("\\toprule\n")
        tex.write("\\textbf{Extension} & \\textbf{Accepted TRiD Types} \\\\\n")
        tex.write("\\midrule\n")
        for ext, types in sorted(accepted_types.items()):
            ext_display = ext if ext else "(none)"
            joined = "; ".join([ft.upper() for ft in sorted(types)])
            count = len(types)
            tex.write(f"{ext_display.upper()} & {joined} \\\\\n")
            tex.write("\\hline\n")
        tex.write("\\bottomrule\n")
        tex.write("\\end{tabular}\n")
