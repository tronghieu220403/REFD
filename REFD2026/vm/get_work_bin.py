import os
import shutil

# ================= CONFIG =================
HOST_ROOT_LOG_DIRS = [
    r"D:\MalwareBazaarRun\logs_refd",
]

MALWARE_SRC_DIR = r"D:\MalwareBazaarFinal"
MALWARE_DST_DIR = r"D:\MalwareBazaarWork"

LOG_ENCODING = "utf-16-le"
KEYWORD = "hieunt-"
# ==========================================

os.makedirs(MALWARE_DST_DIR, exist_ok=True)


def is_ransom_work(log_path: str) -> bool:
    if not os.path.isfile(log_path):
        return False
    try:
        with open(log_path, "r", encoding=LOG_ENCODING, errors="ignore") as f:
            for line in f:
                if KEYWORD in line.lower():
                    return True
    except Exception:
        pass
    return False


def extract_sha256_from_log(filename: str) -> str | None:
    # Expect: <sha256>.log
    if not filename.lower().endswith(".log"):
        return None
    sha256 = filename[:-4]
    if len(sha256) == 64:
        return sha256.lower()
    return None


copied = 0
missing = 0
seen = set()

for root_dir in HOST_ROOT_LOG_DIRS:
    for root, _, files in os.walk(root_dir):
        for fn in files:
            log_path = os.path.join(root, fn)

            if not is_ransom_work(log_path):
                continue

            sha256 = extract_sha256_from_log(fn)
            if not sha256 or sha256 in seen:
                continue

            seen.add(sha256)

            src_file = os.path.join(MALWARE_SRC_DIR, sha256)
            dst_file = os.path.join(MALWARE_DST_DIR, sha256)

            if not os.path.isfile(src_file):
                missing += 1
                print(f"[!] Missing malware file: {sha256}")
                continue

            try:
                shutil.copy2(src_file, dst_file)
                copied += 1
                print(f"[+] Copied: {sha256}")
            except Exception as e:
                print(f"[!] Copy failed {sha256}: {e}")

print("===================================")
print(f"[+] Copied malware files : {copied}")
print(f"[!] Missing malware files: {missing}")
print(f"[+] Total unique ransom logs: {len(seen)}")
