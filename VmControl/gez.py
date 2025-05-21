import sys
import os
import xlsxwriter
import random
import string

def random_string(length=20):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def generate_excel_to_size(filename, target_size_bytes):
    workbook = xlsxwriter.Workbook(filename)
    worksheet = workbook.add_worksheet()

    row = 0
    check_interval = 100  # kiểm tra mỗi 100 dòng

    while True:
        # Ghi thêm dòng dữ liệu
        for _ in range(check_interval):
            data = [random_string() for _ in range(10)]
            worksheet.write_row(row, 0, data)
            row += 1

        # Tạm đóng workbook để kiểm tra file size
        workbook.close()
        current_size = os.path.getsize(filename)
        print(f"[INFO] Dòng: {row} – Size: {current_size / 1024 / 1024:.2f} MB")

        if current_size >= target_size_bytes:
            print(f"[DONE] File đạt kích thước yêu cầu: {current_size / 1024 / 1024:.2f} MB")
            return

        # Nếu chưa đủ size, reopen workbook và tiếp tục
        workbook = xlsxwriter.Workbook(filename)
        worksheet = workbook.add_worksheet()
        worksheet.set_column(0, 9, 20)  # chỉnh column width để tránh lỗi định dạng
        for i in range(row):
            worksheet.write_row(i, 0, [random_string() for _ in range(10)])

# Entry
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py <target_size_in_MB>")
        sys.exit(1)

    try:
        target_mb = float(sys.argv[1])
        target_bytes = int(target_mb * 1024 * 1024)
    except ValueError:
        print("Tham số phải là số (MB).")
        sys.exit(1)

    generate_excel_to_size("output.xlsx", target_bytes)
