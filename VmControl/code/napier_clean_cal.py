import os
import math
import multiprocessing
from multiprocessing import Process, Lock, Queue, current_process

from collections import Counter
import os
import multiprocessing
from pathlib import Path

MAX_RETRY = 10

def process_file_worker(file_queue: multiprocessing.Queue, input_root: str, output_root: str):
    while not file_queue.empty():
        try:
            src_path = file_queue.get(block=True, timeout=10)
        except:
            break

        rel_path = os.path.relpath(src_path, input_root)
        dst_path = os.path.join(output_root, rel_path)

        dst_dir = os.path.dirname(dst_path)
        os.makedirs(dst_dir, exist_ok=True)

        for attempt in range(MAX_RETRY):
            try:
                if os.path.exists(dst_path):
                    break

                file_size = os.path.getsize(src_path)
                with open(src_path, 'rb') as f_src:
                    if file_size <= 2048:
                        to_write = f_src.read()
                    else:
                        head = f_src.read(1024)
                        f_src.seek(-1024, os.SEEK_END)
                        tail = f_src.read(1024)
                        to_write = head + tail

                with open(dst_path, 'wb') as f_dst:
                    f_dst.write(to_write)

                break  # Success
            except Exception as e:
                if attempt == MAX_RETRY - 1:
                    print(f"Failed to process {src_path} after {MAX_RETRY} attempts. Error: {e}")

def copy_head_tail_parallel(input_root: str, output_root: str):
    input_root = os.path.abspath(input_root)
    output_root = os.path.abspath(output_root)

    # Bước 1: Xoá các file KHÔNG chứa dấu '-' trong tên
    for dirpath, _, filenames in os.walk(input_root):
        for filename in filenames:
            if "-" not in filename:
                src_path = os.path.join(dirpath, filename)
                try:
                    os.remove(src_path)
                except Exception as e:
                    print(f"Cannot delete {src_path}: {e}")

    # Bước 2: Tạo queue chứa các file CÓ chứa dấu '-'
    file_queue = multiprocessing.Queue()
    for dirpath, _, filenames in os.walk(input_root):
        for filename in filenames:
            if "-" in filename:
                src_path = os.path.join(dirpath, filename)
                file_queue.put(src_path)

    # Bước 3: Tạo và khởi động 20 process xử lý
    processes = []
    for _ in range(20):
        p = multiprocessing.Process(
            target=process_file_worker,
            args=(file_queue, input_root, output_root)
        )
        p.start()
        processes.append(p)

    # Bước 4: Chờ tất cả process hoàn tất
    for p in processes:
        p.join()

def chi_square_entropy(data: bytes, counter: Counter = None) -> float:
    if not data:
        return 0.0

    data_len = len(data)
    expected_count = data_len / 256
    if counter is None:
        counter = Counter(data)

    chi_square = 0.0
    for byte_value in range(256):
        observed = counter.get(byte_value, 0)
        chi_square += (observed - expected_count) ** 2 / expected_count

    return chi_square

def shannon_entropy(data: bytes, counter: Counter = None) -> float:
    if not data:
        return 0.0
    if counter is None:
        counter = Counter(data)
    total = len(data)
    entropy = -sum((count / total) * math.log2(count / total) for count in counter.values())
    return entropy

from pathlib import Path
def get_parent_folder_name(file_path: str) -> str:
    return Path(file_path).parent.name

def worker(q: Queue, lock, output_dir: str, processed_paths_shannon: set, processed_paths_chi2: set):
    while True:
        try:
            path = q.get_nowait()
        except:
            break  # Queue is empty

        try:
            with open(path, 'rb') as f:
                data = f.read()
            counter = Counter(data)
            parent_dir_name = get_parent_folder_name(path)
            if path not in processed_paths_shannon:
                entropy = shannon_entropy(data, counter)
                with lock:
                    for _ in range(MAX_RETRY):
                        try:
                            with open(output_dir + "\\shannon\\" + parent_dir_name + ".txt", 'a', encoding='utf-8') as out:
                                out.write(f"{path}\n{entropy:.6f}\n")
                                out.flush()
                        except Exception as e:
                            continue
                        break

            if path not in processed_paths_chi2:
                entropy = chi_square_entropy(data, counter)
                with lock:
                    for _ in range(MAX_RETRY):
                        try:
                            with open(output_dir + "\\chi2\\" + parent_dir_name + ".txt", 'a', encoding='utf-8') as out:
                                out.write(f"{path}\n{entropy:.6f}\n")
                                out.flush()
                        except Exception as e:
                            continue
                        break

        except Exception as e:
            print(f"[{current_process().name}] Lỗi khi xử lý file {path}: {e}")

