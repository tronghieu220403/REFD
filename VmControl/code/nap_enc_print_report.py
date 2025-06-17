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
#                 BUILD accepted_types  FROM napier_dataset_custom_trid.txt   #
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
        black_list_name.append(filename)
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

        filename = os.path.basename(lines[i])
        if check_black_list(filename):
            continue

        content = lines[i+1].lower()
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

    # -------- entropy stats (Shannon > threshold) -------------------------- #
    shannon_stats = analyze_shannon_stats(
        input_file="nap_enc_shanon_cal.txt",
        root_dir="E:\\Graduation_Project\\napier_dataset_encrypted",
        threshold=7.95,
    )

    chi2_stats = analyze_chi2_stats(
        input_file="nap_enc_chi_square_cal.txt",
        root_dir="E:\\Graduation_Project\\napier_dataset_encrypted",
        threshold=311,
    )

    # -------- file_map ----------------------------------------------------- #
    with open(r"E:\hieunt20210330\File-Map\file_map.json", "r", encoding="utf-8") as f:
        file_map = json.load(f)

    # -------- iterate every family report ---------------------------------- #
    input_folder = r"E:\Graduation_Project\custom_dataset\code\report_napier_encrypted"
    output_file = "nap_enc_dataset_latex_table.txt"

    # -------- variables for averaging -------------------------------------- #
    total_sum = 0
    unrecog_sum = 0
    null_ext_sum = 0
    sh_high_sum = 0
    chi2_high_sum = 0
    family_count = 0

    with open(output_file, "w", encoding="utf-8") as out:
        out.write("% \\textbf{No.} & \\textbf{Family} & \\textbf{Modified} & "
                  "\\textbf{Unrecognized} & \\textbf{Ratio} & "
                  "\\textbf{Null Ext} & \\textbf{Ratio} & "
                  "\\textbf{Shannon} & \\textbf{Ratio} & "
                  "\\textbf{Chi2} & \\textbf{Ratio} \\\\\n")

        for idx, fname in enumerate(sorted(os.listdir(input_folder)), 1):
            if "erber" not in fname.lower():
                pass
                #continue

            family = fname[:-4]
            fpath = os.path.join(input_folder, fname)
            total, matches, null_ext = process_file(fpath, file_map)
            unrecog = total - matches

            ratio_unrecog = unrecog * 100 / total if total else 0.0
            ratio_null = null_ext * 100 / total if total else 0.0

            sh_high = sh_ratio = 0
            shannon_fam_stat = shannon_stats.get(family)
            if shannon_fam_stat:
                sh_high = shannon_fam_stat["high"]
                sh_ratio = sh_high * 100 / shannon_fam_stat["total"] if shannon_fam_stat["total"] else 0

            chi2_high = chi2_ratio = 0
            chi2_fam_stat = chi2_stats.get(family)
            if chi2_fam_stat:
                chi2_high = chi2_fam_stat["high"]
                chi2_ratio = chi2_high * 100 / chi2_fam_stat["total"] if chi2_fam_stat["total"] else 0

            # ----- update sums --------------------------------------------- #
            total_sum += total
            unrecog_sum += unrecog
            null_ext_sum += null_ext
            sh_high_sum += sh_high
            chi2_high_sum += chi2_high
            family_count += 1

            # ----- highlight top 2 ratios ---------------------------------- #
            ratios = {
                "unrecog": ratio_unrecog,
                "null": ratio_null,
                "shannon": sh_ratio,
                "chi2": chi2_ratio,
            }
            top2_keys = [k for k, v in sorted(ratios.items(), key=itemgetter(1), reverse=True)[:2]]

            def format_ratio(name, value):
                if name in top2_keys:
                    return f"\\cellcolor{{cyan!20}} {value:.2f}\\%"
                else:
                    return f"{value:.2f}\\%"

            # ----- write LaTeX row ------------------------------------------ #
            out.write(f"{idx} & {family} & "
                      f"{total} & {unrecog} & {format_ratio('unrecog', ratio_unrecog)} & "
                      f"{null_ext} & {format_ratio('null', ratio_null)} & "
                      f"{sh_high} & {format_ratio('shannon', sh_ratio)} & "
                      f"{chi2_high} & {format_ratio('chi2', chi2_ratio)} \\\\\n")

        # ----- write average row ------------------------------------------- #
        if family_count:
            avg_total = total_sum / family_count
            avg_unrecog = unrecog_sum / family_count
            avg_null_ext = null_ext_sum / family_count
            avg_sh_high = sh_high_sum / family_count
            avg_chi2_high = chi2_high_sum / family_count

            # ----- compute ratio from average counts ------------------------ #
            avg_ratio_unrecog = avg_unrecog * 100 / avg_total if avg_total else 0
            avg_ratio_null    = avg_null_ext * 100 / avg_total if avg_total else 0
            avg_ratio_shannon = avg_sh_high * 100 / avg_total if avg_total else 0
            avg_ratio_chi2    = avg_chi2_high * 100 / avg_total if avg_total else 0

            # ----- find top 2 ratios ---------------------------------------- #
            avg_ratios = {
                "unrecog": avg_ratio_unrecog,
                "null": avg_ratio_null,
                "shannon": avg_ratio_shannon,
                "chi2": avg_ratio_chi2,
            }
            top2_avg_keys = sorted(avg_ratios.items(), key=lambda x: x[1], reverse=True)[:2]
            top2_avg_names = {k for k, _ in top2_avg_keys}

            def format_avg_ratio(name, value):
                if name in top2_avg_names:
                    return f"\\cellcolor{{cyan!30}} {value:.2f}\\%"
                else:
                    return f"{value:.2f}\\%"

            out.write("\\midrule\n")
            out.write(f"\\multicolumn{{2}}{{l}}{{\\textbf{{Average}}}} & "
                    f"{avg_total:.1f} & {avg_unrecog:.1f} & {format_avg_ratio('unrecog', avg_ratio_unrecog)} & "
                    f"{avg_null_ext:.1f} & {format_avg_ratio('null', avg_ratio_null)} & "
                    f"{avg_sh_high:.1f} & {format_avg_ratio('shannon', avg_ratio_shannon)} & "
                    f"{avg_chi2_high:.1f} & {format_avg_ratio('chi2', avg_ratio_chi2)} \\\\\n")

            out.write("\n\n")
            out.write(f"Total families: {family_count}\n")
            out.write(f"Total files: {total_sum}\n")
            out.write(f"Total unrecognized: {unrecog_sum}\n")
            out.write(f"Total null ext: {null_ext_sum}\n")
            out.write(f"Total shannon high: {sh_high_sum}\n")
            out.write(f"Total chi2 low: {chi2_high_sum}\n")
            out.write("\n\n")


    print(f"[+] Done LaTeX rows saved to: {output_file}")
