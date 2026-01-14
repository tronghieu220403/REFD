import os
import sys
import time
import shutil
import random
import threading
import builtins
from datetime import datetime
from multiprocessing import Process, Lock, Manager

from vix import VixHost, VixError, VixVM


# ============================
# CONFIG
# ============================

HOST_ROOT_MAL_DIR = "D:\\MalwareBazaarWork"
HOST_ROOT_LOG_DIR = "D:\\MalwareBazaarRun\\logs_refd"

HOST_NOT_COMPATIBLE_DIR = "D:\\MalwareBazaarRun\\not_compatible"

GUEST_DOWNLOAD_DIR = "C:\\Users\\hieu\\Downloads\\"
GUEST_LOG_PATH = "C:\\Windows\\EventCollectorDriver.log"

STOP_FILE = "E:\\Code\\Github\\REFD\\REFD2026\\vm\\ggez.txt"

DEFAULT_NUM_PROCESSES = 1
DEFAULT_MAX_RUN_TIME = 4  # minutes

COLLECTOR_PATH = f'E:\\Code\\Github\\REFD\\EventCollectorDriver\\x64\\Debug'

RUN_TIME_LOG_DIR = f"E:\\Code\\Github\\REFD\\REFD2026\\vm\\rtlog"

# ============================
# TIMESTAMP PRINT
# ============================

original_print = builtins.print

def timestamped_print(*args, **kwargs):
    timestamp = datetime.now().strftime("%Y:%m:%d %H:%M:%S")
    if "file" in kwargs:
        original_print(f"[{timestamp}]", *args, **kwargs)
        kwargs = {k: v for k, v in kwargs.items() if k != "file"}
    return original_print(f"[{timestamp}]", *args, **kwargs)


# ============================
# UTIL
# ============================

def run_proc(vm: VixVM, copy_cmd: str, wait: bool = True):
    vm.proc_run("C:\\Windows\\System32\\cmd.exe", f'/c "{copy_cmd}"', wait)

def run_proc_with_notify(vm: VixVM, copy_cmd: str, done_event: threading.Event):
    try:
        vm.proc_run("C:\\Windows\\System32\\cmd.exe", f'/c "{copy_cmd}"', True)
    finally:
        done_event.set()

def run_with_timeout(vm: VixVM, cmd: str, timeout=360):
    done_event = threading.Event()
    t = threading.Thread(target=run_proc_with_notify, args=(vm, cmd, done_event))
    t.start()

    finished = done_event.wait(timeout)

    if not finished:
        return False
    else:
        t.join()
        return True

def host_wait_min(vm, min_to_wait: int, f):
    print(f"Waiting for {min_to_wait} minutes", flush=True, file=f)
    for _ in range(min_to_wait * 4):
        try:
            vm.power_on(False)
        except Exception:
            pass
        time.sleep(15)


def host_wait_sec(sec, f, vm: VixVM):
    print(f"Waiting for {sec} seconds", flush=True, file=f)
    for i in range(sec):
        if (i % 3 == 0):
            run_proc(vm, "whoami", False)
        time.sleep(1)

# ============================
# VM FUNCTIONS
# ============================

def vm_open(vm_path: str):
    host = VixHost()
    vm = host.open_vm(vm_path)
    return vm


def vm_revert_to_current_snapshot(vm, f):
    print("Revert to snapshot", flush=True, file=f)
    cur_snap = vm.snapshot_get_current()
    vm.snapshot_revert(cur_snap)


def vm_ensure_power_on(vm, f):
    print("Run VM", flush=True, file=f)
    if vm.power_state != VixVM.VIX_POWERSTATE_POWERED_ON:
        vm.power_on(False)


def vm_login_as_system(vm, username: str, password: str, f):
    print(f"Login as {username}", flush=True, file=f)
    vm.login(username, password, False)
    print(f"Logged as {username}", flush=True, file=f)


def vm_copy_malware_to_guest(vm, host_mal_path: str, guest_mal_path: str, f):
    print(f"Copy malware file: {host_mal_path} -> {guest_mal_path}", flush=True, file=f)
    vm.copy_host_to_guest(host_mal_path, guest_mal_path)

def vm_run_collector(vm):
    vm.proc_run("C:\\Windows\\System32\\cmd.exe", f'/c "fltmc load EventCollectorDriver""', True)

