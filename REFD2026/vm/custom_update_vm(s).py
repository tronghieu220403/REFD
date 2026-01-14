
import os
import hashlib
from multiprocessing import Process, Lock, Manager

def update_fn(i):
     os.system(f"python custom_update_vm.py {i}")

if __name__ == "__main__":

        from multiprocessing import freeze_support
        freeze_support()  # Optional nếu không đóng gói .exe, nhưng nên có

        vms_i = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
        processes = []
        for i in vms_i:
            processes.append(Process(target=update_fn, args=(i,)))
            processes[len(processes) - 1].start()
        for process in processes:
            process.join()