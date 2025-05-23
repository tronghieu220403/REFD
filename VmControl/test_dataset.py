
import random
import os
import sys
import shutil
from vix import VixHost, VixError, VixVM
import time

import builtins
from datetime import datetime

# Override print function
def timestamped_print(*args, **kwargs):
    timestamp = datetime.now().strftime("%Y:%m:%d %H:%M:%S")
    original_print(f"[{timestamp}]", *args, **kwargs)

def delete_log_files(folder_path):
    for filename in os.listdir(folder_path):
        file_path = os.path.join(folder_path, filename)
        if os.path.isfile(file_path) and 'log' in filename:
            try:
                os.remove(file_path)
                print(f"Đã xóa: {file_path}")
            except Exception as e:
                print(f"Lỗi khi xóa {file_path}: {e}")

def get_all_files(vm, path):
    """
    Duyệt toàn bộ cây thư mục và thu thập danh sách tất cả các file.
    """
    file_list = []
    stack = [path]  # Sử dụng stack để duyệt cây thư mục
    
    while stack:
        current_path = stack.pop()
        try:
            entries = vm.dir_list(current_path)
        except Exception as e:
            print(f"Lỗi khi truy cập {current_path}: {e}")
            continue
        
        for entry in entries:
            file_name, _, is_dir, _, _ = entry
            full_path = f"{current_path}\\{file_name}"
            
            if is_dir:
                stack.append(full_path)  # Thêm thư mục vào stack để duyệt tiếp
            else:
                file_list.append(full_path)
    
    return file_list

def login_as_hieu(vm: VixVM, f):
    print("Login as hieu", file=f)
    vm.login('hieu','1', True)
    print("Logged as hieu", flush=True, file=f)

def login_as_system(vm: VixVM, f):
    print("Login as SYSTEM", flush=True, file=f)
    vm.login('hieu','1', False)
    print("Logged as SYSTEM", flush=True, file=f)

def host_wait_min(vm: VixVM, min_to_wait: int, f):
    print(f"Waiting for {min_to_wait} minutes", flush=True, file=f)
    for _ in range(min_to_wait * 4):
        vm.power_on(0)
        time.sleep(15)

def host_wait_sec(sec_to_wait, f):
    print(f"Waiting for {sec_to_wait} seconds", flush=True, file=f)
    time.sleep(sec_to_wait)

def ReadFileAndProcess(file_path):
    file_hash_ = dict()
    with open(file_path, 'r', encoding='utf-8') as file:
        lines = file.readlines()

    for i in range(0, len(lines), 2):
        file_path_value = lines[i].strip().lower()
        hash_value = 0

        if i + 1 < len(lines):
            try:
                hash_value = int(lines[i + 1].strip())
            except ValueError:
                hash_value = 0
        file_hash_[file_path_value] = hash_value
    return file_hash_

import multiprocessing
import time

import threading
import time

def run_proc(vm: VixVM, copy_cmd: str, done_event: threading.Event):
    try:
        vm.proc_run("C:\\Windows\\System32\\cmd.exe", f'/c "{copy_cmd}"', True)
    finally:
        done_event.set()

def run_with_timeout(vm: VixVM, cmd: str, timeout=360):
    done_event = threading.Event()
    t = threading.Thread(target=run_proc, args=(vm, cmd, done_event))
    t.start()

    finished = done_event.wait(timeout)

    if not finished:
        return 1
    else:
        t.join()
        return 0

def ClearDirectory(directory_path: str):
    """
    Xóa toàn bộ file và thư mục con bên trong thư mục được chỉ định, nhưng giữ nguyên thư mục đó.
    
    :param directory_path: Đường dẫn đến thư mục cần xóa nội dung.
    """
    if not os.path.exists(directory_path):
        print(f"Thư mục '{directory_path}' không tồn tại.")
        return
    
    for item in os.listdir(directory_path):
        item_path = os.path.join(directory_path, item)
        try:
            if os.path.isfile(item_path) or os.path.islink(item_path):
                os.unlink(item_path)  # Xóa file hoặc symbolic link
            elif os.path.isdir(item_path):
                shutil.rmtree(item_path)  # Xóa thư mục con và toàn bộ nội dung của nó
        except Exception as e:
            print(f"Không thể xóa '{item_path}': {e}")

