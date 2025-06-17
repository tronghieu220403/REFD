
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
    print("Login as hieu")
    vm.login('hieu','1', True)
    print("Logged as hieu", flush=True)

def login_as_system(vm: VixVM, f):
    print("Login as SYSTEM", flush=True)
    vm.login('hieu','1', False)
    print("Logged as SYSTEM", flush=True)

def host_wait_min(vm: VixVM, min_to_wait: int, f):
    print(f"Waiting for {min_to_wait} minutes", flush=True)
    for _ in range(min_to_wait * 4):
        vm.power_on(0)
        time.sleep(15)

def host_wait_sec(sec_to_wait, f):
    print(f"Waiting for {sec_to_wait} seconds", flush=True)
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

def run_cmd(vm: VixVM, cmd: str, wait: bool = True):
    #print("Run cmd: ", cmd)
    vm.proc_run("C:\\Windows\\System32\\cmd.exe", '/c "' + cmd + '"', wait)

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



def evaluate_ransom(file_name: str, host_exe_path: str, host_report_path: str, vm: VixVM, f):
    
    guest_download_dir = "C:\\Users\\hieu\\Downloads\\"
    guest_log_path = "E:\\hieunt210330\\hieunt210330\\log.txt"

    print(f"Evaluating ransomware {file_name}", flush=True)
    guest_mal_path = guest_download_dir + file_name + ".exe"
    host_log_name = host_report_path
    host_mal_path = host_exe_path

    if os.path.exists(host_log_name) == True:
        print(f"Log file already exists: {host_log_name}", flush=True)
        print("Done " + file_name, flush=True)
        return True
    try:
        # Back to previous snapshot

        vm.dêt
        os._exit(0)

        time.sleep(2)

        vm.power_off()
        print("Done " + file_name, flush=True)
        return True
    except VixError as ex:
        print("Something went wrong :( {0}".format(ex), flush=True)
        return 2
    except Exception as e:
        print(f"Error: {e}")
    return False

#sys.stdout = open("stdout.txt", "a", encoding='utf-8')

import os
import hashlib
from multiprocessing import Process, Lock, Manager

original_print = builtins.print

host_root_mal_dir = "E:\\Graduation_Project\\marauder\\"
host_root_log_dir = "E:\\Graduation_Project\\marauder_log\\"

def vm_process(vm_path: str, runtime_log: str, ransom_names, mutex):

    # Replace built-in print with our version
    builtins.print = timestamped_print

    host = VixHost()
    vm = host.open_vm(vm_path)

    print("Revert to snapshot", flush=True)
    cur_snap = vm.snapshot_get_current()
    vm.snapshot_revert(cur_snap)
    login_as_system(vm, None)

    from pathlib import Path
    git_path = str(Path.cwd().parent)

    env_path = f"{git_path}\\guest_env"
    collector_path = f'{git_path}\\Event-Collector-Driver'
    sd_path = f'{git_path}\\SelfDefenseKernel'
    
    print("Delete log")
    print("Shut down services")
    run_cmd(vm, "copy nul C:\\Users\\hieu\\Documents\\ggez.txt > nul")
    run_cmd(vm, "copy nul E:\\hieunt210330\\ggez.txt > nul")

    #time.sleep(5)
    run_cmd(vm, 'E:\\hieunt210330\\stop_collector_driver.bat')
    run_cmd(vm, 'E:\\hieunt210330\\stop_sd_driver.bat')

    run_cmd(vm, "del /f E:\\hieunt210330\\hieunt210330\\log.txt")
    run_cmd(vm, "del /f C:\\Users\\hieu\\Documents\\ggez.txt")
    run_cmd(vm, "del /f E:\\hieunt210330\\ggez.txt")

    #run_cmd(vm, f'xcopy "C:\\Users\\hieu\\Downloads\\AAAANapierOne-tiny-backup" "E:\\" /E /I /Y')

    print("Copy driver files to E:\\hieunt210330\\")
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.inf', 'E:\\hieunt210330\\EventCollectorDriver.inf')
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.sys', 'E:\\hieunt210330\\EventCollectorDriver.sys')
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.pdb', 'E:\\hieunt210330\\EventCollectorDriver.pdb')
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.pdb', 'C:\\Windows\\System32\\drivers\\EventCollectorDriver.pdb')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.inf', 'E:\\hieunt210330\\SelfDefenseKernel.inf')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.sys', 'E:\\hieunt210330\\SelfDefenseKernel.sys')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.pdb', 'E:\\hieunt210330\\SelfDefenseKernel.pdb')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.pdb', 'C:\\Windows\\System32\\drivers\\SelfDefenseKernel.pdb')
    while True:
        try:
            
            vm.copy_host_to_guest(f'{git_path}\\RansomDetectorService\\Debug\\RansomDetectorService.exe', 'E:\\hieunt210330\\hieunt210330\\RansomDetectorService.exe')
            vm.copy_host_to_guest(f'{git_path}\\RansomDetectorService\\Debug\\RansomDetectorService.pdb', 'E:\\hieunt210330\\hieunt210330\\RansomDetectorService.pdb')
            
            pass
        except Exception as e:
            print("Error copying files, retrying...", e)
            time.sleep(1)
            continue
        break
    
    print("Start driver and service")
    run_cmd(vm, "E:\\hieunt210330\\start_sd_driver.bat")
    run_cmd(vm, "E:\\hieunt210330\\start_collector_driver.bat")
    run_cmd(vm, "E:\\hieunt210330\\RansomDetectorService.exe", False)
    run_cmd(vm, "sc start REFD")

    print("Creating snapshot", flush=True)
    try:
        vm.create_snapshot("Full setup VM", None, include_memory=True)
    except VixError as e:
        print(f"Error Snapshot VM: {e}", flush=True)
        return
    print("Snapshot created", flush=True)
    
    vm.power_off()

if __name__ == "__main__":

    # Sử dụng multiprocessing.Manager để chia sẻ danh sách giữa các tiến trình
    manager = Manager()
    ransom_names = manager.list(sorted(os.listdir(host_root_mal_dir), key=str.lower, reverse=True))
    mutex = Lock()

    from multiprocessing import freeze_support
    freeze_support()  # Optional nếu không đóng gói .exe, nhưng nên có

    manager = Manager()
    ransom_names = manager.list(sorted(os.listdir(host_root_mal_dir), key=str.lower, reverse=True))
    mutex = Lock()

    n_processes = 14
    processes = []
    for i in range(n_processes):
        vm_path = f'D:\\VM\\Windows_10_test_ransom_{i}\\RansomTestWindows10.vmx'
        runtime_log = f'runtime_log_{i}.txt'
        processes.append(Process(target=vm_process, args=(vm_path, runtime_log, ransom_names, mutex)))
        processes[i].start()
    for i in range(n_processes):
        processes[i].join()