def vm_run_malware(vm, guest_mal_path: str, f):
    print("Run malware", flush=True, file=f)
    vm.proc_run(guest_mal_path, None, False)

def vm_safe_power_off(vm, f=None):
    try:
        vm.power_off()
        if f:
            print("Power off VM", flush=True, file=f)
    except Exception:
        if f:
            print("Power off VM failed (ignored)", flush=True, file=f)

def pull_log(vm, guest_log_path: str, host_log_path: str, f):
    tmp_path = host_log_path + "_tmp"

    # Remove old tmp if it exists
    if os.path.exists(tmp_path):
        try:
            os.remove(tmp_path)
        except:
            pass

    vm.copy_guest_to_host(guest_log_path, tmp_path)
    try:
        if os.path.exists(host_log_path):
            try:
                os.remove(host_log_path)
            except:
                pass

        os.rename(tmp_path, host_log_path)
        print(f"Copied log: {guest_log_path} -> {host_log_path}", flush=True, file=f)
        return True
    except Exception as e:
        print(f"Copied failed {e}: {guest_log_path} -> {host_log_path}", flush=True, file=f)
        return False

def pull_file(vm: VixVM, guest_path: str, host_dir: str, f):
    host_path = os.path.join(host_dir, os.path.basename(guest_path))
    try:
        if os.path.exists(host_path) == False:
            vm.copy_guest_to_host(guest_path, host_path)
            print(f"copy_guest_to_host from {guest_path} to {host_path} files", file=f)
    except Exception as e:
        print(f"Failed, copy_guest_to_host from {guest_path} to {host_path} files, error {e}", file=f)
        pass

# ============================
# LOG ANALYSIS
# ============================

def is_ransom_work(log_path):
    if not os.path.exists(log_path):
        return False
    try:
        with open(log_path, 'r', encoding='utf-16-le') as f:
            for line in f:
                l = line.lower()
                if "hieunt" in l:
                    return True
    except Exception:
        pass
    return False

# ============================
# EVALUATE 1 SAMPLE
# ============================

def evaluate_ransom(sample_name: str, vm: VixVM, f, max_run_time, done_event: threading.Event, vm_path):
    host_mal_path = os.path.join(HOST_ROOT_MAL_DIR, sample_name)
    guest_mal_path = os.path.join(GUEST_DOWNLOAD_DIR, sample_name + ".exe")
    host_log_path = os.path.join(HOST_ROOT_LOG_DIR, sample_name + ".log")

    print(f"Evaluating ransomware {sample_name}", flush=True, file=f)
    try:
        vm_revert_to_current_snapshot(vm, f)
        vm_ensure_power_on(vm, f)
        vm_login_as_system(vm, "hieu", "1", f)
        vm.copy_host_to_guest(f'{COLLECTOR_PATH}\\EventCollectorDriver.sys', 'C:\\Windows\\System32\\drivers\\EventCollectorDriver.sys')
        run_proc(vm, "del /f /q \"C:\\Windows\\Minidump\\*\"")
        vm_copy_malware_to_guest(vm, host_mal_path, guest_mal_path, f)
        vm_run_collector(vm)
        host_wait_sec(5, f, vm)
        try:
            vm_run_malware(vm, guest_mal_path, f)
        except VixError as ex:
            print(f"VIX error: {ex}, {sample_name}, {vm_path}", flush=True, file=f)
            if "The virtual machine needs to be powered on" in f"{ex}":
                done_event.set()
                raise ex
            done_event.set()
            return False

        host_wait_sec(2 * 60, f, vm)
        open(host_log_path, "w").close()
        if pull_log(vm, GUEST_LOG_PATH, host_log_path, f) == False or is_ransom_work(host_log_path) == False:
            done_event.set()
            return False
        host_wait_sec((max_run_time - 2) * 60, f, vm)
        if pull_log(vm, GUEST_LOG_PATH, host_log_path, f) == True and is_ransom_work(host_log_path) == True:
            done_event.set()
            return True
        done_event.set()
        return False
    except VixError as ex:
        print(f"VIX error: {ex}, {sample_name}, {vm_path}", flush=True, file=f)
        try:
            os.remove(host_log_path)
        except:
            pass
        done_event.set()
        return 2
    except Exception as e:
        print(f"Error: {e}", flush=True, file=f)
        done_event.set()
        return False

