import os
import re

def extract_incompatible_malware(log_dir, prefix="runtime_log_", count=10):
    malware_set = set()
    pattern = re.compile(r"Malware is not compatible:\s*(.+)")

    for i in range(count):
        file_name = f"{prefix}{i}.txt"
        file_path = os.path.join(log_dir, file_name)

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                for line in f:
                    match = pattern.search(line)
                    if match:
                        malware_name = match.group(1).strip()
                        malware_set.add(malware_name)
        except Exception as e:
            print(f"Lỗi khi đọc file {file_path}: {e}")

    return malware_set

# Ví dụ sử dụng
log_directory = ""
malware_names = extract_incompatible_malware(log_directory)

print(f"Tổng số malware không tương thích: {len(malware_names)}")
print("Danh sách malware:")
for name in sorted(malware_names):
    print(f"- {name}")
