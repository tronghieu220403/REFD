
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
    while True:
        try:
            original_print(f"[{timestamp}]", *args, **kwargs)
        except Exception:
            continue
        break

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
        try:
            vm.power_on(0)
        except Exception:
            pass
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

def cnt_str(log_path: str, guest_is_modified_str: str):
    # Count the number of line that have both guest_napier_dir_lower_str and guest_is_modified_str" 
    count = 0
    count1 = 0
    with open(log_path, 'r', encoding='utf-16-le') as log_file:
        for line in log_file:
            if guest_is_modified_str in line:
                count += 1
            if "is ransomware" in line:
                count1 += 1
    return (count, count1)

def check_ransom_str(log_path: str):
    if os.path.exists(log_path) == False:
        return False
    try:
        with open(log_path, 'r', encoding='utf-16-le') as log_file:
            for line in log_file:
                if "is ransomware" in line:
                    return True
    except Exception:
        pass
    return False

def evaluate_ransom(file_name: str, host_exe_path: str, host_report_path: str, vm: VixVM, f, max_run_time):
    
    guest_download_dir = "C:\\Users\\hieu\\Downloads\\"
    guest_log_path = "E:\\hieunt210330\\hieunt210330\\log.txt"

    print(f"Evaluating clean file {file_name}", flush=True, file=f)
    guest_clean_path = guest_download_dir + file_name
    host_log_name = host_report_path
    host_clean_path = host_exe_path
    try:
        with open(host_log_name, 'w') as empty_file:
            pass

        # Back to previous snapshot
        print("Revert to snapshot", flush=True, file=f)
        cur_snap = vm.snapshot_get_current()
        vm.snapshot_revert(cur_snap)
        
        # Run VM
        print("Run VM", flush=True, file=f)
        if vm.power_state != VixVM.VIX_POWERSTATE_POWERED_ON:
            vm.power_on()

        login_as_system(vm, f)

        print(f"Copy clean file from host to guest: {host_clean_path} -> {guest_clean_path}", flush=True, file=f)
        vm.copy_host_to_guest(host_clean_path, guest_clean_path)

        # Run clean file
        print("Run clean file", flush=True, file=f)
        try:
            login_as_hieu(vm, f)
            vm.proc_run(guest_clean_path, None, False) # not wait for clean file to finish
            login_as_system(vm, f)
            pass
        except VixError as ex:
            print(f"File is not compatible: {file_name}", flush=True, file=f)
            print(f"File is not compatible: {file_name}", flush=True)
            shutil.move("E:\\Graduation_Project\\clean_combined\\" + file_name, "E:\\Graduation_Project\\clean_combined_not_compatible\\" + file_name)
            return False

        host_wait_min(vm, max_run_time, f)
        '''
        if os.path.exists(host_log_name):
            os.remove(host_log_name)
            pass
        '''
        vm.copy_guest_to_host(guest_log_path, host_log_name)

        vm.power_off()
        print("Done " + file_name, flush=True, file=f)

        return False
    except VixError as ex:
        print("Something went wrong :( {0}".format(ex), flush=True, file=f)
        return 2
    except Exception as e:
        print(f"Error: {e}", file=f)
    return False

#sys.stdout = open("stdout.txt", "a", encoding='utf-8')

import os
import hashlib
from multiprocessing import Process, Lock, Manager

original_print = builtins.print

def check_done(log_path: str):
    if os.path.exists(log_path) == False:
        return False
    if os.path.getsize(log_path) == 0:
        return False
    return True

host_root_clean_dir = "E:\\Graduation_Project\\clean_combined\\"
host_root_log_dir = "E:\\Graduation_Project\\clean_combined_log\\"

def vm_process(vm_path: str, runtime_log: str, clean_names, mutex, max_run_time):
    # Save original print
    original_print = builtins.print
    host = VixHost()
    vm = host.open_vm(vm_path)

    # Replace built-in print with our version
    builtins.print = timestamped_print
    with open(runtime_log, 'a', encoding='utf-8') as f:
        while True:
            if os.path.exists("ggez.txt") == True:
                print("ggez.txt found, exiting...", flush=True)
                try:
                    vm.power_off()
                except VixError as ex:
                    pass
                break
            mutex.acquire()
            print(f"len(clean_names): {len(clean_names)}")
            if len(clean_names) <= 0:
                mutex.release()
                break
            clean_name = clean_names.pop()
            mutex.release()

            log_path = host_root_log_dir + clean_name + ".log"
            
            print(file=f)
            print(f"Processing clean file name {clean_name}", flush=True, file=f)

            from pathlib import Path; clean_name = Path(clean_name).name

            if check_done(log_path) == True:
                print(f"Clean file evaluated: {log_path}", flush=True, file=f)
                print(f"Clean file evaluated: {log_path}", flush=True)
                shutil.move("E:\\Graduation_Project\\clean_combined\\" + clean_name, "E:\\Graduation_Project\\clean_combined_done\\" + clean_name)
                continue

            clean_path = host_root_clean_dir + clean_name

            for _ in range(0, 1):
                print(file=f)
                print(f"Processing clean file name {clean_name}, attempt {_}", flush=True, file=f)
                ret = evaluate_ransom(clean_name, clean_path, log_path, vm, f, max_run_time)
                try:
                    vm.power_off()  
                except Exception:
                    pass
                if ret == True:
                    break
                elif ret == False:
                    print(f"Cannot evaluate clean file {clean_name}\n", flush=True, file=f)
                    if os.path.exists(log_path):
                        #os.remove(log_path)
                        pass
                    #time.sleep(5)
                    continue
                    if os.path.exists(log_path) == False:
                        # Create an empty log fileQ
                        with open(log_path, 'w') as empty_file:
                            pass
                    #break
                elif ret == 2:
                    print(f"Retrying {clean_name}", flush=True, file=f)
                    if os.path.exists(log_path):
                        #os.remove(log_path)
                        pass
                    #time.sleep(5)
                    continue

if __name__ == "__main__":

    # Get args
    if len(sys.argv) > 1:
        n_processes = int(sys.argv[1])  # chuyển đối số đầu tiên thành số nguyên
    else:
        n_processes = 10

    max_run_time = 5

    while True:
        file_list = os.listdir(host_root_clean_dir)
        if len(file_list) == 0 or file_list == None:
            break
        random.shuffle(file_list)

        # Sử dụng multiprocessing.Manager để chia sẻ danh sách giữa các tiến trình
        manager = Manager()
        clean_names = manager.list(file_list)
        mutex = Lock()

        from multiprocessing import freeze_support
        freeze_support()  # Optional nếu không đóng gói .exe, nhưng nên có

        manager = Manager()
        clean_names = manager.list(file_list)
        mutex = Lock()
        
        processes = []
        for i in range(n_processes):
            vm_path = f'D:\\VM\\Windows_10_test_ransom_{i}\\RansomTestWindows10.vmx'
            runtime_log = f'clean_runtime_log_{i}.txt'
            processes.append(Process(target=vm_process, args=(vm_path, runtime_log, clean_names, mutex, max_run_time)))
            processes[i].start()
        for i in range(n_processes):
            processes[i].join()
        #max_run_time += 5
        if os.path.exists("ggez.txt") == True:
            break
        time.sleep(10)