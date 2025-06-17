#!/usr/bin/env python3
"""
ggez1.py  –  Merge LaTeX tables from custom_dataset_latex_table.txt
            and nap_enc_dataset_latex_table.txt

•  Cộng gộp các trường số liệu của những ransomware family trùng tên
•  Tính lại 4 tỉ lệ: Unrecognized, Null-Ext, Shannon-High, Chi2-High
•  Tô màu cyan!20 cho hai tỉ lệ cao nhất của từng hàng,
   cyan!30 cho hai tỉ lệ cao nhất của dòng Average
•  Xuất ra combined_dataset_latex_table.txt
"""

import re
import os
from operator import itemgetter
from collections import defaultdict

# ---------- helpers --------------------------------------------------------- #
_number_re = re.compile(r"[-+]?\d+(?:\.\d+)?")

def num(token: str) -> float:
    """Trích số đầu tiên tìm thấy trong một ô LaTeX (dạng int hay float)."""
    m = _number_re.search(token)
    return float(m.group()) if m else 0.0

def parse_table(path: str) -> dict[str, dict[str, float]]:
    """
    Đọc 1 file LaTeX, trả về dict:
        { family: {total, unrecog, null_ext, sh_high, chi2_high} }
    """
    stats: dict[str, dict[str, float]] = defaultdict(lambda: {
        "total": 0, "unrecog": 0, "null_ext": 0, "sh_high": 0, "chi2_high": 0
    })

    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if (not line                                    # dòng trống
                or line.startswith("%")                      # tiêu đề
                or "\\midrule" in line                       # đường phân cách
                or "\\multicolumn" in line):                 # dòng Average cũ
                continue

            cell = [c.strip() for c in line.split("&")]
            if len(cell) < 10:
                continue                                     # không đủ cột

            fam = cell[1]
            stats[fam]["total"]     += num(cell[2])
            stats[fam]["unrecog"]   += num(cell[3])
            stats[fam]["null_ext"]  += num(cell[5])
            stats[fam]["sh_high"]   += num(cell[7])
            stats[fam]["chi2_high"] += num(cell[9])

    return stats

def format_ratio(name: str, value: float, top2: set[str]) -> str:
    clamped_value = max(0, min(value/100, 1))
    percent_value = clamped_value * 100  # giá trị từ 0–100

    # Xác định màu và cường độ dựa trên mức value
    if percent_value < 20:
        base_color = "red"
        intensity = 50 - int(percent_value)  # red!50 → red!30
    elif percent_value < 40:
        base_color = "orange"
        intensity = 50 - int((percent_value - 20))
    elif percent_value < 60:
        base_color = "yellow"
        intensity = 45 - int((percent_value - 40))
    elif percent_value < 80:
        base_color = "cyan"
        intensity = 20 + int((percent_value - 60)) * 2
    else:
        base_color = "green"
        intensity = 10 + int((percent_value - 80)) * 2.7

    intensity = int(max(20, min(100, intensity)))

    return f"\\cellcolor{{{base_color}!{intensity}}} {value:.2f}\\%"

