import os
import hashlib
import shutil

def ComputeSha256ForFiles(root_dir: str) -> dict:
    result = {}
    max_read_size = 100 * 1024  # 100KB

    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            file_path = os.path.join(dirpath, filename)
            relative_path = os.path.relpath(file_path, root_dir)
            try:
                with open(file_path, 'rb') as f:
                    data = f.read(max_read_size)
                    sha256_hash = hashlib.sha256(data).hexdigest()
                    result[relative_path] = sha256_hash
            except Exception as e:
                print(f"Warning: Could not read {file_path} - {e}")
    
    return result

def ListDirectories(root_dir: str) -> list:
    directories = []
    for item in os.listdir(root_dir):
        item_path = os.path.join(root_dir, item)
        if os.path.isdir(item_path):
            directories.append(item)
    return directories

import sys

sys.stdout = open("report.txt", "w")

ori_hashes = ComputeSha256ForFiles("E:\\Code\\Python\\AAAANapierOne-tiny")

import re

def ExtractTrimmedFilePathsToDict(file_path: str) -> dict:
    result = {}
    keyword = "File Operation, Create event"
    path_marker = "file path "

    with open(file_path, 'r', encoding='utf-16-le') as f:
        for line in f:
            if keyword in line:
                path_start = line.find(path_marker)
                if path_start != -1:
                    path_start += len(path_marker)
                    path_end = line.find(',', path_start)
                    raw_path = line[path_start:path_end].strip()

                    if raw_path:
                        norm_path = os.path.normpath(raw_path)
                        path_parts = norm_path.split(os.sep)

                        if len(path_parts) > 2:
                            trimmed_parts = path_parts[:-2]
                            trimmed_path = os.path.join(*trimmed_parts) + os.sep
                            if trimmed_path not in result:
                                result[trimmed_path] = True
    return result

napier_path = dict[str, bool]()

root_dir = "E:\\Graduation_Project\\custom_dataset\\done_real\\"

directories = ListDirectories(root_dir)

output_dir = "E:\\Graduation_Project\\custom_dataset\\code\\report\\"

import os
import threading

def run_trid(directory, root_dir, output_dir):
    encr_path = root_dir + directory + "\\encrypted"
    os.system(f"E:\\Code\\VS2022\\TridAPI\\Release\\TridAPI.exe {encr_path} {output_dir}")

threads = []

for directory in directories:
    t = threading.Thread(target=run_trid, args=(directory, root_dir, output_dir))
    t.start()
    threads.append(t)

# Đợi tất cả thread hoàn thành
for t in threads:
    t.join()