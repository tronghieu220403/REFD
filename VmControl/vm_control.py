
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

vm_path = r'C:\Users\hieunt118\Documents\Virtual Machines\Windows 10 x64 22h2\Windows 10 x64 22h2.vmx'

if os.path.exists(vm_path) == False:
    vm_path = r'E:\\VM\\Windows_10_test_ransom_0\\RansomTestWindows10.vmx'

host = VixHost()
vm = host.open_vm(vm_path)

#vm.snapshot_revert(vm.snapshot_get_current())

vm.power_on(True)

vm.wait_for_tools()

login_as_system()

from pathlib import Path

git_path = str(Path.cwd().parent)

env_path = f"{git_path}\\guest_env"
collector_path = f'{git_path}\\Event-Collector-Driver'
sd_path = f'{git_path}\\SelfDefenseKernel'
def init_env():
    print("Init env")
    try:
        vm.copy_host_to_guest(f'{env_path}\\start_collector_driver.bat', 'E:\\start_collector_driver.bat')
        vm.copy_host_to_guest(f'{env_path}\\stop_collector_driver.bat', 'E:\\stop_collector_driver.bat')
        vm.copy_host_to_guest(f'{env_path}\\start_sd_driver.bat', 'E:\\start_sd_driver.bat')
        vm.copy_host_to_guest(f'{env_path}\\stop_sd_driver.bat', 'E:\\stop_sd_driver.bat')

        pass
    except Exception as e:
        pass
    try:
        vm.create_directory('E:\\backup')
        pass
    except VixError as e:
        pass
    try:
        vm.create_directory('E:\\hieunt210330')
    except VixError as e:
        pass
    try:
        vm.create_directory('E:\\test')
    except VixError as e:
        pass
    print("Shut down services")
    run_cmd("del /f C:\\Users\\hieu\\Documents\\ggez.txt")
    run_cmd("del /f E:\\ggez.txt")
    run_cmd("copy nul C:\\Users\\hieu\\Documents\\ggez.txt > nul")
    run_cmd("copy nul E:\\ggez.txt > nul")

    #time.sleep(5)
    run_cmd('E:\\stop_collector_driver.bat')
    run_cmd('E:\\stop_sd_driver.bat')

    run_cmd("del /f C:\\Users\\hieu\\Documents\\ggez.txt")
    run_cmd("del /f E:\\ggez.txt")

    #print("Delete files in C:\\Users\\hieu\\Downloads\\AAAANapierOne-tiny and E:\\backup")
    #run_cmd("powershell -Command \"Remove-Item \'C:\\Users\\hieu\\Downloads\\AAAANapierOne-tiny\\*\' -Recurse -Force\"")
    #run_cmd("powershell -Command \"Remove-Item \'E:\\test\\*\' -Recurse -Force\"")
    #run_cmd("powershell -Command \"Remove-Item \'E:\\backup\\*\' -Recurse -Force\"")

    print("Copy files to C:\\Users\\hieu\\Downloads\\TestIo.exe")
    try:
        #run_cmd("del /f C:\\Users\\hieu\\Downloads\\TestIo.exe")
        #vm.copy_host_to_guest(f"{git_path}\\TestIo\\x64\\Debug\\TestIo.exe", 'C:\\Users\\hieu\\Downloads\\TestIo.exe')
        pass
    except Exception as e:
        pass
    #run_cmd("E:\\TestIo.exe c")
    #run_cmd(f'xcopy "C:\\Users\\hieu\\Downloads\\test" "E:\\test" /E /I /Y')
    #run_cmd(f'xcopy "C:\\Users\\hieu\\Downloads\\AAAANapierOne-tiny-backup" "C:\\Users\\hieu\\Downloads\\AAAANapierOne-tiny" /E /I /Y')
    #run_cmd(f'xcopy "C:\\Users\\hieu\\Downloads\\AAAANapierOne-tiny-backup" "E:\\test" /E /I /Y')

    #vm.copy_host_to_guest(f'{env_path}\\TrIDLib.dll', 'E:\\hieunt210330\\TrIDLib.dll')
    #vm.copy_host_to_guest(f'{env_path}\\triddefs.trd', 'E:\\hieunt210330\\triddefs.trd')

    print("Copy driver files to E:\\")
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.inf', 'E:\\EventCollectorDriver.inf')
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.sys', 'E:\\EventCollectorDriver.sys')
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.pdb', 'E:\\EventCollectorDriver.pdb')
    vm.copy_host_to_guest(f'{collector_path}\\x64\\Debug\\EventCollectorDriver.pdb', 'C:\\Windows\\System32\\drivers\\EventCollectorDriver.pdb')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.inf', 'E:\\SelfDefenseKernel.inf')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.sys', 'E:\\SelfDefenseKernel.sys')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.pdb', 'E:\\SelfDefenseKernel.pdb')
    vm.copy_host_to_guest(f'{sd_path}\\x64\\Debug\\SelfDefenseKernel.pdb', 'C:\\Windows\\System32\\drivers\\SelfDefenseKernel.pdb')
    #os._exit(0)
    while True:
        try:
            vm.copy_host_to_guest(f'{git_path}\\RansomDetectorService\\Debug\\RansomDetectorService.exe', 'E:\\hieunt210330\\RansomDetectorService.exe')
            vm.copy_host_to_guest(f'{git_path}\\RansomDetectorService\\Debug\\RansomDetectorService.pdb', 'E:\\hieunt210330\\RansomDetectorService.pdb')
            pass
        except Exception as e:
            print("Error copying files, retrying...", e)
            time.sleep(1)
            continue
        break

    #os._exit(0)
    print("Start driver and service")

    run_cmd("E:\\start_sd_driver.bat")
    run_cmd("E:\\start_collector_driver.bat")
    run_cmd("E:\\hieunt210330\\RansomDetectorService.exe", False)

init_env()

#time.sleep(5)

print("Start testing")

run_cmd("C:\\Users\\hieu\\Downloads\\muestra.zip\\muestra\\wetransfer-29ee54\\MsMpEng.exe")

print("Test stop")

#time.sleep(30)

print("Test done")

#run_cmd("copy nul C:\\Users\\hieu\\Documents\\ggez.txt > nul")
#run_cmd("copy nul E:\\ggez.txt > nul")

#run_cmd('E:\\stop_collector_driver.bat')
#run_cmd('E:\\stop_sd_driver.bat')
