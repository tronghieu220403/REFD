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

    for dirpath, _, filenames in os.walk(input_root):
        for filename in filenames:
            if "-" not in filename and filename.endswith(".pdf"):
                src_path = os.path.join(dirpath, filename)
                try:
                    os.remove(src_path)
                except Exception as e:
                    print(f"Cannot delete {src_path}: {e}")

    # Bước 2: Tạo queue chứa các file CÓ chứa dấu '-'
    file_queue = multiprocessing.Queue()
    for dirpath, _, filenames in os.walk(input_root):
        for filename in filenames:
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

if __name__ == "__main__":
    copy_head_tail_parallel(
        "E:\\Graduation_Project\\napier_dataset_encrypted",
        "E:\\Graduation_Project\\napier_dataset_encrypted_crop"
    )
