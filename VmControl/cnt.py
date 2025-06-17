import os
import shutil

def count_ransomware_files(source_dir):
    total = 0
    count = 0

    for root, _, files in os.walk(source_dir):
        for name in files:
            total += 1
            file_path = os.path.join(root, name)
            try:
                with open(file_path, 'r', encoding='utf-16-le') as f:
                    content = f.read()
                    if "is ransomware" in content:
                        count += 1
                        #print(name)
                        pass                    

            except Exception as e:
                print(f"Lỗi khi đọc file {file_path}: {e}")
    return count, total

# Ví dụ sử dụng
source_folder = "E:\\Graduation_Project\\clean_combined_log\\"
while True:
    result, total = count_ransomware_files(source_folder)
    print(f"Tổng số file chứa 'is ransomware': {result}")
    print(f"Tổng số file đã duyệt: {total}")
    print(f"Tỉ lệ: {result / total * 100:.2f}%, Không chứa: {total - result}")
    import time
    time.sleep(60)
