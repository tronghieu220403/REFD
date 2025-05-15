
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
    vm.proc_run("C:\\Windows\\System32\\cmd.exe", '/c "' + cmd + '"', wait)

vm_path = r'E:\\VM\\Windows_10_test_ransom_0\\RansomTestWindows10.vmx'
host = VixHost()
vm = host.open_vm(vm_path)

vm.power_on()

vm.wait_for_tools()

login_as_system()

def init_env():
    print("Init env")
    try:
        vm.create_directory('E:\\backup')
    except VixError as e:
        pass
    try:
        vm.create_directory('E:\\hieunt210330')
    except VixError as e:
        pass
    print("Shut down service")
    run_cmd("del /f C:\\Users\\hieu\\Documents\\ggez.txt")
    run_cmd("copy nul C:\\Users\\hieu\\Documents\\ggez.txt > nul")
    #time.sleep(5)
    run_cmd('E:\\stop_driver.bat')

    print("Copy files")
    '''
    vm.copy_host_to_guest('E:\\hieunt20210330\\TrID\\TrIDLib.dll', 'E:\\hieunt210330\\TrIDLib.dll')
    vm.copy_host_to_guest('E:\\hieunt20210330\\TrID\\triddefs.trd', 'E:\\hieunt210330\\triddefs.trd')
    vm.copy_host_to_guest('E:\\Code\\Github\\REFD\\Event-Collector-Driver\\x64\\Debug\\EventCollectorDriver.sys', 'E:\\EventCollectorDriver.sys')
    vm.copy_host_to_guest('E:\\Code\\Github\\REFD\\Event-Collector-Driver\\x64\\Debug\\EventCollectorDriver.pdb', 'E:\\EventCollectorDriver.pdb')
    '''
    while True:
        try:
            vm.copy_host_to_guest("E:\\Code\\Github\\REFD\\TestIo\\x64\\Debug\\TestIo.exe", 'E:\\TestIo.exe')
            vm.copy_host_to_guest('E:\\Code\\Github\\REFD\\RansomDetectorService\\Debug\\RansomDetectorService.exe', 'E:\\hieunt210330\\RansomDetectorService.exe')
            vm.copy_host_to_guest('E:\\Code\\Github\\REFD\\RansomDetectorService\\Debug\\RansomDetectorService.pdb', 'E:\\hieunt210330\\RansomDetectorService.pdb')
        except Exception as e:
            print("Error copying files, retrying...", e)
            continue
        break
    
    print("Start driver and service")
    run_cmd("del /f C:\\Users\\hieu\\Documents\\ggez.txt")

    run_cmd("E:\\start_driver.bat")
    run_cmd("E:\\hieunt210330\\RansomDetectorService.exe", False)

init_env()

print("Start testing")

time.sleep(2)

for i in range(1):

    test_cmdl = "E:\\TestIo.exe d 9 10"

    run_cmd(test_cmdl)
    time.sleep(1)

time.sleep(30)
print("Test done")

run_cmd("copy nul C:\\Users\\hieu\\Documents\\ggez.txt > nul")
run_cmd('E:\\stop_driver.bat')