def evaluate_ransom(file_name: str, host_mal_dir: str, vm: VixVM, f):
    
    guest_download_dir = "C:\\Users\\hieu\\Downloads\\"
    guest_log_path = "E:\\hieunt20210330\\log.txt"

    print(f"Evaluating ransomware {file_name}", flush=True, file=f)
    guest_mal_path = guest_download_dir + file_name + ".exe"
    host_log_name = host_mal_dir + "log.txt"
    host_mal_path = host_mal_dir + file_name

    if True == False:
        return
    try:
        # Back to previous snapshot
        print("Revert to snapshot", flush=True, file=f)
        cur_snap = vm.snapshot_get_current()
        vm.snapshot_revert(cur_snap)

        # Run VM
        print("Run VM", flush=True, file=f)
        if vm.power_state != VixVM.VIX_POWERSTATE_POWERED_ON:
            vm.power_on()

        login_as_system(vm, f)

        # Copy malware file from host to guest
        print(f"Copy malware file from host to guest: {host_mal_path} -> {guest_mal_path}", flush=True, file=f)
        vm.copy_host_to_guest(host_mal_path, guest_mal_path)

        # TODO: Run self defense, SD and evaluator service


        # Run malware
        print("Run malware", flush=True, file=f)
        try:
            vm.proc_run(guest_mal_path, None, False) # not wait for malware to finish
            pass
        except VixError as ex:
            print(f"Malware is not compatible: {file_name}", flush=True, file=f)
            return
        
        print("Wait for 15 minutes", flush=True, file=f)
        host_wait_min(vm, 15, f)

        login_as_system(vm, f)
        
        # Copy log file from guest to host
        print(f"Copy log file from guest to host: {guest_log_path} -> {host_log_name}", flush=True, file=f)
        vm.copy_guest_to_host(guest_log_path, host_log_name)

        # Check if lines that have both "is_modified: 1" and \"AAAANapierOne-tiny\" exists at least X times in log file
        print("Check if lines that have both \"is_modified: 1\" and \"AAAANapierOne-tiny\" exists at least X times in log file", flush=True, file=f)
        guest_napier_dir_lower_str = str("apierOne").lower()
        guest_is_modified_str = str("is_modified: 1").lower()

        def cnt_napier_one_tiny(log_path: str, guest_napier_dir_lower_str: str, guest_is_modified_str: str):\
            # TODO: Check if lines that have both "is_modified: 1" and \"AAAANapierOne-tiny\" exists at least X times in log file
            '''
            with open(log_path, 'r', encoding='utf-16-le') as log_file:
                log_data = log_file.read()
            return log_data.count(guest_napier_dir_lower)
            '''
            pass
        X = 20
        cnt = cnt_napier_one_tiny(host_log_name, guest_napier_dir_lower_str, guest_is_modified_str)
        if  cnt < X:
            print(f"Not found enough modified files in log file, appears {cnt} times", flush=True, file=f)
            print(f"Malware is not work: {file_name}", flush=True, file=f)
            return
        else:
            # TODO: If found found a line with "is ransomware" then break.
            '''
            print(f"NapierOne-tiny found enough in log file, appears {cnt} times", flush=True, file=f)
            last_cnt = cnt
            for i in range(1, 15):
                host_wait_min(vm, 4, f)
                # Copy log file from guest to host
                print(f"Copy log file from guest to host: {guest_log_path} -> {host_log_name}", flush=True, file=f)
                vm.copy_guest_to_host(guest_log_path, host_log_name)
                cnt = cnt_napier_one_tiny(host_log_name, guest_napier_dir_lower)
                if cnt > last_cnt + X:
                    print(f"NapierOne-tiny found enough in log file, appears {cnt} times", flush=True, file=f)
                    last_cnt = cnt
                else:
                    break
            '''
            '''
            # TODO: Turn off all drivers and services ?? Not nessary
            try:
                print("Turn off all drivers and services", flush=True, file=f)
                time.sleep(1)
            except VixError as ex:
                pass
            '''
            vm.power_off()
            print("Done " + file_name, flush=True, file=f)
            return True
    except VixError as ex:
        print("Something went wrong :( {0}".format(ex), flush=True, file=f)
    except Exception as e:
        print(f"Error: {e}", file=f)
    return False

