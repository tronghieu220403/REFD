
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

vm_path = "E:\\VM\\Windows 10 and later x64\\Windows 10 and later x64.vmx"

host = VixHost()
vm = host.open_vm(vm_path)

login_as_system()
#login_as_hieu()

COLLECTOR_SERVICE_PATH = f'E:\\Code\\Github\\REFD\\RansomDetectorService\\x64\\Release'

def init_env():
    print("Init env")

    print("Shut down services")
    run_cmd("sc stop REFD")
    run_cmd("taskkill /IM RansomDetectorService.exe /F")
    print("Copy files")
    run_cmd("del C:\\tmp\\RansomDetectorService.exe")
    vm.copy_host_to_guest(f'{COLLECTOR_SERVICE_PATH}\\RansomDetectorService.exe', 'C:\\tmp\\RansomDetectorService.exe')
    # vm.copy_host_to_guest(f'{COLLECTOR_SERVICE_PATH}\\RansomDetectorService.pdb', 'C:\\tmp\\RansomDetectorService.pdb')

    print("Start driver and service")
    
    run_cmd("C:\\tmp\\RansomDetectorService.exe")

init_env()

