from collections import defaultdict
import os
import re

HOST_ROOT_LOG_DIRS = [
    "D:\\MalwareBazaarRun\\logs_refd",
]

def is_ransom_work(log_path):
    if not os.path.exists(log_path):
        return False
    try:
        with open(log_path, 'r', encoding='utf-16-le', errors="ignore") as f:
            for line in f:
                if "hieunt-" in line.lower():
                    return True
    except Exception:
        pass
    return False

ransom_list = set()
matched_list = set()

for rld in HOST_ROOT_LOG_DIRS:
    for root, dirs, files in os.walk(rld):
        for fn in files:
            path = os.path.join(root, fn)
            ransom_list.add(fn)
            if is_ransom_work(path):
                matched_list.add(fn)

print(f"{len(matched_list)}/{len(ransom_list)} files match")