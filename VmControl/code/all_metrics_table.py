#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Merge of custom_report_shanon.py + custom_print_report.py
Generates LaTeX rows:
No. & Family & Modified & Unrecognized & Ratio & Null Ext & Ratio & Shannon & Ratio \\
"""

import os
import json
from pathlib import Path
from collections import defaultdict

# --------------------------------------------------------------------------- #
#                             BLACK / WHITE LISTS                             #
# --------------------------------------------------------------------------- #
black_list_name = [
    "whathappened", "read", "restore", "instruction", "decryp",
    "r_e_a_d", "how", "notification", "help", "criticalbreachdetected",
    "rgnr", "recov", "losttrustencoded",
]

white_list_name = [
    "bit_decrypt@", "supportdecrypt@firemail", "losttrustencoded", "readmanual", "help@"
]

def check_black_list(filename: str) -> bool:
    filename_low = filename.lower()
    is_blacklisted = False
    for match in black_list_name:
        if match in filename_low:
            is_blacklisted = True
            break
    if is_blacklisted:
        for match in white_list_name:
            if match in filename_low:
                is_blacklisted = False
                break
    return is_blacklisted

# --------------------------------------------------------------------------- #
#                             ENTROPY (SHANNON) PART                          #
# --------------------------------------------------------------------------- #
def analyze_shannon_stats(input_file: str,
                          root_dir: str,
                          threshold: float = 7.95) -> dict[str, dict[str, int]]:
    """
    Đọc file <path, entropy> ghép cặp 2-dòng, trả về:
        {family: {'total': int, 'high': int}}
    """
    stats: dict[str, dict[str, int]] = {}
    with open(input_file, "r", encoding="utf-8") as f:
        lines = f.readlines()

    checked_files = set()

    for i in range(0, len(lines), 2):
        full_path = lines[i].strip()
        entropy   = float(lines[i + 1].strip())
        
        if full_path in checked_files:
            continue
        checked_files.add(full_path)

        if check_black_list(full_path.split("\\")[-1].lower()):
            continue

        family = os.path.relpath(full_path, root_dir).split(os.sep)[0]

        if family not in stats:
            stats[family] = {'total': 0, 'high': 0}

        stats[family]['total'] += 1
        if entropy >= threshold:
            stats[family]['high'] += 1
    return stats

# --------------------------------------------------------------------------- #
#                          ENTROPY (CHI-SQUARE) PART                          #
# --------------------------------------------------------------------------- #
def analyze_chi2_stats(input_file: str,
                          root_dir: str,
                          threshold: float = 310) -> dict[str, dict[str, int]]:
    """
    Đọc file <path, entropy> ghép cặp 2-dòng, trả về:
        {family: {'total': int, 'high': int}}
    """
    stats: dict[str, dict[str, int]] = {}
    with open(input_file, "r", encoding="utf-8") as f:
        lines = f.readlines()

    checked_files = set()

    for i in range(0, len(lines), 2):
        full_path = lines[i].strip()
        entropy   = float(lines[i + 1].strip())

        if full_path in checked_files:
            continue
        checked_files.add(full_path)        

        if check_black_list(full_path.split("\\")[-1].lower()):
            continue

        family = os.path.relpath(full_path, root_dir).split(os.sep)[0]

        if family not in stats:
            stats[family] = {'total': 0, 'high': 0}

        stats[family]['total'] += 1
        if entropy <= threshold:
            stats[family]['high'] += 1
    return stats

# --------------------------------------------------------------------------- #
#                          UTILITIES – ORIGINAL FUNCTIONS                     #
# --------------------------------------------------------------------------- #
def get_parent_folder_name(file_path: str) -> str:
    return Path(file_path).parent.name

def get_all_filenames_without_extension(root_dir: str) -> set[str]:
    filename_set = set()
    for root, _, files in os.walk(root_dir):
        for file in files:
            filename_set.add(file.split(".")[0].lower())
    return filename_set


# --------------------------------------------------------------------------- #
#                 BUILD accepted_types  FROM napier_dataset.txt               #
# --------------------------------------------------------------------------- #
accepted_types_in_dir: dict[str, set[str]] = defaultdict(set)
accepted_types: dict[str, set[str]] = defaultdict(set)

with open("napier_dataset_custom_trid.txt", 'r', encoding='utf-8') as f:
    lines = [line.strip().lower() for line in f]

for i in range(0, len(lines), 2):
    if i+1 >= len(lines):
        break
    
    filename = os.path.basename(lines[i])
    parent_dir_name = get_parent_folder_name(lines[i])
    ori_type = filename.split(".")[-1].lower()
    content = lines[i+1].lower()

    if len(content) == 0:
        #black_list_name.append(filename)
        continue
    if content[-1] == ";":
        content = content[:-1]
    l = content.split(";")
    accepted_types[ori_type].update(l)

# --------------------------------------------------------------------------- #
#                        PROCESS ONE FAMILY REPORT FILE                       #
# --------------------------------------------------------------------------- #
def process_file(filepath: str, file_map: dict) -> tuple[int, int, int]:
    """
    Trả về (total, matches, no_ext)
    matches  = có giao cắt accepted_types
    no_ext   = content rỗng hoặc không có type
    """
    with open(filepath, "r", encoding="utf-8") as f:
        lines = [line.strip().lower() for line in f]

    matches = total = no_ext = 0

    for i in range(0, len(lines), 2):
        if i + 1 >= len(lines):
            break

        if "0041-css.css" in lines[i]:
            pass

        filename = os.path.basename(lines[i])
        if check_black_list(filename):
            continue

        if "0060-css.css" in filename:
            pass
        content = lines[i+1]
        if ".c" in content:
            pass
        total += 1
        if len(content) > 0:
            if content[-1] == ";":
                content = content[:-1]

            try:
                file_ext = filename.split(".")[1].lower()
            except Exception:
                file_ext = None
                
            l = content.split(";")
            ext_list = set(l)
            if len(ext_list) == 0:
                no_ext += 1
            if file_ext == None:
                matches += 1
            elif file_ext not in accepted_types:
                matches += 1
            elif not accepted_types[file_ext].isdisjoint(ext_list):
                matches += 1
        else:
            no_ext += 1
    return total, matches, no_ext

if __name__ == "__main__":

    from operator import itemgetter
    with open(r"E:\hieunt20210330\File-Map\file_map.json", "r", encoding="utf-8") as f:
        file_map = json.load(f)
    
    victim_shannon_stats = analyze_shannon_stats(
        input_file="nap_victim_shannon_cal.txt",
        root_dir="E:\\Graduation_Project\\napier_dataset_victim",
        threshold=7.95,
    )

    victim_chi2_stats = analyze_chi2_stats(
        input_file="nap_victim_chi2_cal.txt",
        root_dir="E:\\Graduation_Project\\napier_dataset_victim",
        threshold=311,
    )

    v_total, v_matches, v_null_ext = process_file("napier_dataset_victim_trid.txt", file_map)

    v_high_shannon = sum([v["high"] for v in victim_shannon_stats.values()])
    v_low_chi2 = sum([v["high"] for v in victim_chi2_stats.values()])
    print(v_total, v_matches, v_null_ext)
    
    custom_shannon_stats = analyze_shannon_stats(
        input_file="nap_custom_shannon_cal.txt",
        root_dir="E:\\Graduation_Project\\napier_dataset",
        threshold=7.95,
    )

    custom_chi2_stats = analyze_chi2_stats(
        input_file="nap_custom_chi2_cal.txt",
        root_dir="E:\\Graduation_Project\\napier_dataset",
        threshold=311,
    )
    
    c_total, c_matches, c_null_ext = process_file("napier_dataset_custom_trid.txt", file_map)

    c_high_shannon = sum([v["high"] for v in custom_shannon_stats.values()])
    c_low_chi2 = sum([v["high"] for v in custom_chi2_stats.values()])

    total_negative = c_total + v_total
    trid_matches = c_matches + v_matches
    trid_null_ext = c_null_ext + v_null_ext
    shannon_high = c_high_shannon + v_high_shannon
    chi2_low = c_low_chi2 + v_low_chi2
    trid_fpr = trid_null_ext / total_negative
    shannon_fpr = shannon_high / total_negative
    chi2_fpr = chi2_low / total_negative
    print(f"Total: {total_negative}, Matches: {trid_matches}, Null Ext: {trid_null_ext}")
    print(f"Shannon High: {shannon_high}, Chi2 Low: {chi2_low}")
    trid_tnr = 1 - trid_fpr
    shannon_tnr = 1 - shannon_fpr
    chi2_tnr = 1 - chi2_fpr
    print(f"Trid TNR: {trid_tnr*100:.2f}%\nShannon TNR: {shannon_tnr*100:.2f}%\nChi2 TNR: {chi2_tnr*100:.2f}%")
    print(f"Trid FPR: {trid_fpr*100:.2f}%\nShannon FPR: {shannon_fpr*100:.2f}%\nChi2 FPR: {chi2_fpr*100:.2f}%")

    trid_tpr = 90.68 / 100
    shannon_tpr = 85.37 / 100
    chi2_tpr = 69.62 / 100
    trid_fnr = 1 - trid_tpr
    shannon_fnr = 1 - shannon_tpr
    chi2_fnr = 1 - chi2_tpr
    print(f"Trid TPR: {trid_tpr*100:.2f}%\nShannon TPR: {shannon_tpr*100:.2f}%\nChi2 TPR: {chi2_tpr*100:.2f}%")
    print(f"Trid FNR: {trid_fnr*100:.2f}%\nShannon FNR: {shannon_fnr*100:.2f}%\nChi2 FNR: {chi2_fnr*100:.2f}%")

    # ----- Generate LaTeX Table (f-string, reordered columns) -------------------- #
    latex_table = f"""
    \\begin{{table}}[htbp]
    \\centering
    \\caption{{Detection Performance Metrics for TrID, Shannon Entropy, and Chi-Square}}
    \\begin{{tabular}}{{|l|c|c|c|c|}}
    \\toprule
    \\textbf{{Test method}} & \\textbf{{TPR (\\%)}} & \\textbf{{TNR (\\%)}} & \\textbf{{FNR (\\%)}} & \\textbf{{FPR (\\%)}} \\\\
    \\midrule
    Null ext   & {trid_tpr * 100:.2f} & {trid_tnr * 100:.2f} & {trid_fnr * 100:.2f} & {trid_fpr * 100:.2f} \\\\
    Shannon    & {shannon_tpr * 100:.2f} & {shannon_tnr * 100:.2f} & {shannon_fnr * 100:.2f} & {shannon_fpr * 100:.2f} \\\\
    Chi-Square & {chi2_tpr * 100:.2f} & {chi2_tnr * 100:.2f} & {chi2_fnr * 100:.2f} & {chi2_fpr * 100:.2f} \\\\
    \\bottomrule
    \\end{{tabular}}
    \\end{{table}}
    """

    with open("detection_metrics_table.tex", "w", encoding="utf-8") as f:
        f.write(latex_table)

    print("\nLaTeX table written to 'detection_metrics_table.tex'")
