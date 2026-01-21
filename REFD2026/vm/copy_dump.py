
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

def login_as_system():
    print("Login as SYSTEM", flush=True)
    vm.login('hieu','1', False)
    print("Logged as SYSTEM", flush=True)

def run_cmd(cmd: str, wait: bool = True):
    print("Run cmd: ", cmd)
    vm.proc_run("C:\\Windows\\System32\\cmd.exe", '/c "' + cmd + '"', wait)

vm_path = f"D:\\VM\\Windows_10_test_ransom_0\\RansomTestWindows10.vmx"

host = VixHost()
vm = host.open_vm(vm_path)

login_as_system()

tmp_path = f'E:\\tmp'
COLLECTOR_DRIVER_PATH = f'E:\\Code\\Github\\REFD\\EventCollectorDriver\\x64\\Debug'

def init_env():
    try:
        shutil.rmtree(tmp_path)
        os.mkdir(tmp_path)
    except:
        pass
    vm.copy_guest_to_host('C:\\Windows\\Minidump', f'{tmp_path}\\Minidump')
    shutil.copyfile(f"{COLLECTOR_DRIVER_PATH}\\EventCollectorDriver.sys", f'{tmp_path}\\EventCollectorDriver.sys')
    shutil.copyfile(f"{COLLECTOR_DRIVER_PATH}\\EventCollectorDriver.pdb", f'{tmp_path}\\EventCollectorDriver.pdb')
init_env()

