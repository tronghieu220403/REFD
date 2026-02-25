# Thiết kế vector đặc trưng hành vi thao tác file theo PID

## Khung vấn đề và taxonomy nhóm đặc trưng

Bài toán của tôi là **phân loại nhị phân (malware vs benign)** dựa **duy nhất** trên luồng sự kiện thao tác file của **một tiến trình đơn (một PID)** trong một **cửa sổ thời gian trượt**. So với nhiều hướng nghiên cứu “behavioral malware detection” kinh điển, đây là một bối cảnh **thông tin cực hạn chế**: không có read/open/close, không có bytes/size, không có entropy nội dung, không có trạng thái thành công/thất bại, không có tên tiến trình/cha-con, không có user/session. Điều này loại bỏ trực tiếp một số chỉ báo rất mạnh thường dùng trong phát hiện ransomware dựa trên I/O (ví dụ: so sánh entropy dữ liệu đọc/ghi, mẫu read→encrypt→overwrite) như trong UNVEIL/CryptoDrop.

Vì vậy, mục tiêu thiết kế cần chuyển từ “nhận biết nội dung bị mã hóa” sang “nhận biết **hình thái hành vi (shape)** của tiến trình khi thao tác file”: cường độ, phân bố theo thời gian, mức độ lan rộng theo namespace, cấu trúc rename, và đặc biệt là **ngữ nghĩa đường dẫn (path semantics)** (ví dụ đụng đến User Documents/ProgramData/Temp/System). Các nghiên cứu về ransomware đều nhấn mạnh rằng để tấn công thành công, ransomware **phải can thiệp vào file của nạn nhân** (write/overwrite/delete/rename) với hành vi lặp đi lặp lại trên nhiều file, tạo ra dấu vết I/O đặc trưng—dù cơ chế mã hóa có thể khác nhau.

Dựa trên ràng buộc schema, một taxonomy thực dụng (2–8 nhóm) nên bao phủ cả tín hiệu “thô” (volume/rate) lẫn tín hiệu “cấu trúc” (entropy/đa dạng/chuỗi/rename) để chống false positive và tránh bị né tránh bằng nhiễu. Ngoài ra, cần lưu ý tính **dịch chuyển phân phối** giữa môi trường thu thập dữ liệu và môi trường triển khai (sandbox vs endpoint, hoặc endpoint giữa các máy khác nhau), vốn được chỉ ra là nguyên nhân làm suy giảm mạnh hiệu năng của nhiều detector hành vi trong thực tế.

### Taxonomy đề xuất

- **Nhóm A — Cường độ & phối trộn op_type:** đếm/tỷ lệ Create/Write/Delete/Rename, entropy của op mix. Đây là nền cho gần như mọi detector I/O ransomware vì ransomware thường tạo ra “khối lượng sửa đổi” bất thường.
- **Nhóm B — Động học thời gian (temporal dynamics):** inter-arrival time, burstiness, độ tập trung theo time-bin, tỷ lệ khoảng lặng. Ransomware thường chạy theo “đợt” với cường độ dày, còn nhiều benign có nhịp khác (hoặc burst nhưng gắn với thư mục/định dạng đặc thù). Các thước đo burstiness/memory trong chuỗi sự kiện được nghiên cứu rộng rãi trong mô hình hóa hành vi theo thời gian.
- **Nhóm C — Ngữ nghĩa đường dẫn & mục tiêu:** phân loại vùng hệ thống vs Program Files/ProgramData vs user data vs AppData/Temp vs UNC/network share; nhóm extension (doc-like/executable-like). Windows “Known Folders” và biến môi trường chuẩn cung cấp cách gắn hoạt động file vào ý nghĩa người dùng/hệ thống.
- **Nhóm D — Đa dạng & tập trung theo namespace:** số lượng file/dir/extension khác nhau, entropy phân bố theo thư mục/extension, Gini (tập trung trên vài file hay lan rộng). UNVEIL và các phân tích ransomware đều quan sát traversal qua rất nhiều thư mục/ổ đĩa và thao tác lặp trên nhiều file.
- **Nhóm E — Cấu trúc chuỗi (sequential structure):** xác suất chuyển trạng thái (Create→Write, Write→Rename, Write→Delete…), độ “bám” cùng thư mục/file giữa các sự kiện liên tiếp, độ dài run. Mô hình chuỗi/đồ thị hành vi từ system call/I/O là một hướng lâu đời; đồng thời đã được chỉ ra rằng đặc trưng chuỗi thuần túy dễ bị né tránh bằng chèn nhiễu, nên cần phối hợp với đặc trưng phân bố/namespace.
- **Nhóm F — Đặc trưng Rename & nhân bản tên file:** tỷ lệ đổi extension, mức “append suffix”, độ giống tiền tố tên file cũ/mới, rename cùng thư mục, và “tạo cùng một filename ở nhiều thư mục” (dấu hiệu ransom note). Nhiều ransomware có hành vi append extension và/hoặc tạo note ở nhiều thư mục; ngay cả khi ransomware cố né bằng thay extension ngẫu nhiên, các chỉ báo cấu trúc khác vẫn hỗ trợ.

## Đặc tả master feature vector

### Ký hiệu và tiền xử lý chuẩn

Xét một cửa sổ thời gian \(W=[T_s, T_e)\) có độ dài \(\Delta = T_e-T_s\) (tính theo giây). Lọc các sự kiện thuộc **một PID** cần biểu diễn:

- Mỗi sự kiện \(e_i = (t_i, o_i, p_i)\), với:
  - \(t_i\): timestamp
  - \(o_i \in \{C,W,D,R\}\) tương ứng Create, Write/Modify, Delete, Rename
  - \(p_i\): full_path (đường dẫn đầy đủ)
  - Nếu \(o_i=R\), có thêm \(p_i^{new}\): full_path_new