# ============================
# WORKER PROCESS
# ============================

def evaluate_ransom_with_timeout(sample_name: str, vm: VixVM, f, max_run_time_minute, vm_path):
    host_log_path = os.path.join(HOST_ROOT_LOG_DIR, sample_name + ".log")
    done_event = threading.Event()
    t = threading.Thread(target=evaluate_ransom, args=(sample_name, vm, f, max_run_time_minute, done_event, vm_path))
    t.start()

    finished = done_event.wait((max_run_time_minute + 3) * 60)

    if not finished or os.path.exists(host_log_path) == False:
        try:
            os.remove(host_log_path)
        except:
            pass
        os._exit(0)
        return 2
    else:
        t.join()
        if is_ransom_work(host_log_path) == True:
            return True
        try:
            os.remove(host_log_path)
        except:
            pass
        return False

def vm_process(vm_path: str, runtime_log: str, ransom_names, mutex, max_run_time_minute):
    builtins.print = timestamped_print

    vm = vm_open(vm_path)

    with open(runtime_log, 'a', encoding='utf-8') as f:
        print(f"VM process start: {vm_path}", flush=True, file=f)

        while True:
            if os.path.exists(STOP_FILE):
                print("STOP file detected, exit", flush=True, file=f)
                vm_safe_power_off(vm, f)
                break

            with mutex:
                left = len(ransom_names)
                print(f"Remaining samples: {left}", flush=True, file=f)
                if left <= 0:
                    break
                name = ransom_names.pop()

            print(f"Processing {name}", flush=True, file=f)
            ret = evaluate_ransom_with_timeout(name, vm, f, max_run_time_minute, vm_path)
            
            if ret == 2:
                os._exit(0)
            vm_safe_power_off(vm, f)
            if ret is True:
                print(f"Done {name}\n", flush=True, file=f)
            elif ret is False:
                print(f"Failed: {name}\n", flush=True, file=f)
                time.sleep(5)
            elif ret == 2:
                print(f"Need to retry: {name}\n", flush=True, file=f)
                time.sleep(5)

        print("VM process end", flush=True, file=f)


# ============================
# MAIN LOOP
# ============================

def main():
    if len(sys.argv) > 1:
        try:
            n_processes = int(sys.argv[1])
        except:
            n_processes = DEFAULT_NUM_PROCESSES
    else:
        n_processes = DEFAULT_NUM_PROCESSES

    max_run_time = DEFAULT_MAX_RUN_TIME

    os.system("taskkill /IM vmware-vmx.exe /F")
    time.sleep(5)
    while True:
        names = os.listdir(HOST_ROOT_MAL_DIR)
        files = []
        for name in names:
            log_path = os.path.join(HOST_ROOT_LOG_DIR, name + ".log")
            if os.path.exists(log_path): #and is_ransom_work(log_path) == True:
                #print(f"Already positive: {log_path}", flush=True)
                continue
            files.append(name)
        print("Sample:", len(files))
        #return
        if len(files) == 0:
            break
        random.shuffle(files)

        manager = Manager()
        ransom_names = manager.list(files)
        mutex = Lock()

        if n_processes == 1:
            # ransom_names = manager.list(["0442cfabb3212644c4b894a?7e4a7e84c00fd23489cc4f96490f9988e6074b6ab", "0d2137d133179a2fbff7bf38a8125d4b74e9615aaa47b5f4a3056eccce7a8f6e"])
            vm_process(f"D:/VM/Windows_10_test_ransom_0/RansomTestWindows10.vmx", f"{RUN_TIME_LOG_DIR}\\runtime_log_0.txt", ransom_names, mutex, max_run_time)
            break
        else:
            processes = []
            for i in range(n_processes):
                vm_path = f"D:/VM/Windows_10_test_ransom_{i}/RansomTestWindows10.vmx"
                runtime_log = f"{RUN_TIME_LOG_DIR}\\runtime_log_{i}.txt"

                p = Process(target=vm_process,
                            args=(vm_path, runtime_log, ransom_names, mutex, max_run_time))
                processes.append(p)
                p.start()

            for p in processes:
                p.join()

        if os.path.exists(STOP_FILE):
            break
        os.system("taskkill /IM vmware-vmx.exe /F")
        time.sleep(10)

if __name__ == "__main__":
    from multiprocessing import freeze_support
    freeze_support()
    main()
