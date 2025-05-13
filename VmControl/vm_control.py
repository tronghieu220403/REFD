
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

def run_cmd(cmd: str):
    vm.proc_run("C:\\Windows\\System32\\cmd.exe", '/c "' + cmd + '"', True)

vm_path = r'E:\\VM\\Windows_10_test_ransom_0\\RansomTestWindows10.vmx'
host = VixHost()
vm = host.open_vm(vm_path)

vm.power_on()

vm.wait_for_tools()

login_as_system()

def init_env():

    run_cmd(r'E:\stop_driver.bat')
    
    vm.copy_host_to_guest(r"E:\Code\VS2022\TestIo\x64\Debug\TestIo.exe", r'E:\TestIo.exe')
    vm.copy_host_to_guest(r'E:\Code\VS2022\Event-Collector-Driver\x64\Debug\EventCollectorDriver.sys', r'E:\EventCollectorDriver.sys')
    vm.copy_host_to_guest(r'E:\Code\VS2022\Event-Collector-Driver\x64\Debug\EventCollectorDriver.pdb', r'E:\EventCollectorDriver.pdb')

    run_cmd(r'E:\start_driver.bat')

init_env()

for i in range(10000):

    test_cmdl = "E:\\TestIo.exe d c 1 2 3 4 5 6 7 8 9 10"

    run_cmd(test_cmdl)

    #time.sleep(5)

    #run_cmd(r'E:\start_driver.bat')

#run_cmd(r'E:\stop_driver.bat')
