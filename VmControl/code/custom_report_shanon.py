import os

black_list_name = [
    "whathappened", "read", "restore", "instruction", "decryp",
    "r_e_a_d", "how", "notification", "help", "criticalbreachdetected",
    "rgnr", "recov", "losttrustencoded",
]

white_list_name = [
    "bit_decrypt@", "supportdecrypt@firemail", "losttrustencoded", "readmanual",
]

def check_black_list(filename: str) -> bool:
    is_blacklisted = False
    for match in black_list_name:
        if match in filename:
            is_blacklisted = True
            break
    if is_blacklisted:
        for match in white_list_name:
            if match in filename:
                is_blacklisted = False
                break
    return is_blacklisted

def analyze_entropy_stats(output_file: str, root_dir: str, threshold: float = 7.95):
    ransomware_stats = {}

    with open(output_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Duyệt từng cặp (path, entropy)
    for i in range(0, len(lines), 2):
        full_path = lines[i].strip()
        entropy = float(lines[i + 1].strip())

        if check_black_list(full_path):
            continue

        # Bỏ phần root_dir để lấy phần còn lại
        relative_path = os.path.relpath(full_path, root_dir)

        # Tên ransomware = folder cha đầu tiên (thường là như vậy)
        ransomware_name = relative_path.split(os.sep)[0]

        if ransomware_name not in ransomware_stats:
            ransomware_stats[ransomware_name] = {'total': 0, 'high_entropy': 0}

        ransomware_stats[ransomware_name]['total'] += 1
        if entropy >= threshold:
            ransomware_stats[ransomware_name]['high_entropy'] += 1

    # In thống kê
    import sys
    sys.stdout = open("custom_shannon_report.txt", "w", encoding='utf-8')
    for name, stats in ransomware_stats.items():
        print(f"{name}: {stats['high_entropy']}/{stats['total']} ({stats['high_entropy'] * 100 /stats['total']:.2f}%) file có entropy > {threshold}")

# Ví dụ chạy
analyze_entropy_stats(
    output_file="custom_shanon_cal.txt",
    root_dir='E:\\Graduation_Project\\custom_dataset\\done_real',
    threshold=7.95
)
