
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

# Save original print
original_print = builtins.print

# Replace built-in print with our version
builtins.print = timestamped_print

def login_as_hieu():
    print("Login as hieu")
    vm.login('hieu','1', True)
    print("Logged as hieu", flush=True)

def login_as_system():
    print("Login as SYSTEM", flush=True)
    vm.login('hieu','1', False)
    print("Logged as SYSTEM", flush=True)

def run_cmd(cmd: str, wait: bool = True):
    print("Run cmd: ", cmd)
    vm.proc_run("C:\\Windows\\System32\\cmd.exe", '/c "' + cmd + '"', wait)

x = 0

if __name__ == "__main__":
    # Get args
    if len(sys.argv) > 1:
        x = int(sys.argv[1]) 

vm_path = f"D:\\VM\\Windows_10_test_ransom_{x}\\RansomTestWindows10.vmx"

host = VixHost()
vm = host.open_vm(vm_path)

git_path = "E:\\Code\\Github\\REFD\\"

COLLECTOR_PATH = f'E:\\Code\\Github\\REFD\\EventCollectorDriver\\x64\\Debug'

def revert_snap():
    # Back to previous snapshot
    cur_snap = vm.snapshot_get_current()
    vm.snapshot_revert(cur_snap)

def do_snap():
    vm.create_snapshot("Minverva")

def init_env():
    
    revert_snap()

    login_as_system()

    print("Init env")

    print("Shut down services")
    run_cmd("type nul > \"C:\\Users\\hieu\\Downloads\\ggez.txt\"", False)
    run_cmd("fltmc unload SelfDefenseKernel", False)
    run_cmd("fltmc unload EventCollectorDriver", False)
    run_cmd("del C:\\Users\\hieu\\Downloads\\ggez.txt", False)
    run_cmd("sc control REFD 129", False)
    run_cmd("sc stop REFD", False)

    run_cmd("del E:\\hieunt210330\\EventCollectorDriver.sys", False)
    run_cmd("del C:\\Windows\\System32\\drivers\\EventCollectorDriver.sys", False)
    run_cmd("del E:\\hieunt210330\\SelfDefenseKernel.sys", False)
    run_cmd("del C:\\Windows\\System32\\drivers\\SelfDefenseKernel.sys", False)
    run_cmd("del E:\\hieunt210330\\hieunt210330\\RansomDetectorService.exe", False)
    run_cmd("del E:\\hieunt210330\\hieunt210330\\log.txt", False)
    run_cmd("del C:\\Windows\\EventCollectorDriver.log", False)

    run_cmd("reg add \"HKLM\\SYSTEM\\ControlSet001\\Services\\EventCollectorDriver\" /v Start /t REG_DWORD /d 3 /f")

    print("Copy files to E:\\hieunt210330\\")
    vm.copy_host_to_guest(f'{COLLECTOR_PATH}\\EventCollectorDriver.pdb', 'E:\\hieunt210330\\EventCollectorDriver.pdb')
    vm.copy_host_to_guest(f'{COLLECTOR_PATH}\\EventCollectorDriver.sys', 'E:\\hieunt210330\\EventCollectorDriver.sys')
    vm.copy_host_to_guest(f'{COLLECTOR_PATH}\\EventCollectorDriver.sys', 'C:\\Windows\\System32\\drivers\\EventCollectorDriver.sys')

    do_snap()
    vm.power_off()

init_env()