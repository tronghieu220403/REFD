import os
import json

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

def extract_string_x(filename: str):
    try:
        x = filename.split("-")[1]
        return x
    except IndexError:
        return None

black_list_name = [
    "WhatHappened".lower(),
    "read",
    "Restore".lower(),
    "instruction",
    "DECRYP".lower(),
    "R_E_A_D".lower(),
    "HOW".lower(),
    "NOTIFICATION".lower(),
    "HELP".lower(),
    "CriticalBreachDetected".lower(),
    "RGNR".lower(),
    "Recov".lower(),
    "LostTrustEncoded".lower(),
]

white_list_name = [
    "Bit_decrypt@".lower(),
    "supportdecrypt@firemail".lower(),
    "losttrustencoded",
    "ReadManual".lower(),
]

from pathlib import Path

def get_parent_folder_name(file_path):
    return Path(file_path).parent.name


from collections import defaultdict

accepted_types = defaultdict(set)

with open("napier_dataset.txt", 'r', encoding='utf-8') as f:
    lines = [line.strip().lower() for line in f]

for i in range(0, len(lines), 2):
    if i+1 >= len(lines):
        break
    
    filename = os.path.basename(lines[i])
    parent_path = get_parent_folder_name(lines[i])
    content = lines[i+1].lower()

    if len(content) == 0:
        black_list_name.append(filename)
        continue
    if content[-1] == ";":
        content = content[:-1]
    l = content.split(";")
    accepted_types[parent_path].update(l)

def process_file(filepath, file_map, out_file):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = [line.strip().lower() for line in f]
    
    matches = 0
    total = 0
    no_type = 0

    ext_list = set()

    for i in range(0, len(lines), 2):
        if i+1 >= len(lines):
            break
        filename = os.path.basename(lines[i])
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
            if is_blacklisted:
                continue        
        content = lines[i+1].lower()
        total += 1
        if len(content) > 0:
            if content[-1] == ";":
                content = content[:-1]

            parent_path = get_parent_folder_name(lines[i])
            l = content.split(";")
            ext_list = set(l)
            if len(ext_list) == 0:
                no_type += 1
            if not accepted_types[parent_path].isdisjoint(ext_list):
                matches += 1
                pass
            else:
                pass
        else:
            no_type += 1
    return total, matches, no_type

# Đường dẫn thư mục chứa các file txt cần xử lý
input_folder = "E:\\Graduation_Project\\custom_dataset\\code\\report\\" 

# Đọc file_map.json
with open('E:\\hieunt20210330\\File-Map\\file_map.json', 'r', encoding='utf-8') as f:
    file_map = json.load(f)

# Mở file output để ghi kết quả
with open('output_custom_dataset.txt', 'w', encoding='utf-8') as out_file:
    cnt = 0
    family_list = set()
    for filename in os.listdir(input_folder):
        if "Dha" not in filename:
            pass
            #continue
        if filename.endswith('.txt'):
            cnt += 1
            filepath = os.path.join(input_folder, filename)
            total, matches, no_ext = process_file(filepath, file_map, out_file)
            mismatches = total - matches
            mismatch_rate = (mismatches / total) * 100 if total else 0
            no_ext_rate = (no_ext / total) * 100 if total else 0
            out_file.write(f'File: {filename}\n')
            out_file.write(f'Total files: {total}\n') 
            out_file.write(f'Number of file with no ext detected: {no_ext}\n')
            out_file.write(f'No extension rate: {no_ext_rate:.2f}%\n')
            out_file.write(f'Number of file extensions mismatch content: {mismatches}\n')
            out_file.write(f'Mismatch rate: {mismatch_rate:.2f}%\n\n')
            #out_file.write(f'{cnt} & {filename.replace(".txt", "")} & {total} & {mismatches} & {percentage:.2f}\\% \\\\\n')
            family_list.add(filename.replace(".txt", ""))
    
    output_str = ""
    family_list = sorted(family_list)
    for i, family in enumerate(family_list):
        if i % 4 == 0 and i != 0:
            output_str += "\\\\\n"
        output_str += f"{family} "
        if i != len(family_list) - 1 and i % 4 != 3:
            output_str += "& "
    out_file.write("\n")
    out_file.write(output_str.strip() + "\n")