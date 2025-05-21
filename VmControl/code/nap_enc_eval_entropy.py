import os
import math
import multiprocessing
from multiprocessing import Process, Lock, Queue, current_process

from collections import Counter

def chi_square_entropy(data: bytes) -> float:
    if not data:
        return 0.0

    data_len = len(data)
    expected_count = data_len / 256
    counter = Counter(data)

    chi_square = 0.0
    for byte_value in range(256):
        observed = counter.get(byte_value, 0)
        chi_square += (observed - expected_count) ** 2 / expected_count

    return chi_square

def shannon_entropy(data: bytes) -> float:
    if not data:
        return 0.0
    freq = {}
    for byte in data:
        freq[byte] = freq.get(byte, 0) + 1
    total = len(data)
    entropy = -sum((count / total) * math.log2(count / total) for count in freq.values())
    return entropy

def worker(q: Queue, lock, output_file: str, entropy_type: str = "shannon"):
    while True:
        try:
            path = q.get_nowait()
        except:
            break  # Queue is empty

        try:
            with open(path, 'rb') as f:
                data = f.read()
            
            if entropy_type == "shannon":
                entropy = shannon_entropy(data)
            elif entropy_type == "chi_square":
                entropy = chi_square_entropy(data)

            with lock:
                with open(output_file, 'a', encoding='utf-8') as out:
                    out.write(f"{path}\n")
                    out.write(f"{entropy:.6f}\n")
                    out.flush()
        except Exception as e:
            print(f"[{current_process().name}] Lỗi khi xử lý file {path}: {e}")

def process_files(root_dir: str, keyword: str = "", output_file: str = "output.txt", num_processes: int = 8, entropy_type: str = "shannon"):
    if os.path.exists(output_file):
        #os.remove(output_file)
        pass

    q = Queue()
    lock = Lock()

    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            full_path = os.path.join(dirpath, filename)
            if keyword in full_path:
                q.put(full_path)

    processes = []
    for _ in range(num_processes):
        p = Process(target=worker, args=(q, lock, output_file, entropy_type))
        p.start()
        processes.append(p)

    for p in processes:
        p.join()

# Ví dụ chạy
if __name__ == '__main__':
    '''
    process_files(
        root_dir="E:\\Graduation_Project\\napier_dataset_encrypted",
        output_file="nap_enc_shanon_cal.txt",
        num_processes=20, # Số process tùy chỉnh theo CPU
        entropy_type="shannon"
    )
    '''
    process_files(
        root_dir="E:\\Graduation_Project\\napier_dataset_encrypted",
        output_file="nap_enc_chi_square_cal.txt",
        num_processes=20, # Số process tùy chỉnh theo CPU
        entropy_type="chi_square"
    )
