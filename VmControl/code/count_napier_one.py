import os
from collections import defaultdict

def get_file_types_stats(root_dir):
    file_stats = defaultdict(lambda: {"count": 0, "size_bytes": 0})

    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            file_path = os.path.join(dirpath, filename)
            try:
                ext = os.path.splitext(filename)[1].lower() or "<no_extension>"
                size = os.path.getsize(file_path)
                file_stats[ext]["count"] += 1
                file_stats[ext]["size_bytes"] += size
            except Exception as e:
                print(f"Error accessing {file_path}: {e}")

    return file_stats

def print_stats(file_stats):
    print(f"{'Extension':<15} {'Count':<10} {'Total Size (MB)':<20}")
    print("-" * 45)
    cnt_i = 0
    total_cnt = 0
    total_size = 0
    for ext, stats in sorted(file_stats.items()):
        size_kb = stats["size_bytes"] // (1024)
        cnt_i += 1
        print(f"{cnt_i} & {(ext[1:]).upper()} & {stats['count']} & {size_kb:,} \\\\")
        total_cnt += stats["count"]
        total_size += stats["size_bytes"]
    
    print(f'{total_cnt} & {total_size // (1024):,}')

if __name__ == "__main__":
    root_directory = "E:\\Graduation_Project\\napier_dataset\\AAAANapierOne-tiny\\"
    stats = get_file_types_stats(root_directory)
    print_stats(stats)