processes = []

def process_files(root_dir: str, keyword: str = "", output_dir: str = "", num_processes: int = 8):

    q = Queue()
    lock = Lock()

    processed_paths_shannon = set()
    processed_paths_chi2 = set()

    output_dir_shannon = os.path.join(output_dir, "shannon")
    if os.path.isdir(output_dir_shannon):
        for file in os.listdir(output_dir_shannon):
            full_out_file = os.path.join(output_dir_shannon, file)
            if not full_out_file.endswith(".txt"):
                continue
            try:
                with open(full_out_file, "r", encoding="utf-8") as f:
                    lines = f.readlines()
                    # Dữ liệu lưu dạng: path\nentropy\n, nên lấy dòng chẵn
                    processed_paths_shannon.update(line.strip() for i, line in enumerate(lines) if i % 2 == 0)
            except Exception as e:
                print(f"Lỗi khi đọc file kết quả {full_out_file}: {e}")

    output_dir_chi2 = os.path.join(output_dir, "chi2")
    if os.path.isdir(output_dir_chi2):
        for file in os.listdir(output_dir_chi2):
            full_out_file = os.path.join(output_dir_chi2, file)
            if not full_out_file.endswith(".txt"):
                continue
            try:
                with open(full_out_file, "r", encoding="utf-8") as f:
                    lines = f.readlines()
                    # Dữ liệu lưu dạng: path\nentropy\n, nên lấy dòng chẵn
                    processed_paths_chi2.update(line.strip() for i, line in enumerate(lines) if i % 2 == 0)
            except Exception as e:
                print(f"Lỗi khi đọc file kết quả {full_out_file}: {e}")

    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            full_path = os.path.join(dirpath, filename)
            if keyword in full_path and (full_path not in processed_paths_shannon or full_path not in processed_paths_chi2):
                q.put(full_path)

    for _ in range(num_processes):
        p = Process(target=worker, args=(q, lock, output_dir, processed_paths_shannon, processed_paths_chi2))
        p.start()
        processes.append(p)

def run_trid(src_dir: str, log_file: str):
    if os.path.exists(log_file):
        return

    os.system(f"E:\\Code\\VS2022\\TridAPI\\Release\\TridAPI.exe {src_dir} {log_file} 1")

if __name__ == '__main__':

    clean_full_dir = r"E:\Graduation_Project\napier_dataset_total_clean"
    clean_full_log_dir = r"E:\Graduation_Project\custom_dataset\code\report_napier_clean"
    clean_crop_dir = r"E:\Graduation_Project\napier_dataset_total_crop"
    clean_crop_log_dir = r"E:\Graduation_Project\custom_dataset\code\report_napier_clean_crop"

    #copy_head_tail_parallel(clean_full_dir,clean_crop_dir)

    for dir_name in os.listdir(clean_full_dir):
        log_file = os.path.join(clean_full_log_dir, "trid", dir_name + ".txt")
        src_dir = os.path.join(clean_full_dir, dir_name)
        run_trid(src_dir, log_file)
    os._exit(0)
    print("Copy head and tail done! Starting to TrID crop dir...")

    for dir_name in os.listdir(clean_crop_dir):
        log_file = os.path.join(clean_crop_log_dir, "trid", dir_name + ".txt")
        src_dir = os.path.join(clean_crop_dir, dir_name)
        run_trid(src_dir, log_file)
    
    print("TrID done! Starting to TrID full dir...")

    for dir_name in os.listdir(clean_full_dir):
        log_file = os.path.join(clean_full_log_dir, "trid", dir_name + ".txt")
        src_dir = os.path.join(clean_full_dir, dir_name)
        run_trid(src_dir, log_file)

    print("TrID done! Starting to calculate entropy for full dir...")

    process_files(
        root_dir=clean_full_dir,
        output_dir=clean_full_log_dir,
        num_processes=20, # Số process tùy chỉnh theo CPU
    )

    print("Entropy full dir done! Starting to calculate entropy for crop dir...")

    process_files(
        root_dir=clean_crop_dir,
        output_dir=clean_crop_log_dir,
        num_processes=20, # Số process tùy chỉnh theo CPU
    )