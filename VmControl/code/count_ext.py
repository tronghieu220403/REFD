import os
import json
from collections import defaultdict

def get_all_filenames_without_extension(root_dir: str) -> set:
    filename_set = set()
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            filename_without_ext = file.split(".")[0].lower()
            filename_set.add(filename_without_ext)
    return filename_set

# Ví dụ sử dụng
original_napier_directory = "E:\\hieunt20210330\\NapierOne-tiny"
filenames = get_all_filenames_without_extension(original_napier_directory)

def extract_type(filename: str):
    try:
        x = (filename.split("-")[0]).split("\\")[1]
        return x
    except IndexError:
        return None

ext_cnt = defaultdict(int)

# Hàm xử lý từng file
def process_file(filepath, file_map, out_file):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = [line.strip().lower() for line in f]
    
    ext_list = set()

    for i in range(0, len(lines), 2):
        if i+1 >= len(lines):
            break
        filename = lines[i]
        content = lines[i+1]
        
        x = extract_type(filename)
        if x == None:
            continue
        if len(content) == 0:
            ext_cnt[x] += 1
        else:
            if not x:
                continue
            if x == extract_type(filename):
                ext_list.add(x)

            match_list = [x]#[match.lower() for match in (file_map.get(x, []) + [x])]
            if any(match in content for match in match_list):
                ext_cnt[x] += 1

# Đường dẫn thư mục chứa các file txt cần xử lý
input_folder = "E:\\Graduation_Project\\custom_dataset\\code\\report\\" 

# Đọc file_map.json
with open('E:\\hieunt20210330\\File-Map\\file_map.json', 'r', encoding='utf-8') as f:
    file_map = json.load(f)

# Mở file output để ghi kết quả
with open('output_count_extension.txt', 'w', encoding='utf-8') as out_file:
    for filename in os.listdir(input_folder):
        if "Eter" not in filename:
            pass
            #continue
        if filename.endswith('.txt'):
            filepath = os.path.join(input_folder, filename)
            process_file(filepath, file_map, out_file)
    
    ext_cnt = dict(sorted(ext_cnt.items(), key=lambda item: item[1]))
    for ext, count in ext_cnt.items():
        #out_file.write(f"{ext}: {count}\n")
        out_file.write(f"L\".{ext}\",\n")