Sắp xếp các sự kiện theo thời gian tăng dần: \(t_1 \le \dots \le t_N\). Đặt:
- \(N\): tổng số sự kiện trong cửa sổ.
- \(\varepsilon = 10^{-9}\) để tránh chia cho 0.
- Hàm chuẩn hóa đường dẫn \(norm(\cdot)\): lowercase, chuẩn hóa separator về `\` hoặc `/` thống nhất, loại bỏ `\\?\` nếu có, rút gọn lặp separator (tất cả chỉ dựa trên chuỗi).
- \(dir(p)\): thư mục chứa file (loại bỏ segment cuối).
- \(fname(p)\): tên file (segment cuối).
- \(ext(p)\): phần mở rộng (ký tự sau dấu `.` cuối cùng trong \(fname\); nếu không có thì rỗng).
- \(depth(p)\): số lượng segment thư mục (không tính drive letter/UNC root).

Với các đặc trưng theo **nhóm extension**, định nghĩa hàm \(g(ext)\in\{\text{doc},\text{exe},\text{archive},\text{media},\text{image},\text{code},\text{other}\}\) theo danh sách đuôi mở rộng do tôi cố định (ví dụ doc: doc/docx/xls/xlsx/ppt/pptx/pdf/txt/rtf/odt/…; exe: exe/dll/sys/scr/…).

Với các đặc trưng theo **vùng đường dẫn Windows**, dùng luật match theo prefix (không cần username cụ thể), ví dụ:
- `C:\Windows\` (system)
- `C:\Program Files\`, `C:\Program Files (x86)\`, `C:\ProgramData\` (program)
- `C:\Users\<*>\Documents\`, `C:\Users\<*>\Desktop\`, `C:\Users\<*>\Downloads\`… (user_data)
- ...

### Nhóm A — Cường độ & phối trộn op_type

**f_total_events — Tổng số sự kiện (int)**  
Định nghĩa: \(f\_total\_events = N\).  
(A) Cơ chế phân biệt: Nhiều lớp malware, đặc biệt ransomware, tạo ra **khối lượng thao tác file cao** khi can thiệp hàng loạt file nạn nhân; các nghiên cứu I/O-based ransomware nhấn mạnh đặc trưng “nhiều thao tác I/O trong thời gian ngắn”.
(B) Bổ trợ: Một mình \(N\) dễ false positive (backup/sync/installer). Nó cần đi cùng đặc trưng ngữ nghĩa đường dẫn (Nhóm C) và độ lan rộng (Nhóm D) để phân biệt “tác vụ nặng nhưng hợp lệ” với “tác vụ nặng và phá hoại”.

**f_create_count — Số Create (int)**  
Định nghĩa: \(f\_create\_count = |\{i:o\_i=C\}|\).  
(A) Create nhiều có thể xuất hiện trong dropper (thả nhiều file) hoặc ransomware tạo file note/aux; tuy nhiên benign cũng tạo temp/cache.
(B) Bổ trợ: Create trở nên đáng ngờ khi đồng thời (i) xảy ra trong user_data hoặc Program Files (Nhóm C), (ii) kèm Write/Delete/Rename cao (A/B/D/F), hoặc (iii) “tạo cùng một tên file ở nhiều thư mục” (f_create_filename_replication).

**f_write_count — Số Write/Modify (int)**  
Định nghĩa: \(f\_write\_count = |\{i:o\_i=W\}|\).  
(A) Ransomware file-locker/crypto-ransomware buộc phải ghi/ghi đè dữ liệu trên nhiều file (hoặc tạo file mới rồi ghi), do đó Write là trụ cột của I/O-based detection.
(B) Bổ trợ: Write cũng phổ biến ở benign (browser cache, DB). Kết hợp với “độ lan rộng” (f_unique_file_count–f_ext_entropy), “mục tiêu user_data” (f_user_data_event_count–f_user_data_rename_count), và dấu rename/extension-change (f_rename_ext_change_ratio–f_rename_same_dir_ratio) để giảm nhầm.

**f_delete_count — Số Delete (int)**  
Định nghĩa: \(f\_delete\_count = |\{i:o\_i=D\}|\).  
(A) Delete cao có thể gợi ý wiper hoặc chiến lược “tạo ciphertext rồi xóa bản gốc”; các mô tả hành vi ransomware cũng nêu xóa/đổi tên bản gốc như một cách hoàn tất tấn công.
(B) Bổ trợ: Delete đơn lẻ dễ nhầm với cleanup hợp lệ; mạnh hơn khi đi cùng Write/Rename cao và tập trung vào user_data (f_user_data_delete_count/f_delete_count tương quan theo cơ chế “phá dữ liệu người dùng”).

**f_rename_count — Số Rename (int)**  
Định nghĩa: \(f\_rename\_count = |\{i:o\_i=R\}|\).  
(A) Rename nổi bật trong nhiều kịch bản ransomware: append extension, đổi tên file, hoặc di chuyển để đánh dấu đã xử lý; nhiều mẫu ransomware quan sát được có hành vi đổi extension trên diện rộng.
(B) Bổ trợ: Rename hợp lệ cũng phổ biến (editor lưu tạm, tool batch-rename). Cần kết hợp dấu “đổi extension” (f_rename_ext_change_ratio), “dominant extension mới” (f_rename_dominant_new_ext_ratio), và “cùng lúc Write/Delete tăng” (f_write_count/f_delete_count) để tránh nhầm.

**f_total_event_rate — Tốc độ sự kiện tổng (float)**  
Định nghĩa: \(f\_total\_event\_rate = \frac{N}{\Delta+\varepsilon}\).  
(A) Ransomware thường cố mã hóa nhanh để tối đa hóa thiệt hại trước khi bị chặn; các nghiên cứu quan sát I/O cho thấy tần suất thao tác file/crypto API có thể đạt hàng nghìn/giây trong sandbox.
(B) Bổ trợ: Nếu malware “throttling” để né, f_total_event_rate suy yếu; khi đó các đặc trưng cấu trúc (entropy thư mục, đa ổ đĩa, rename patterns) sẽ bù.

**f_write_ratio — Tỷ lệ Write (float)**  
Định nghĩa: \(f\_write\_ratio = \frac{f\_write\_count}{N+\varepsilon}\).  
(A) Ransomware file-encrypting thường bị chi phối bởi thao tác ghi; do đó tỷ lệ Write trong op mix là tín hiệu mạnh hơn “đếm tuyệt đối” trong bối cảnh cửa sổ có thể ngắn.
(B) Bổ trợ: Dùng cùng f_op_type_entropy (entropy op) và f_user_data_event_count–f_user_data_rename_count (đích user_data) để phân biệt “Write nhiều nhưng vào cache/Temp” với “Write nhiều vào tài liệu”.

**f_delete_ratio — Tỷ lệ Delete (float)**  
Định nghĩa: \(f\_delete\_ratio = \frac{f\_delete\_count}{N+\varepsilon}\).  
(A) Delete cao tương thích với wiper/cleanup phá hoại hoặc chiến lược xóa bản gốc sau khi tạo ciphertext; là một trong các nhóm I/O characteristics thường được xem xét trong I/O-based ransomware detection.
(B) Bổ trợ: Delete cũng là cleanup bình thường; cần nhìn cùng (i) Write trước Delete (f_transition_write_to_delete), (ii) vùng user_data (f_user_data_delete_count), (iii) mức lan rộng (f_unique_file_count–f_dir_entropy).

**f_rename_ratio — Tỷ lệ Rename (float)**  
Định nghĩa: \(f\_rename\_ratio = \frac{f\_rename\_count}{N+\varepsilon}\).  
(A) Append extension trên hàng loạt file sẽ làm tỷ lệ Rename tăng đáng kể; nhiều quan sát cho thấy đổi extension là dấu hiệu gắn với mã hóa. 
(B) Bổ trợ: Nếu ransomware không rename (chỉ overwrite), f_rename_ratio thấp; khi đó f_write_count/f_total_event_rate/f_vmr_10bins (burstiness) và f_doclike_write_count (write_doclike) sẽ “gánh”.

**f_op_type_entropy — Entropy phối trộn op_type (float)**  
Định nghĩa: đặt \(p\_o=\frac{|\{i:o\_i=o\}|}{N+\varepsilon}\) với \(o\in\{C,W,D,R\}\).  
\(f\_op\_type\_entropy = -\sum_{o} p_o \log_2(p_o+\varepsilon)\).  
(A) Entropy thấp nghĩa là tiến trình “chỉ làm một kiểu thao tác” (ví dụ: write-dominant), thường thấy trong pha mã hóa hàng loạt; entropy cao hơn có thể xuất hiện ở workflow benign phức tạp, nhưng cần xem theo đường dẫn.
(B) Bổ trợ: f_op_type_entropy giảm nhiễu cho các count tuyệt đối (f_create_count–f_rename_count) bằng cách mô tả “hình dạng” distribution; kết hợp với f_write_ext_group_entropy/f_ext_entropy (entropy theo extension/dir) để phân biệt loại “đơn điệu nhưng lan rộng” (đáng ngờ) với “đơn điệu nhưng cục bộ” (ví dụ log writer).

### Nhóm B — Động học thời gian

Đặt inter-arrival times \(\tau_i = t_{i+1}-t_i\) cho \(i=1..N-1\) (nếu \(N<2\) thì các đặc trưng dựa trên \(\tau\) lấy 0). Chia cửa sổ thành \(m=10\) time-bin đều nhau, đếm \(n_j\) là số sự kiện trong bin \(j\).

**f_interarrival_mean — Trung bình inter-arrival (float)**  
Định nghĩa: \(f\_interarrival\_mean=\frac{1}{\max(N-1,1)}\sum\_{i=1}^{N-1}\tau\_i\).  
(A) Ransomware thường tạo chuỗi thao tác dày → inter-arrival nhỏ; benign tương tác người dùng thường thưa/gián đoạn hơn. Quan sát về “burst” trong chuỗi sự kiện là nền tảng của nhiều thước đo temporal.
(B) Bổ trợ: Bị yếu khi malware cố chèn delay; khi đó dùng f_vmr_10bins/f_inactivity_bin_fraction (bin-level burstiness & khoảng lặng) và Nhóm C/D (đích & lan rộng) để vẫn bắt được footprint.

**f_interarrival_cv — Hệ số biến thiên inter-arrival (float)**  
Định nghĩa: \(\mu=\text{mean}(\tau),\ \sigma=\text{std}(\tau)\). \(f\_interarrival\_cv=\frac{\sigma}{\mu+\varepsilon}\).  
(A) CV cao thường phản ánh burst xen kẽ khoảng lặng; nhiều tiến trình benign có nhịp đều hơn hoặc theo “chunk” có cấu trúc khác ransomware.
(B) Bổ trợ: CV kết hợp với f_vmr_10bins (VMR) giúp tách “burst do công việc nặng” khỏi “burst do traversal phá hoại”; đi cùng f_user_data_event_count–f_user_data_rename_count để tránh nhầm các app sync hợp lệ.

**f_interarrival_p90 — Bách phân vị 90% inter-arrival (float)**  
Định nghĩa: \(f\_interarrival\_p90 = Q\_{0.9}(\{\tau\_i\})\).  
(A) Nếu tiến trình có nhiều khoảng nghỉ dài xen kẽ burst, Q90 sẽ lớn; ransomware “chạy liên tục” thường có Q90 nhỏ hơn trong pha mã hóa. 
(B) Bổ trợ: f_interarrival_p90 bổ sung cho f_interarrival_mean/f_interarrival_cv bằng cách “nhìn đuôi”; kết hợp với f_half_window_imbalance (mất cân bằng nửa cửa sổ) để bắt kịch bản ransomware bắt đầu muộn trong cửa sổ.

**f_vmr_10bins — Variance-to-mean ratio theo 10 bin (float)**  
Định nghĩa: \(\bar n=\frac{1}{m}\sum\_{j=1}^m n\_j,\ s^2=\frac{1}{m}\sum\_{j=1}^m (n\_j-\bar n)^2\).  
\(f\_vmr\_10bins=\frac{s^2}{\bar n+\varepsilon}\).  
(A) VMR > 1 mô tả clustering/burstiness; các mô hình và thước đo burstiness được dùng rộng rãi để đặc trưng chuỗi sự kiện không-Poisson. 
(B) Bổ trợ: f_vmr_10bins ít nhạy với reorder ở mức nhỏ, và vẫn hoạt động khi N lớn; kết hợp với f_total_event_rate (rate) để phân biệt “nhiều sự kiện đều” vs “nhiều sự kiện dồn cục”.

**f_max_bin_ratio — Tỷ lệ bin dày nhất (float)**  
Định nghĩa: \(f\_max\_bin\_ratio=\frac{\max\_j n\_j}{N+\varepsilon}\).  
(A) Ransomware có thể tạo “đỉnh” khi vào pha mã hóa; benign tương tác người dùng thường ít tạo một đỉnh chiếm tỷ trọng quá lớn trong cửa sổ ngắn. 
(B) Bổ trợ: f_max_bin_ratio tương tác mạnh với f_user_data_event_count–f_user_data_rename_count (nếu đỉnh nằm trong user_data thì đáng ngờ hơn) và với f_temp_event_count (nếu đỉnh nằm trong temp thì có thể là cache benign).

**f_burstiness_B — Burstiness tham số B (float)**  
Định nghĩa (Goh–Barabási): \(B=\frac{\sigma-\mu}{\sigma+\mu+\varepsilon}\) với \(\mu=\text{mean}(\tau),\sigma=\text{std}(\tau)\). \(f\_burstiness\_B=B\).
(A) \(B\to 1\) biểu thị cực bursty; \(B\to -1\) gần đều; ransomware thường có burstiness cao trong pha mã hóa. 
(B) Bổ trợ: B có hiệu ứng “finite-size” khi N nhỏ; nghiên cứu đã bàn về điều này và đề xuất hiệu chỉnh. Vì vậy, trong triển khai nên dùng f_burstiness_B cùng f_vmr_10bins (bin-level) để ổn định.

**f_inactivity_bin_fraction — Tỷ lệ bin trống (float)**  
Định nghĩa: \(f\_inactivity\_bin\_fraction=\frac{|\{j: n\_j=0\}|}{m}\).  
(A) Malware “throttling” để né detector sẽ tạo nhiều khoảng lặng → f_inactivity_bin_fraction tăng; ngược lại ransomware chạy liên tục thì f_inactivity_bin_fraction thấp. 
(B) Bổ trợ: f_inactivity_bin_fraction bù cho các đặc trưng “rate” (f_total_event_rate) bị vô hiệu bởi delay; kết hợp với f_unique_file_count/f_unique_dir_count (vẫn lan rộng dù chậm) để bắt kịch bản né tránh.

**f_half_window_imbalance — Mất cân bằng nửa cửa sổ (float)**  
Định nghĩa: \(N\_1\)=số sự kiện trong \([T\_s, T\_s+\Delta/2)\), \(N\_2\)=số sự kiện trong \([T\_s+\Delta/2,T\_e)\).  
\(f\_half\_window\_imbalance=\frac{|N_1-N_2|}{N+\varepsilon}\).  
(A) Hữu ích khi ransomware “bùng pha” (đầu hoặc cuối cửa sổ) hoặc khi benign có pha chuẩn bị rồi thao tác; UNVEIL mô tả “access patterns” và pha hoạt động rõ rệt trong runtime.
(B) Bổ trợ: f_half_window_imbalance hỗ trợ giảm phụ thuộc vào việc cửa sổ có cắt ngang pha hoạt động hay không; đi cùng f_max_bin_ratio/f_vmr_10bins để mô tả hình thái trong thời gian.

### Nhóm C — Ngữ nghĩa đường dẫn & mục tiêu

Các đặc trưng này dựa trên phân loại đường dẫn theo vùng Windows. Đây là cách “đưa tri thức hệ điều hành” vào features mà không cần metadata khác, phù hợp với gợi ý của tôi về Known Folders. 

**f_user_data_event_count — Số sự kiện trong user_data (int)**  
Định nghĩa: \(f\_user\_data\_event\_count = |\{i: p\_i \in user\_data\}|\), với user\_data ≈ Documents/Desktop/Downloads/Pictures/Music/Videos (và UNC/network share) dưới `\Users\<*>\`.  
(A) Ransomware nhắm vào dữ liệu người dùng nên tương tác mạnh với các thư mục này; UNVEIL nhấn mạnh điều kiện “tamper with user’s files” để tấn công thành công.
(B) Bổ trợ: Kết hợp f_user_data_event_count với entropy theo thư mục (f_dir_entropy) và unique_dirs (f_unique_dir_count) để phân biệt “editor chỉnh một thư mục” với “traversal nhiều thư mục user”.

**f_user_data_write_count — Số Write trong user_data (int)**  
Định nghĩa: \(f\_user\_data\_write\_count = |\{i: o\_i=W \land p\_i \in user\_data\}|\).  
(A) Đây là tín hiệu trực tiếp của “sửa dữ liệu người dùng”; ransomware file locker buộc phải ghi/ghi đè.
(B) Bổ trợ: Nếu benign backup/sync cũng Write nhiều vào user_data, thì rename patterns (f_rename_ext_change_ratio–f_rename_same_dir_ratio) và “dominant extension mới” (f_rename_dominant_new_ext_ratio) giúp giảm nhầm.

**f_user_data_delete_count — Số Delete trong user_data (int)**  
Định nghĩa: \(f\_user\_data\_delete\_count = |\{i: o\_i=D \land p\_i \in user\_data\}|\).  
(A) Delete tài liệu người dùng có thể là wiper hoặc xóa bản gốc sau mã hóa; là dấu hiệu phá hoại hơn so với delete temp.
(B) Bổ trợ: f_user_data_delete_count nên đi cùng f_transition_write_to_delete (Write→Delete) và f_file_event_gini (Gini) để phân biệt “xóa hàng loạt” với “xóa một nhóm nhỏ theo thao tác người dùng”.

**f_user_data_rename_count — Số Rename trong user_data (int)**  
Định nghĩa: \(f\_user\_data\_rename\_count = |\{i: o\_i=R \land p\_i \in user\_data\}|\) (dựa trên **old path**).  
(A) Hành vi đổi tên/append extension trên tài liệu người dùng là mẫu thường gặp; nhiều đo đạc cho thấy phần lớn mẫu ransomware append extension mới khi mã hóa. 
(B) Bổ trợ: f_user_data_rename_count mạnh hơn khi f_rename_ext_change_ratio cao (đổi extension), hoặc f_rename_dominant_new_ext_ratio cao (hầu hết rename ra cùng extension), gợi ý “đánh dấu file đã mã hóa”.

**f_appdata_event_count — Số sự kiện trong AppData (int)**  
Định nghĩa: \(f\_appdata\_event\_count = |\{i: p\_i \in appdata\}|\), appdata ≈ `\Users\<*>\AppData\Roaming\` hoặc `\Users\<*>\AppData\Local\` (không bao gồm Temp).  
(A) Nhiều benign (browser, app) hoạt động mạnh ở AppData; ngược lại, malware/persistence cũng hay thả cấu hình, payload ở đây. Do thiếu process name, f_appdata_event_count giúp phân biệt “hành vi nặng nhưng ở vùng hợp lệ”.
(B) Bổ trợ: f_appdata_event_count không mang tính kết luận; nó bù cho f_total_events/f_write_count bằng “bối cảnh”. Ví dụ cùng mức Write, nếu phần lớn nằm ở AppData/Temp thì ít đáng ngờ hơn so với user_data.

**f_temp_event_count — Số sự kiện trong Temp (int)**  
Định nghĩa: \(f\_temp\_event\_count = |\{i: p\_i \in temp/cache\}|\), temp/cache ≈ `\Users\<*>\AppData\Local\Temp\` (và các vùng cache/tạm tương đương theo taxonomy).
(A) Rất nhiều phần mềm benign ghi temp; ransomware cũng có thể dùng temp staging. Do đó cần đọc như “điểm neo benignness”, không phải “điểm malware”. 
(B) Bổ trợ: Khi f_total_event_rate/f_write_count cao nhưng f_temp_event_count cũng rất cao và f_user_data_write_count thấp, mô hình có thể giảm điểm nghi ngờ; ngược lại, nếu f_temp_event_count thấp nhưng f_user_data_write_count cao, tăng nghi ngờ.

**f_system_event_count — Số sự kiện trong System (int)**  
Định nghĩa: \(f\_system\_event\_count = |\{i: p\_i \in system\}|\), system ≈ \Windows\ và/hoặc \Windows\System32\.  
(A) Sửa file hệ thống thường hiếm đối với app thường, và có thể là dấu hiệu persistence/tampering.
(B) Bổ trợ: Kết hợp với các đặc trưng ngữ cảnh vùng system/program để phân biệt installer hợp lệ (có thể ghi) với hành vi bất thường khác (đặc biệt nếu đồng thời f_user_data_event_count–f_user_data_rename_count cao).

**f_unique_root_count — Số root khác nhau (int)**  
Định nghĩa: root(p) = drive letter (`C:`) hoặc UNC share prefix (`\\server\share`). \(f\_unique\_root\_count = |\{root(p\_i)\}|\).  
(A) Một số ransomware duyệt nhiều ổ (thậm chí A:..Z:) và nhiều thư mục khi mã hóa; số root tăng phản ánh traversal rộng. 
(B) Bổ trợ: f_unique_root_count bị yếu nếu cửa sổ ngắn hoặc malware giới hạn phạm vi; khi đó f_unique_dir_count/f_dir_entropy (unique_dirs/dir_entropy) và f_unc_event_count (UNC) bù.

**f_root_entropy — Entropy theo root (float)**  
Định nghĩa: với \(q\_r=\frac{|\{i:root(p\_i)=r\}|}{N+\varepsilon}\),  
\(f_root_entropy = -\sum_r q_r\log_2(q_r+\varepsilon)\).  
(A) Entropy cao nghĩa là hoạt động phân tán trên nhiều ổ/share; trong bối cảnh ransomware “quét” rộng, đây là dấu cấu trúc tốt hơn chỉ đếm f_unique_root_count. 
(B) Bổ trợ: Kết hợp với f_temp_event_count/f_appdata_event_count để phân biệt “phân tán nhưng chủ yếu ở cache” vs “phân tán vào user_data/UNC”.

**f_doclike_write_count — Số Write vào doc-like (int)**  
Định nghĩa: \(f\_doclike\_write\_count = |\{i:o\_i=W \land g(ext(p\_i))=\text{doc}\}|\).  
(A) Nhiều ransomware nhắm tài liệu người dùng (doc/xls/ppt/pdf…) vì giá trị cao; I/O-based detection thường xét “file type coverage”.
(B) Bổ trợ: Benign cũng Write doc-like (soạn thảo); cần đi cùng f_unique_file_count (unique_files) và f_total_event_rate/f_vmr_10bins (burst) để tách “edit 1 file” khỏi “đụng 1000 file”.

**f_exelike_write_count — Số Write vào executable-like (int)**  
Định nghĩa: \(f\_exelike\_write\_count = |\{i:o\_i=W \land g(ext(p\_i))=\text{exe}\}|\).  
(A) Việc ghi lên exe/dll/sys có thể gợi ý dropper hoặc tampering hệ thống; ít thấy ở người dùng thông thường ngoài cài đặt/cập nhật.
(B) Bổ trợ: f_exelike_write_count tương tác với f_system_event_count/f_program_files_create_write_count (vùng system/program files) để tăng độ phân biệt; nếu Write exe-like nằm trong Temp/AppData có thể là installer cache, cần f_temp_event_count/f_appdata_event_count để giảm nhầm.

**f_write_ext_group_entropy — Entropy nhóm extension trong các Write (float)**  
Định nghĩa: xét tập Write events, đếm theo nhóm \(k \in \{\text{doc,exe,archive,media,image,code,other}\}\):  
\(w_k=\frac{|\{i:o_i=W \land g(ext(p_i))=k\}|}{f_write_count+\varepsilon}\),  
\(f\_write\_ext\_group\_entropy=-\sum_k w_k\log_2(w_k+\varepsilon)\).  
(A) Một số ransomware “funnel” vào nhóm file giá trị (doc/image) → entropy thấp; trong khi một số benign (backup) có thể trải rộng → entropy cao. Các thảo luận về “file type coverage” xuất hiện trong tổng quan I/O-based detection.
(B) Bổ trợ: Vì entropy thấp cũng có thể do workflow chuyên biệt (ví dụ công cụ xử lý ảnh), nên cần kết hợp chặt với f_user_data_event_count–f_user_data_rename_count (mục tiêu thư mục) và f_rename_ext_change_ratio–f_rename_dominant_new_ext_ratio (đổi extension).

### Nhóm D — Đa dạng & tập trung theo namespace

**f_unique_file_count — Số file khác nhau (int)**  
Định nghĩa: \(F=\{norm(p\_i)\}\) trên **full\_path** của mọi sự kiện; \(f\_unique\_file\_count=|F|\).  
(A) Ransomware thường tác động rất nhiều file; UNVEIL và các phân tích ransomware nhấn mạnh traversal rộng.
(B) Bổ trợ: f_unique_file_count kết hợp với f_events_per_file_mean/f_file_event_gini (tập trung hay lan rộng) để phân biệt “đụng nhiều file” (ransomware/backup) vs “đụng ít file nhưng nhiều event” (DB).

**f_unique_dir_count — Số thư mục khác nhau (int)**  
Định nghĩa: \(D=\{dir(norm(p\_i))\}\). \(f\_unique\_dir\_count=|D|\).  
(A) Ransomware thường đi qua nhiều thư mục; đo đạc trong nghiên cứu cũng cho thấy số thư mục “touched” lớn. 
(B) Bổ trợ: f_unique_dir_count giảm nhầm với ứng dụng thao tác nhiều file nhưng trong một thư mục (ví dụ build system); kết hợp f_adjacent_same_dir_ratio (same_dir_adjacent_rate) để nhận ra traversal theo cụm.

**f_unique_ext_count — Số extension khác nhau (int)**  
Định nghĩa: \(X=\{ext(norm(p\_i))\}\). \(f\_unique\_ext\_count=|X|\).  
(A) Ransomware có thể nhắm nhiều loại file; hoặc ngược lại chỉ nhắm một số loại giá trị—cả hai đều có ý nghĩa.
(B) Bổ trợ: f_unique_ext_count phải đọc cùng f_write_ext_group_entropy (entropy) để biết “nhiều extension” có phân tán thật hay chỉ vài nhóm chiếm ưu thế. Nó cũng bù cho f_doclike_write_count (doc-like) khi ransomware nhắm ảnh/media.

**f_events_per_file_mean — Trung bình sự kiện trên mỗi file (float)**  
Định nghĩa: \(f\_events\_per\_file\_mean=\frac{N}{|F|+\varepsilon}\).  
(A) Ransomware kiểu “đi qua từng file một, làm ít thao tác rồi chuyển file khác” thường cho f_events_per_file_mean gần 1–vài; ngược lại app xử lý một file nhiều lần cho f_events_per_file_mean lớn.
(B) Bổ trợ: f_events_per_file_mean kết hợp với f_unique_file_count để phân biệt “many-files shallow” vs “few-files deep”; bù cho f_total_events vốn chỉ nói tổng khối lượng.

**f_file_event_gini — Gini của số sự kiện theo file (float)**  
Định nghĩa: đặt \(c\_f = |\{i:norm(p\_i)=f\}|\) cho mỗi \(f\in F\), \(k=|F|\).  
\(f\_file\_event\_gini = \frac{\sum_{a=1}^{k}\sum_{b=1}^{k} |c_a-c_b|}{2k\sum_{a=1}^{k}c_a+\varepsilon}\).  
(A) Gini thấp → phân bố đều trên nhiều file (ransomware “quét”); Gini cao → tập trung vào vài file (nhiều benign như DB/log).
(B) Bổ trợ: Gini giúp chống né tránh bằng “thêm nhiễu vài file”: nếu malware cố đẩy công việc về ít file để bắt chước benign, f_unique_file_count/f_unique_dir_count và f_user_data_event_count–f_user_data_rename_count sẽ thay đổi theo hướng khác, tạo tương tác hữu ích.

**f_dir_entropy — Entropy phân bố theo thư mục (float)**  
Định nghĩa: với \(p\_d=\frac{|\{i:dir(norm(p\_i))=d\}|}{N+\varepsilon}\),  
\(f\_dir\_entropy=-\sum_{d\in D} p_d\log_2(p_d+\varepsilon)\).  
(A) Entropy cao phản ánh lan tỏa trên nhiều thư mục; ransomware traversal thường làm tăng.
(B) Bổ trợ: Entropy cao cũng có thể do tool index/search. Kết hợp với op mix (f_write_ratio–f_rename_ratio), đặc biệt rename/ext-change (f_rename_ext_change_ratio), để tăng độ chắc.

**f_ext_entropy — Entropy phân bố theo extension (float)**  
Định nghĩa: với \(p\_x=\frac{|\{i:ext(norm(p\_i))=x\}|}{N+\varepsilon}\),  
\(f\_ext\_entropy=-\sum_{x\in X} p_x\log_2(p_x+\varepsilon)\).  
(A) Nhiều ransomware nhắm tập file cụ thể → entropy thấp; một số nhắm rộng → entropy cao; cả hai đều hữu ích khi đặt trong ngữ cảnh thư mục (f_user_data_event_count–f_user_data_rename_count).
(B) Bổ trợ: f_ext_entropy bổ sung cho f_unique_ext_count bằng cách đo “độ đều”; giúp mô hình phân biệt “nhiều extension nhưng chủ yếu vẫn một nhóm” vs “thực sự đa dạng”.

**f_path_depth_mean — Độ sâu đường dẫn trung bình (float)**  
Định nghĩa: \(f\_path\_depth\_mean=\frac{1}{N+\varepsilon}\sum\_{i=1}^{N} depth(norm(p\_i))\).  
(A) Truy cập dữ liệu người dùng thường nằm sâu dưới `Users\<user>\...`; traversal đệ quy qua nhiều nhánh tăng độ sâu trung bình. Nghiên cứu về namespace/độ sâu thư mục trong hệ file cũng cho thấy cấu trúc phân cấp là một thuộc tính đo được và biến thiên theo workload.
(B) Bổ trợ: depth bị phụ thuộc môi trường; do đó cần kết hợp với phân loại known-folder (Nhóm C) để “giải nghĩa” độ sâu (sâu vì user profile hay sâu vì cache).

**f_path_depth_std — Độ lệch chuẩn độ sâu đường dẫn (float)**  
Định nghĩa: \(f\_path\_depth\_std=\sqrt{\frac{1}{N+\varepsilon}\sum\_i (depth(norm(p\_i)) - f\_path\_depth\_mean)^2}\).  
(A) Độ lệch chuẩn cao gợi ý tiến trình đụng cả file ở các mức rất khác nhau (root + sâu), có thể tương ứng traversal rộng. 
(B) Bổ trợ: Kết hợp với f_unique_root_count/f_root_entropy (root diversity) để phân biệt “nhiều mức trong một ổ” vs “nhiều mức trên nhiều ổ/share”.

**f_dominant_dir_ratio — Tỷ lệ thư mục chiếm ưu thế nhất (float)**  
Định nghĩa: \(f\_dominant\_dir\_ratio=\frac{\max\_{d\in D} |\{i:dir(norm(p\_i))=d\}|}{N+\varepsilon}\).  
(A) Ransomware traversal thường giảm sự “độc chiếm” của một thư mục; ngược lại nhiều benign workload tập trung vào một working dir/cache dir.
(B) Bổ trợ: f_dominant_dir_ratio tương tác với f_dir_entropy: entropy thấp thường kéo theo f_dominant_dir_ratio cao; kết hợp cả hai giúp mô hình không cần học quá nhiều từ raw path tokens (giảm rủi ro spurious feature).

### Nhóm E — Cấu trúc chuỗi (sequential structure)

Các đặc trưng này dựa trên thứ tự thời gian của sự kiện. Lưu ý: đặc trưng chuỗi thuần túy có thể bị né tránh bằng cách chèn “noise operations” hoặc đổi thứ tự độc lập—điểm yếu đã được nêu trong nghiên cứu về mô hình hành vi dựa trên system call sequence. Vì vậy, chúng được thiết kế để **phối hợp** với Nhóm C/D/F.

**f_adjacent_same_dir_ratio — Tỷ lệ hai sự kiện liên tiếp cùng thư mục (float)**  
Định nghĩa: nếu \(N<2\) thì 0, ngược lại  
\(f\_adjacent\_same\_dir\_ratio=\frac{|\{i: dir(p_i)=dir(p_{i+1})\}|}{N-1}\).  
(A) Ransomware thường xử lý nhiều file trong cùng thư mục trước khi chuyển sang thư mục khác → f_adjacent_same_dir_ratio có thể cao; đồng thời nhiều benign scan/index có thể thấp (đổi thư mục liên tục). 
(B) Bổ trợ: f_adjacent_same_dir_ratio kết hợp với f_unique_dir_count/f_dir_entropy để phân biệt “làm sâu trong từng thư mục” vs “nhảy thư mục”; và kết hợp với f_rename_ext_change_ratio–f_rename_dominant_new_ext_ratio để nhận ra pattern “append extension hàng loạt trong cùng folder”.

**f_adjacent_same_path_ratio — Tỷ lệ hai sự kiện liên tiếp cùng full_path (float)**  
Định nghĩa: \(f\_adjacent\_same\_path\_ratio=\frac{|\{i: norm(p\_i)=norm(p\_{i+1})\}|}{N-1}\) (0 nếu \(N<2\)).  
(A) App như DB/log writer thường có nhiều thao tác lặp trên cùng file → f_adjacent_same_path_ratio cao; ransomware quét nhiều file → f_adjacent_same_path_ratio thấp.
(B) Bổ trợ: f_adjacent_same_path_ratio bổ sung cho f_file_event_gini (Gini): f_file_event_gini nhìn phân bố toàn cục, f_adjacent_same_path_ratio nhìn “tính cục bộ theo thời gian”; cả hai cùng giúp phân biệt “few-files deep” vs “many-files shallow”.

**f_transition_create_to_write — Xác suất chuyển Create→Write (float)**  
Định nghĩa: \(f\_transition\_create\_to\_write=\frac{|\{i:o\_i=C \land o\_{i+1}=W\}|}{N-1+\varepsilon}\).  
(A) Nhiều xử lý hợp lệ lẫn malware có Create→Write; nhưng trong ransomware kiểu “tạo bản mã hóa mới rồi thao tác”, mẫu này có thể tăng.
(B) Bổ trợ: f_transition_create_to_write mạnh hơn khi đồng thời f_delete_count/f_transition_write_to_delete (Delete sau đó) tăng, tạo motif “Create→Write→Delete” trên nhiều file; motif dạng này bù cho thiếu read/entropy.

**f_transition_write_to_rename — Xác suất chuyển Write→Rename (float)**  
Định nghĩa: \(f\_transition\_write\_to\_rename=\frac{|\{i:o\_i=W \land o\_{i+1}=R\}|}{N-1+\varepsilon}\).  
(A) Nhiều ransomware ghi ciphertext rồi đổi tên/append extension để đánh dấu; quan sát về delete/rename sau khi tạo ciphertext được mô tả trong phân tích ransomware.
(B) Bổ trợ: f_transition_write_to_rename kết hợp trực tiếp với f_rename_ext_change_ratio/f_rename_dominant_new_ext_ratio: nếu Write→Rename cao và đa số rename đổi extension theo một pattern, độ phân biệt tăng mạnh; nếu malware “không rename”, f_transition_write_to_rename không giúp, lúc đó dùng f_transition_write_to_delete + f_doclike_write_count/f_user_data_write_count.

**f_transition_write_to_delete — Xác suất chuyển Write→Delete (float)**  
Định nghĩa: \(f\_transition\_write\_to\_delete=\frac{|\{i:o\_i=W \land o\_{i+1}=D\}|}{N-1+\varepsilon}\).  
(A) Gợi ý hành vi overwrite rồi xóa hoặc tạo file mới rồi xóa bản cũ (trong cửa sổ quan sát); phù hợp với mô tả “delete original after producing encrypted file”.
(B) Bổ trợ: Write→Delete đôi khi benign (temp file). Vì vậy phải gắn với vùng user_data (f_user_data_delete_count) và “rename ext-change” thấp/cao để suy ra đúng motif (wiper vs ransomware vs temp cleanup).

**f_longest_same_op_run — Run dài nhất của cùng op_type (int)**  
Định nghĩa: xét chuỗi \(o\_1..o\_N\), \(f\_longest\_same\_op\_run=\max\) độ dài đoạn liên tiếp có cùng op\_type.  
(A) Ransomware thường có “đoạn dài” bị chi phối bởi Write hoặc Rename (một kiểu thao tác lặp lại hàng loạt). UNVEIL quan sát các mẫu I/O lặp mạnh do chiến lược tấn công giống nhau cho mỗi file.
(B) Bổ trợ: Run-length là đặc trưng chuỗi tương đối bền trước việc đổi thứ tự giữa các nhóm file (nếu vẫn lặp cùng loại thao tác). Kết hợp với f_op_type_entropy (op entropy) để phân biệt “một run dài” với “phân bố đều”.

### Nhóm F — Rename & nhân bản tên file

**f_rename_ext_change_ratio — Tỷ lệ Rename đổi extension (float)**  
Định nghĩa: nếu \(f\_rename\_count=0\) thì 0, ngược lại  
\(f\_rename\_ext\_change\_ratio=\frac{|\{i:o_i=R \land ext(p_i)\ne ext(p_i^{new})\}|}{f\_rename\_count+\varepsilon}\).  
(A) Đổi extension là dấu hiệu mạnh của “file type changing/marking” trong nhiều mẫu ransomware; đo đạc gần đây cho thấy đa số mẫu append extension mới.
(B) Bổ trợ: Nếu malware randomize extension để né f_rename_dominant_new_ext_ratio, f_rename_ext_change_ratio vẫn có thể cao; nếu malware không rename, f_rename_ext_change_ratio=0 và cần dựa vào f_user_data_write_count/f_doclike_write_count/f_unique_file_count/f_dir_entropy.

**f_rename_dominant_new_ext_ratio — Dominant new-extension ratio trong Rename (float)**  
Định nghĩa: với mỗi extension \(e\), \(u\_e = |\{i:o\_i=R \land ext(p\_i^{new})=e\}|\).  
\(f\_rename\_dominant\_new\_ext\_ratio=\frac{\max_e u_e}{f\_rename\_count+\varepsilon}\) (0 nếu \(f\_rename\_count=0\)).  
(A) Nhiều ransomware gắn một extension “thương hiệu” cho toàn bộ file đã mã hóa → một \(e\) chiếm ưu thế. 
(B) Bổ trợ: f_rename_dominant_new_ext_ratio chống false positive của batch-rename “đa dạng” (ví dụ đổi tên ảnh có giữ extension) vì khi benign rename mà không đổi extension, f_rename_ext_change_ratio thấp. Cặp (f_rename_ext_change_ratio,f_rename_dominant_new_ext_ratio) là tương tác then chốt.

**f_rename_filename_prefix_similarity — Độ giống tiền tố tên file cũ–mới trung bình (float)**  
Định nghĩa: với rename event \(i\), đặt \(a=fname(p\_i), b=fname(p\_i^{new})\), \(LCP(a,b)\)=độ dài common prefix theo ký tự.  
\(s_i=\frac{LCP(a,b)}{\max(|a|,1)}\).  
\(f\_rename\_filename\_prefix\_similarity=\text{mean}(s_i)\) trên mọi rename.  
(A) Append extension kiểu `report.docx` → `report.docx.locked` tạo LCP rất cao; đây là một fingerprint “string-level” không cần nội dung/bytes. 
(B) Bổ trợ: Nếu malware đổi tên hoàn toàn (LCP thấp) thì f_rename_filename_prefix_similarity yếu; khi đó f_rename_ext_change_ratio (đổi extension) và f_unique_dir_count/f_dir_entropy (lan rộng) vẫn hỗ trợ. Đồng thời f_rename_filename_prefix_similarity giúp chống benign rename có mẫu khác (ví dụ đổi tên theo template nhưng không append).

**f_rename_same_dir_ratio — Tỷ lệ Rename giữ nguyên thư mục (float)**  
Định nghĩa: \(f\_rename\_same\_dir\_ratio=\frac{|\{i:o\_i=R \land dir(p\_i)=dir(p\_i^{new})\}|}{f\_rename\_count+\varepsilon}\) (0 nếu \(f\_rename\_count=0\)).  
(A) Nhiều ransomware chỉ đổi tên/extension ngay tại chỗ (same directory) thay vì di chuyển; do đó f_rename_same_dir_ratio thường cao trong append-extension scenario.
(B) Bổ trợ: f_rename_same_dir_ratio kết hợp với f_user_data_rename_count (rename trong user_data) giúp phân biệt “append extension tại chỗ” (ransomware) với “move/organize” (benign file manager).

**f_create_filename_replication — Mức nhân bản filename khi Create (int)**  
Định nghĩa: xét tập Create events \(I\_C=\{i:o\_i=C\}\). Với mỗi filename \(u=fname(norm(p\_i))\), đặt \(S\_u=\{dir(norm(p\_i)) : i\in I\_C \land fname(norm(p\_i))=u\}\).  
\(f\_create\_filename\_replication=\max_u |S_u|\) (nếu không có Create thì 0).  
(A) Ransomware thường tạo **ransom note** với cùng tên trong nhiều thư mục; đặc trưng này bắt tín hiệu đó mà không cần biết tên note cụ thể.
(B) Bổ trợ: f_create_filename_replication bù cho trường hợp ransomware không rename extension (f_rename_ext_change_ratio thấp) nhưng vẫn thả note; hoặc ngược lại, nếu note name ngẫu nhiên, f_create_filename_replication giảm và ta dựa vào f_unique_file_count/f_dir_entropy/f_transition_write_to_rename. Nó cũng giúp giảm false positive của installer: installer ít khi tạo cùng một filename ở hàng trăm thư mục user.

## Tập đặc trưng lõi khuyến nghị

Một tập “lõi” (ít nhưng mạnh) nên ưu tiên các đặc trưng vừa (i) bắt được **bản chất bắt buộc** của ransomware (tamper nhiều file người dùng), vừa (ii) ít phụ thuộc môi trường, vừa (iii) có tính bổ trợ để chống false positive của backup/sync.

Tập lõi đề xuất (15 đặc trưng, lấy từ master):

- **Cường độ & mix:** f_write_count (Write), f_delete_count (Delete), f_rename_count (Rename), f_total_event_rate (rate_total), f_op_type_entropy (op entropy).  
- **Temporal:** f_vmr_10bins (VMR_10), f_inactivity_bin_fraction (inactivity_frac).  
- **Targeting:** f_user_data_write_count (Write trong user_data), f_user_data_rename_count (Rename trong user_data), f_temp_event_count (Temp activity), f_unc_event_count (UNC activity).  
- **Lan rộng:** f_unique_file_count (unique_files), f_unique_dir_count (unique_dirs), f_file_event_gini (Gini per-file), f_dir_entropy (dir entropy).  
- **Rename cấu trúc:** f_rename_ext_change_ratio (ext-change frac), f_rename_dominant_new_ext_ratio (dominant new-ext ratio) *hoặc* f_create_filename_replication (create filename replication) tùy mục tiêu ransomware/không.

Lý do tập này mạnh:

- Nó ghép thành “tam giác” **cường độ (f_total_event_rate/f_write_count) + lan rộng (f_unique_file_count/f_dir_entropy/f_file_event_gini) + mục tiêu (f_user_data_write_count/f_user_data_rename_count vs f_temp_event_count)**, bám sát insight rằng ransomware phải đụng vào file nạn nhân và thường thực hiện hàng loạt.
- Nó bổ sung một lớp “tín hiệu rename” (f_rename_ext_change_ratio/f_rename_dominant_new_ext_ratio) vốn rất hữu ích khi thiếu entropy/bytes; đồng thời có “escape hatch” f_create_filename_replication cho biến thể tạo note.
- Nó giảm phụ thuộc vào dữ liệu sandbox: phần lớn dựa trên tỷ lệ/entropy/phân bố thay vì token cụ thể của path, phù hợp với khuyến nghị hạn chế học “spurious features” và xử lý phân phối endpoint khác nhau.   

## Họ đặc trưng mở rộng tùy chọn

Các họ dưới đây hữu ích khi tôi muốn tăng “độ bắt được biến thể” hoặc tăng chống né tránh, đổi lại vector lớn hơn và/hoặc tính toán nặng hơn. Tất cả vẫn computable từ strict schema.

### Histogram mặt nạ hành vi theo file

Ý tưởng: với mỗi file \(f\), xét trong cửa sổ các op-type đã xảy ra trên file đó, tạo mặt nạ 4-bit:  
\(\text{mask}(f)=1[C\in S_f]+2[W\in S_f]+4[D\in S_f]+8[R\in S_f]\) (16 trạng thái).  
Tạo histogram 16 chiều và entropy của mask distribution.

- Lợi ích: gần với “per-file access pattern repetitiveness” mà UNVEIL khai thác (dù UNVEIL có thêm read/entropy), nhưng ở mức coarse, vẫn có thể bắt motif như Write+Rename, Create+Write+Delete lặp trên hàng loạt file.
- Trade-off: cần group by file path trong cửa sổ; rename làm “đứt” identity file (không có file ID), nhưng vẫn hữu ích nếu dùng old-path nhất quán hoặc nối bằng edges rename trong cửa sổ.

### Đặc trưng “tốc độ lan rộng theo thư mục”

Từ chuỗi thư mục theo thời gian \(d_i=dir(p_i)\), đo:
- số lần chuyển thư mục (switch count),
- phân bố độ dài “đoạn liên tiếp cùng thư mục”,
- khoảng thời gian trung bình spent trong một thư mục (từ timestamp đầu tới cuối của các event trong thư mục đó trong cửa sổ).

Lợi ích: phân biệt traversal kiểu ransomware (ở mỗi thư mục xử lý nhiều file rồi đi tiếp) với nhiều benign workload (ví dụ DB log: gần như một thư mục).

### Vector hóa đường dẫn bằng hashing n-gram

Tạo **vector cố định kích thước \(M\)** (ví dụ 1024/4096) từ token/n-gram ký tự của \(norm(full\_path)\) hoặc \(dir\) hoặc \(ext\). Mỗi chiều \(j\) là tổng số lần các token hash vào \(j\). Điều này tương tự cách một số mô hình coi “behavior report như văn bản” để học đặc trưng tự động.

- Lợi ích: bắt được tín hiệu tinh hơn (tên thư mục, pattern filename “random-looking”, GUID-like) mà không cần metadata bổ sung.  
- Rủi ro: dễ học artifact môi trường (đặc biệt sandbox) và giảm tổng quát; cần regularization, feature normalization, và đánh giá chéo theo máy/organization.   

### Mô hình point process/Hawkes đơn giản trên timestamps

Fitting một Hawkes process hoặc mô hình point process đơn giản để lấy tham số “self-excitation/branching ratio” (đặc trưng clustering) từ \(t_i\). Hawkes được dùng để mô tả chuỗi sự kiện có tính “kích hoạt” và clustering.

- Lợi ích: tóm tắt temporal dynamics bằng vài tham số thay vì nhiều thống kê rời rạc.  
- Trade-off: fitting nặng, nhạy với N nhỏ, và không trực tiếp dùng ngữ nghĩa đường dẫn—vì vậy chỉ nên là feature bổ sung.

## Cân nhắc đối kháng và né tránh

### Benign có thể “giống ransomware” như thế nào

Một số phần mềm hợp lệ có thể tạo dấu vết gần như ransomware nếu chỉ nhìn count/rate:

- **Backup/sync clients**: chạm hàng loạt file, có thể rename/replace/cleanup, thậm chí thao tác trên network shares.  
- **Công cụ batch rename / organizer ảnh**: rename hàng loạt trong user_data.  
- **Công cụ mã hóa hợp lệ / nén giải nén lớn**: write dày và theo burst.  

Ngay cả CryptoDrop cũng thừa nhận giới hạn cơ bản: hệ thống quan sát thay đổi dữ liệu khó “hiểu ý định” và cần phối hợp nhiều chỉ báo để giảm false positives.

Cách giảm nhầm trong khung feature của tôi là **bắt tương tác**:
- Nếu “nặng nhưng lành”: thường tập trung ở AppData/Temp (f_appdata_event_count/f_temp_event_count cao), Gini cao (f_file_event_gini cao) hoặc f_adjacent_same_path_ratio cao (lặp cùng file), rename không đổi extension (f_rename_ext_change_ratio thấp).  
- Nếu “nặng và phá”: Write/Delete/Rename cao (f_write_count/f_delete_count/f_rename_count), lan rộng (f_unique_file_count/f_dir_entropy), tập trung vào user_data/UNC (f_user_data_write_count/f_unc_event_count), và có dấu hiệu đổi extension hàng loạt (f_rename_ext_change_ratio/f_rename_dominant_new_ext_ratio) hoặc tạo cùng filename trên nhiều thư mục (f_create_filename_replication).

### Malware có thể né các đặc trưng “ngây thơ” ra sao

- **Throttling/slow encryption:** giảm f_total_event_rate và làm f_interarrival_mean lớn, tăng f_inactivity_bin_fraction; đây là kỹ thuật né dễ nhất đối với detector dựa trên rate.  
  - Giảm brittleness bằng cách dựa thêm vào lan rộng/entropy thư mục (f_unique_file_count/f_dir_entropy) và dấu rename cấu trúc (f_rename_ext_change_ratio–f_rename_same_dir_ratio).  
- **Chèn nhiễu / xen thao tác vô hại:** tương tự vấn đề đã được nêu trong nghiên cứu về mô hình chuỗi system call: chuỗi thuần túy có thể bị phá bằng reorder/chèn call độc lập.
  - Khắc phục bằng các đặc trưng phân bố (entropy/Gini) vốn khó “đánh lừa” nếu malware vẫn phải xử lý hàng loạt file thật.  
- **Randomize extension hoặc không đổi extension:** làm suy yếu f_rename_dominant_new_ext_ratio/f_rename_ext_change_ratio.  
  - Bù bằng motif Write→Delete (f_transition_write_to_delete), lan rộng (f_unique_file_count/f_unique_dir_count), và “tạo file note hàng loạt” (f_create_filename_replication) nếu tồn tại.
- **Chia nhỏ hành vi qua nhiều PID:** vì tôi quan sát “mỗi PID độc lập”, attacker có thể dùng multi-process để làm mỗi PID trông bình thường.  
  - Đây là giới hạn cấu trúc của bài toán (không giải bằng feature trong schema). Nếu giữ nguyên ràng buộc “một PID”, cách giảm tác hại là chọn cửa sổ đủ dài và tận dụng rename/note replication vốn khó phân tán hoàn toàn.  

### Rủi ro từ dữ liệu huấn luyện và triển khai

Nhiều nghiên cứu gần đây chỉ ra khoảng cách hiệu năng lớn khi mô hình hành vi huấn luyện trên sandbox đem ra endpoint do **distribution shift, label noise, và spurious features**; ngoài ra malware còn có thể né sandbox bằng đặc trưng “wear-and-tear”.

Hệ quả cho feature engineering trong schema của tôi:

- Ưu tiên đặc trưng **tương đối/chuẩn hóa** (tỷ lệ, entropy, Gini, VMR) hơn là token path cụ thể.  
- Các đặc trưng dựa trên “Known Folder” (Nhóm C) hữu ích nhưng cần viết rule robust (không hardcode username) và nên coi là “context features” thay vì quyết định một mình.
- Với đối thủ chủ động, ML model có thể bị tấn công né tránh; thực hành phổ biến là adversarial training/hardening, vốn cũng được thảo luận trong bối cảnh malware ML evasion.
