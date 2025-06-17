import pandas as pd
import matplotlib.pyplot as plt

def export_png(input_str, output_str):

    # Đọc lại dữ liệu từ file đã tải
    file_path = input_str
    df = pd.read_csv(file_path, skiprows=1, nrows=1038, header=None)
    df.columns = ["Time", "ProcessorTime", "WorkingSet"]

    # Làm sạch dữ liệu
    df["ProcessorTime"] = pd.to_numeric(df["ProcessorTime"], errors="coerce")
    df["WorkingSet"] = pd.to_numeric(df["WorkingSet"], errors="coerce")
    df["WorkingSet_MB"] = df["WorkingSet"] / (1024 * 1024)

    # Trung bình tích lũy
    df["Avg_ProcessorTime"] = df["ProcessorTime"].expanding().mean()
    df["Avg_WorkingSet_MB"] = df["WorkingSet_MB"].expanding().mean()

    # Vị trí và giá trị cực đại
    max_proc_idx = df["Avg_ProcessorTime"].idxmax()
    max_proc_time = df["Avg_ProcessorTime"].iloc[max_proc_idx]
    max_mem_idx = df["Avg_WorkingSet_MB"].idxmax()
    max_mem = df["Avg_WorkingSet_MB"].iloc[max_mem_idx]

    # Trục x theo số phút từ 0 đến 30 phút (mỗi điểm cách 5s)
    x_ticks = list(range(0, len(df), 120))  # mỗi 10 phút (120 dòng)
    x_tick_labels = [f"{(i*5)//60} min" for i in x_ticks]

    # Vẽ và lưu ảnh với font chữ lớn
    plt.figure(figsize=(14, 6))
    plt.plot(df["Avg_ProcessorTime"], label="Avg % Processor Time (max 400%)", linewidth=2)
    plt.plot(df["Avg_WorkingSet_MB"], label="Avg Working Set (MB)", linewidth=2)
    plt.plot(max_proc_idx, max_proc_time, 'ro', label="Max CPU")
    plt.plot(max_mem_idx, max_mem, 'o', color='orange', label="Max Mem")

    plt.xticks(ticks=x_ticks, labels=x_tick_labels, rotation=45, fontsize=16)
    plt.yticks(fontsize=16)
    plt.xlabel("Time (Minutes)", fontsize=18)
    plt.ylabel("Value", fontsize=18)
    #plt.title("Average Processor and Memory Usage During Peak File I/O", fontsize=20)
    plt.legend(fontsize=16)
    plt.grid(True)
    plt.tight_layout()

    # Xuất ảnh
    output_path = output_str
    plt.savefig(output_path)

#export_png("RansomDetectorService1.csv", "processor_memory_usage_peak.png")
export_png("RansomDetectorService_normal.csv", "normal_processor_memory_usage.png")
