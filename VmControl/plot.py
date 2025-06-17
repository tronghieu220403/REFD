import pandas as pd
import matplotlib.pyplot as plt

# Đọc lại dữ liệu từ file đã tải
file_path = "RansomDetectorService1.csv"
df = pd.read_csv(file_path, skiprows=1, nrows=1038, header=None)
df.columns = ["Time", "ProcessorTime", "WorkingSet"]

# Làm sạch dữ liệu
df["ProcessorTime"] = pd.to_numeric(df["ProcessorTime"], errors="coerce")
df["WorkingSet"] = pd.to_numeric(df["WorkingSet"], errors="coerce")
df["WorkingSet_MB"] = df["WorkingSet"] / (1024 * 1024)

# Trung bình tích lũy
df["Avg_ProcessorTime"] = df["ProcessorTime"].expanding().mean()
df["Avg_WorkingSet_MB"] = df["WorkingSet_MB"].expanding().mean()

# Các mốc thời gian (theo chỉ số dòng)
time_points = [60, 120, 300, 600, 900, 1200, 1500, 1800]  # giây
indices = [t // 5 for t in time_points]
x_labels = [f"{t//60} min" for t in time_points]

# Lấy giá trị tại các mốc
avg_processor_at_milestones = df["Avg_ProcessorTime"].iloc[indices].values
avg_workingset_at_milestones = df["Avg_WorkingSet_MB"].iloc[indices].values

# Vị trí và giá trị cực đại
max_proc_idx = df["Avg_ProcessorTime"].idxmax()
max_proc_time = df["Avg_ProcessorTime"].iloc[max_proc_idx]
max_mem_idx = df["Avg_WorkingSet_MB"].idxmax()
max_mem = df["Avg_WorkingSet_MB"].iloc[max_mem_idx]

# Trục x theo số phút từ 0 đến 30 phút (mỗi điểm cách 5s)
x_ticks = list(range(0, len(df), 60))  # mỗi 5 phút (60 dòng)
x_tick_labels = [f"{(i*5)//60} min" for i in x_ticks]

# Vẽ và lưu ảnh
plt.figure(figsize=(14, 6))
plt.plot(df["Avg_ProcessorTime"], label="Avg % Processor Time")
plt.plot(df["Avg_WorkingSet_MB"], label="Avg Working Set (MB)")
plt.plot(max_proc_idx, max_proc_time, 'ro', label="Max CPU")
plt.plot(max_mem_idx, max_mem, 'o', color='orange', label="Max Mem")
plt.xticks(ticks=x_ticks, labels=x_tick_labels, rotation=45)
plt.xlabel("Time (Minutes)")
plt.ylabel("Value")
plt.title("Average Processor and Memory Usage During Peak File I/O")
plt.legend()
plt.grid(True)
plt.tight_layout()

# Xuất ảnh
output_path = "processor_memory_usage_peak.png"
plt.savefig(output_path)

'''
# Đọc toàn bộ dữ liệu từ file đã upload
file_path = "RansomDetectorService2.csv"
df_full = pd.read_csv(file_path, skiprows=1, header=None)
df_full.columns = ["Time", "ProcessorTime", "WorkingSet"]

# Cắt phần từ dòng 1039 đến hết (dữ liệu bình thường)
df = df_full.iloc[1039:].reset_index(drop=True)

# Làm sạch dữ liệu
df["ProcessorTime"] = pd.to_numeric(df["ProcessorTime"], errors="coerce")
df["WorkingSet"] = pd.to_numeric(df["WorkingSet"], errors="coerce")
df["WorkingSet_MB"] = df["WorkingSet"] / (1024 * 1024)

# Tính trung bình tích lũy
df["Avg_ProcessorTime"] = df["ProcessorTime"].expanding().mean()
df["Avg_WorkingSet_MB"] = df["WorkingSet_MB"].expanding().mean()

# Tìm vị trí và giá trị cực đại
max_proc_idx = df["Avg_ProcessorTime"].idxmax()
max_proc_time = df["Avg_ProcessorTime"].iloc[max_proc_idx]
max_mem_idx = df["Avg_WorkingSet_MB"].idxmax()
max_mem = df["Avg_WorkingSet_MB"].iloc[max_mem_idx]

# Nhãn trục x mỗi 5 phút (60 dòng mỗi 5 phút)
x_ticks = list(range(0, len(df), 60))
x_tick_labels = [f"{(i * 5) // 60} min" for i in x_ticks]

# Vẽ và lưu ảnh
plt.figure(figsize=(14, 6))
plt.plot(df["Avg_ProcessorTime"], label="Avg % Processor Time")
plt.plot(df["Avg_WorkingSet_MB"], label="Avg Working Set (MB)")
plt.plot(max_proc_idx, max_proc_time, 'ro', label="Max CPU")
plt.plot(max_mem_idx, max_mem, 'o', color='orange', label="Max Mem")
plt.xticks(ticks=x_ticks, labels=x_tick_labels, rotation=45)
plt.xlabel("Time (Minutes)")
plt.ylabel("Value")
plt.title("Average Processor and Memory Usage During Normal Operation")
plt.legend()
plt.grid(True)
plt.tight_layout()

# Lưu ảnh
output_path = "normal_processor_memory_usage.png"
plt.savefig(output_path)
'''
