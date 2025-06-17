import os
import shutil

def copy_all_files(src_dir, dst_dir):
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)

    for root, dirs, files in os.walk(src_dir):
        for file in files:
            src_file = os.path.join(root, file)
            dst_file = os.path.join(dst_dir, file)

            # Nếu file đã tồn tại, thêm hậu tố để tránh ghi đè
            if os.path.exists(dst_file):
                base, ext = os.path.splitext(file)
                i = 1
                while True:
                    new_name = f"{base}_{i}{ext}"
                    dst_file = os.path.join(dst_dir, new_name)
                    if not os.path.exists(dst_file):
                        break
                    i += 1

            shutil.copy2(src_file, dst_file)
            print(f"Copied: {src_file} -> {dst_file}")

destination_folder = r"E:\Graduation_Project\clean_combined"

source_folder = r"E:\Graduation_Project\Ransomware Combined Structural Feature Dataset\Executable Files\Goodware\Goodware_Test"
copy_all_files(source_folder, destination_folder)

source_folder = r"E:\Graduation_Project\Ransomware Combined Structural Feature Dataset\Executable Files\Goodware\Goodware_Training"
copy_all_files(source_folder, destination_folder)