# ---------- main ------------------------------------------------------------ #
def main():
    INPUTS  = ["custom_dataset_latex_table.txt",
               "nap_enc_dataset_latex_table.txt"]
    OUTPUT  = "combined_dataset_latex_table.txt"

    # gộp số liệu từ hai bảng
    merged: dict[str, dict[str, float]] = defaultdict(lambda: {
        "total": 0, "unrecog": 0, "null_ext": 0, "sh_high": 0, "chi2_high": 0
    })
    for path in INPUTS:
        if not os.path.isfile(path):
            raise FileNotFoundError(f"Không tìm thấy {path}")
        for fam, stat in parse_table(path).items():
            for k, v in stat.items():
                merged[fam][k] += v

    # ---------- tính & ghi bảng mới ---------------------------------------- #
    with open(OUTPUT, "w", encoding="utf-8") as out:
        out.write("\\textbf{No.} & \\textbf{Family} & \\textbf{Modified} & "
                  "\\textbf{Unrecognized} & \\textbf{Ratio} & "
                  "\\textbf{Null Ext} & \\textbf{Ratio} & "
                  "\\textbf{Shannon} & \\textbf{Ratio} & "
                  "\\textbf{Chi2} & \\textbf{Ratio} \\\\\n")

        # biến tích luỹ để tính Average
        total_sum = unrecog_sum = null_ext_sum = sh_high_sum = chi2_high_sum = 0
        fam_count = 0

        for idx, (family, s) in enumerate(sorted(merged.items()), 1):
            total       = s["total"]
            unrecog     = s["unrecog"]
            null_ext    = s["null_ext"]
            sh_high     = s["sh_high"]
            chi2_high   = s["chi2_high"]

            # ---- ratios --------------------------------------------------- #
            r_unrecog = (unrecog  * 100 / total) if total else 0.0
            r_null    = (null_ext * 100 / total) if total else 0.0
            r_shannon = (sh_high  * 100 / total) if total else 0.0
            r_chi2    = (chi2_high* 100 / total) if total else 0.0

            ratios = {
                "unrecog": r_unrecog,
                "null":    r_null,
                "shannon": r_shannon,
                "chi2":    r_chi2,
            }
            top2 = {k for k, _ in sorted(ratios.items(),
                                         key=itemgetter(1), reverse=True)[:2]}

            # ---- ghi dòng -------------------------------------------------- #
            out.write(f"{idx} & {family} & "
                      f"{int(total)} & {int(unrecog)} & {format_ratio('unrecog', r_unrecog, top2)} & "
                      f"{int(null_ext)} & {format_ratio('null', r_null, top2)} & "
                      f"{int(sh_high)} & {format_ratio('shannon', r_shannon, top2)} & "
                      f"{int(chi2_high)} & {format_ratio('chi2', r_chi2, top2)} \\\\\n")

            # ---- cộng dồn -------------------------------------------------- #
            total_sum   += total
            unrecog_sum += unrecog
            null_ext_sum+= null_ext
            sh_high_sum += sh_high
            chi2_high_sum += chi2_high
            fam_count   += 1

        # ---------- dòng Average ------------------------------------------ #
        if fam_count:
            avg_total   = total_sum   / fam_count
            avg_unrecog = unrecog_sum / fam_count
            avg_null    = null_ext_sum/ fam_count
            avg_sh      = sh_high_sum / fam_count
            avg_chi2    = chi2_high_sum / fam_count

            avg_ratios = {
                "unrecog": avg_unrecog * 100 / avg_total if avg_total else 0,
                "null":    avg_null    * 100 / avg_total if avg_total else 0,
                "shannon": avg_sh      * 100 / avg_total if avg_total else 0,
                "chi2":    avg_chi2    * 100 / avg_total if avg_total else 0,
            }
            top2_avg = {k for k, _ in sorted(avg_ratios.items(),
                                             key=itemgetter(1), reverse=True)[:2]}

            out.write("\\midrule\n")
            out.write("\\multicolumn{2}{l}{\\textbf{Average}} & "
                      f"{avg_total:.1f} & {avg_unrecog:.1f} & {format_ratio('unrecog', avg_ratios['unrecog'], top2_avg)} & "
                      f"{avg_null:.1f} & {format_ratio('null', avg_ratios['null'], top2_avg)} & "
                      f"{avg_sh:.1f} & {format_ratio('shannon', avg_ratios['shannon'], top2_avg)} & "
                      f"{avg_chi2:.1f} & {format_ratio('chi2', avg_ratios['chi2'], top2_avg)} \\\\\n")
            
            out.write("\n\n")
            out.write(f"Total families: {fam_count}\n")
            out.write(f"Total files: {total_sum}\n")
            out.write(f"Total unrecognized: {unrecog_sum}\n")
            out.write(f"Total null ext: {null_ext_sum}\n")
            out.write(f"Total shannon high: {sh_high_sum}\n")
            out.write(f"Total chi2 low: {chi2_high_sum}\n")
            out.write("\n\n")

    print(f"[+] Combined LaTeX saved to: {OUTPUT}")

# --------------------------------------------------------------------------- #
if __name__ == "__main__":
    main()