#sys.stdout = open("stdout.txt", "a", encoding='utf-8')

import os
import hashlib
from multiprocessing import Process, Lock, Manager

def GetSha256Filename(directory: str) -> str:
    if not os.path.isdir(directory):
        return ""
    
    files = [f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))]
    if not files:
        return ""
    
    for file in files:
        if "." not in file:
            return file
    
    return ""  # Return empty string if no match found

original_print = builtins.print

def vm_process(vm_path: str, runtime_log: str, ransom_familes, mutex):
    # Save original print
    original_print = builtins.print

    # Replace built-in print with our version
    builtins.print = timestamped_print

    host_root_mal_dir = "E:\\Graduation_Project\\custom_dataset\\ransomware_dataset\\need_update\\"
    with open(runtime_log, 'a', encoding='utf-8') as f:
        host = VixHost()
        vm = host.open_vm(vm_path)
        while True:
            mutex.acquire()
            if len(ransom_familes) == 0:
                mutex.release()
                break
            ransom_family = ransom_familes.pop()
            mutex.release()
            
            host_mal_dir = os.path.join(host_root_mal_dir, ransom_family)
            if os.path.exists(os.path.join(host_mal_dir, "encrypted")):
                print(f"Ransomware family {ransom_family} has been evaluated", flush=True, file=f)
                continue
            print(file=f)
            print(f"Processing ransomware family {ransom_family}", flush=True, file=f)
            file_name = GetSha256Filename(host_mal_dir)
            if file_name == "":
                print(f"Cannot find ransomware file in {host_mal_dir}", flush=True, file=f)
                continue
            for _ in range(10):
                if evaluate_ransom(file_name, os.path.join(host_mal_dir, ""), vm, f) == True:
                    break
                else:
                    delete_log_files(host_mal_dir)
                    if _ == 9:
                        print(f"Cannot evaluate ransomware {file_name}\n", flush=True, file=f)
                    else:
                        print(f"Retrying {file_name}\n", flush=True, file=f)
                        time.sleep(10)

if __name__ == "__main__":
    host_root_mal_dir = "E:\\Graduation_Project\\custom_dataset\\ransomware_dataset\\need_update\\"

    # Sử dụng multiprocessing.Manager để chia sẻ danh sách giữa các tiến trình
    manager = Manager()
    ransom_familes = manager.list(sorted(os.listdir(host_root_mal_dir), key=str.lower, reverse=True))
    mutex = Lock()

    from multiprocessing import freeze_support
    freeze_support()  # Optional nếu không đóng gói .exe, nhưng nên có

    manager = Manager()
    ransom_familes = manager.list(sorted(os.listdir(host_root_mal_dir), key=str.lower, reverse=True))
    mutex = Lock()

    vm_path_0 = r'E:\\VM\\Windows_10_test_ransom_0\\RansomTestWindows10.vmx'
    runtime_log_0 = r'runtime_log_0.txt'
    p0 = Process(target=vm_process, args=(vm_path_0, runtime_log_0, ransom_familes, mutex))

    vm_path_1 = r'E:\\VM\\Windows_10_test_ransom_1\\RansomTestWindows10.vmx'
    runtime_log_1 = r'runtime_log_1.txt'
    p1 = Process(target=vm_process, args=(vm_path_1, runtime_log_1, ransom_familes, mutex))

    p0.start()
    p1.start()

    p0.join()
    p1.join()
