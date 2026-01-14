
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
vm_path = f"D:\\VM\\Windows_10_test_ransom_0\\RansomTestWindows10.vmx"

host = VixHost()
vm = host.open_vm(vm_path)

login_as_system()
#login_as_hieu()

git_path = "E:\\Code\\Github\\Minerva\\"

collector_path = f'E:\\Code\\Github\\Minerva\\EventCollectorDriver\\x64\\Debug'

def init_env():
    print("Init env")

    print("Shut down services")
    run_cmd("fltmc unload EventCollectorDriver")

    run_cmd("del E:\\hieunt210330\\EventCollectorDriver.sys")
    run_cmd("del C:\\Windows\\System32\\drivers\\EventCollectorDriver.sys")
    run_cmd("del C:\\Windows\\EventCollectorDriver.log")

    print("Copy files to E:\\hieunt210330\\")
    vm.copy_host_to_guest(f'{collector_path}\\EventCollectorDriver.sys', 'E:\\hieunt210330\\EventCollectorDriver.sys')
    vm.copy_host_to_guest(f'{collector_path}\\EventCollectorDriver.sys', 'C:\\Windows\\System32\\drivers\\EventCollectorDriver.sys')
    vm.copy_host_to_guest(f'{collector_path}\\EventCollectorDriver.pdb', 'E:\\hieunt210330\\EventCollectorDriver.pdb')
    #os._exit(0)
    print("Start driver and service")
    
    run_cmd("fltmc load EventCollectorDriver")
init_env()

