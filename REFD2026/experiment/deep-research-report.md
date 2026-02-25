# Thiết kế vector đặc trưng hành vi thao tác file theo PID

## Khung vấn đề và taxonomy nhóm đặc trưng

Bài toán của bạn là **phân loại nhị phân (malware vs benign)** dựa **duy nhất** trên luồng sự kiện thao tác file của **một tiến trình đơn (một PID)** trong một **cửa sổ thời gian trượt**. So với nhiều hướng nghiên cứu “behavioral malware detection” kinh điển, đây là một bối cảnh **thông tin cực hạn chế**: không có read/open/close, không có bytes/size, không có entropy nội dung, không có trạng thái thành công/thất bại, không có tên tiến trình/cha-con, không có user/session. Điều này loại bỏ trực tiếp một số chỉ báo rất mạnh thường dùng trong phát hiện ransomware dựa trên I/O (ví dụ: so sánh entropy dữ liệu đọc/ghi, mẫu read→encrypt→overwrite) như trong UNVEIL/CryptoDrop. citeturn3view0turn4view0turn17search3

Vì vậy, mục tiêu thiết kế cần chuyển từ “nhận biết nội dung bị mã hóa” sang “nhận biết **hình thái hành vi (shape)** của tiến trình khi thao tác file”: cường độ, phân bố theo thời gian, mức độ lan rộng theo namespace, cấu trúc rename, và đặc biệt là **ngữ nghĩa đường dẫn (path semantics)** (ví dụ đụng đến User Documents/ProgramData/Temp/System). Các nghiên cứu về ransomware đều nhấn mạnh rằng để tấn công thành công, ransomware **phải can thiệp vào file của nạn nhân** (write/overwrite/delete/rename) với hành vi lặp đi lặp lại trên nhiều file, tạo ra dấu vết I/O đặc trưng—dù cơ chế mã hóa có thể khác nhau. citeturn5view0turn3view0turn2view0

Dựa trên ràng buộc schema, một taxonomy thực dụng (2–8 nhóm) nên bao phủ cả tín hiệu “thô” (volume/rate) lẫn tín hiệu “cấu trúc” (entropy/đa dạng/chuỗi/rename) để chống false positive và tránh bị né tránh bằng nhiễu. Ngoài ra, cần lưu ý tính **dịch chuyển phân phối** giữa môi trường thu thập dữ liệu và môi trường triển khai (sandbox vs endpoint, hoặc endpoint giữa các máy khác nhau), vốn được chỉ ra là nguyên nhân làm suy giảm mạnh hiệu năng của nhiều detector hành vi trong thực tế. citeturn8view0turn16search3

### Taxonomy đề xuất

- **Nhóm A — Cường độ & phối trộn op_type:** đếm/tỷ lệ Create/Write/Delete/Rename, entropy của op mix. Đây là nền cho gần như mọi detector I/O ransomware vì ransomware thường tạo ra “khối lượng sửa đổi” bất thường. citeturn5view0turn4view0turn3view0  
- **Nhóm B — Động học thời gian (temporal dynamics):** inter-arrival time, burstiness, độ tập trung theo time-bin, tỷ lệ khoảng lặng. Ransomware thường chạy theo “đợt” với cường độ dày, còn nhiều benign có nhịp khác (hoặc burst nhưng gắn với thư mục/định dạng đặc thù). Các thước đo burstiness/memory trong chuỗi sự kiện được nghiên cứu rộng rãi trong mô hình hóa hành vi theo thời gian. citeturn13search0turn13search2turn12search12  
- **Nhóm C — Ngữ nghĩa đường dẫn & mục tiêu:** phân loại vùng hệ thống vs Program Files/ProgramData vs user data vs AppData/Temp vs UNC/network share; nhóm extension (doc-like/executable-like). Windows “Known Folders” và biến môi trường chuẩn cung cấp cách gắn hoạt động file vào ý nghĩa người dùng/hệ thống. citeturn6search0turn6search1  
- **Nhóm D — Đa dạng & tập trung theo namespace:** số lượng file/dir/extension khác nhau, entropy phân bố theo thư mục/extension, Gini (tập trung trên vài file hay lan rộng). UNVEIL và các phân tích ransomware đều quan sát traversal qua rất nhiều thư mục/ổ đĩa và thao tác lặp trên nhiều file. citeturn3view0turn15view0  
- **Nhóm E — Cấu trúc chuỗi (sequential structure):** xác suất chuyển trạng thái (Create→Write, Write→Rename, Write→Delete…), độ “bám” cùng thư mục/file giữa các sự kiện liên tiếp, độ dài run. Mô hình chuỗi/đồ thị hành vi từ system call/I/O là một hướng lâu đời; đồng thời đã được chỉ ra rằng đặc trưng chuỗi thuần túy dễ bị né tránh bằng chèn nhiễu, nên cần phối hợp với đặc trưng phân bố/namespace. citeturn10view0turn1search2  
- **Nhóm F — Đặc trưng Rename & nhân bản tên file:** tỷ lệ đổi extension, mức “append suffix”, độ giống tiền tố tên file cũ/mới, rename cùng thư mục, và “tạo cùng một filename ở nhiều thư mục” (dấu hiệu ransom note). Nhiều ransomware có hành vi append extension và/hoặc tạo note ở nhiều thư mục; ngay cả khi ransomware cố né bằng thay extension ngẫu nhiên, các chỉ báo cấu trúc khác vẫn hỗ trợ. citeturn15view0turn17search13  

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

Với các đặc trưng theo **nhóm extension**, định nghĩa hàm \(g(ext)\in\{\text{doc},\text{exe},\text{archive},\text{media},\text{image},\text{code},\text{other}\}\) theo danh sách đuôi mở rộng do bạn cố định (ví dụ doc: doc/docx/xls/xlsx/ppt/pptx/pdf/txt/rtf/odt/…; exe: exe/dll/sys/scr/…).

Với các đặc trưng theo **vùng đường dẫn Windows**, dùng luật match theo prefix/segment (không cần username cụ thể), ví dụ:
- `\Windows\`, `\Windows\System32\` (system)
- `\Program Files\`, `\Program Files (x86)\` (program_files)
- `\ProgramData\` (programdata)
- `\Users\<*>\Documents\`, `Desktop\`, `Downloads\`… (user_data)
- `\Users\<*>\AppData\Roaming\`, `\Users\<*>\AppData\Local\` (appdata)
- `\Users\<*>\AppData\Local\Temp\` (temp)
- UNC/network share: bắt đầu bằng `\\`  
Các đúng/sai của điển hình đường dẫn “Known Folders” và biến môi trường được mô tả trong tài liệu của entity["company","Microsoft","technology company"]. citeturn6search0turn6search1  

### Nhóm A — Cường độ & phối trộn op_type

**f1 — Tổng số sự kiện (int)**  
Định nghĩa: \(f1 = N\).  
(A) Cơ chế phân biệt: Nhiều lớp malware, đặc biệt ransomware, tạo ra **khối lượng thao tác file cao** khi can thiệp hàng loạt file nạn nhân; các nghiên cứu I/O-based ransomware nhấn mạnh đặc trưng “nhiều thao tác I/O trong thời gian ngắn”. citeturn5view0turn3view0turn15view0  
(B) Bổ trợ: Một mình \(N\) dễ false positive (backup/sync/installer). Nó cần đi cùng đặc trưng ngữ nghĩa đường dẫn (Nhóm C) và độ lan rộng (Nhóm D) để phân biệt “tác vụ nặng nhưng hợp lệ” với “tác vụ nặng và phá hoại”.

**f2 — Số Create (int)**  
Định nghĩa: \(f2 = |\{i:o_i=C\}|\).  
(A) Create nhiều có thể xuất hiện trong dropper (thả nhiều file) hoặc ransomware tạo file note/aux; tuy nhiên benign cũng tạo temp/cache. citeturn15view0turn4view0  
(B) Bổ trợ: Create trở nên đáng ngờ khi đồng thời (i) xảy ra trong user_data hoặc Program Files (Nhóm C), (ii) kèm Write/Delete/Rename cao (A/B/D/F), hoặc (iii) “tạo cùng một tên file ở nhiều thư mục” (f53).

**f3 — Số Write/Modify (int)**  
Định nghĩa: \(f3 = |\{i:o_i=W\}|\).  
(A) Ransomware file-locker/crypto-ransomware buộc phải ghi/ghi đè dữ liệu trên nhiều file (hoặc tạo file mới rồi ghi), do đó Write là trụ cột của I/O-based detection. citeturn5view0turn3view0turn17search3  
(B) Bổ trợ: Write cũng phổ biến ở benign (browser cache, DB). Kết hợp với “độ lan rộng” (f33–f39), “mục tiêu user_data” (f19–f22), và dấu rename/extension-change (f49–f52) để giảm nhầm.

**f4 — Số Delete (int)**  
Định nghĩa: \(f4 = |\{i:o_i=D\}|\).  
(A) Delete cao có thể gợi ý wiper hoặc chiến lược “tạo ciphertext rồi xóa bản gốc”; các mô tả hành vi ransomware cũng nêu xóa/đổi tên bản gốc như một cách hoàn tất tấn công. citeturn17search13turn5view0  
(B) Bổ trợ: Delete đơn lẻ dễ nhầm với cleanup hợp lệ; mạnh hơn khi đi cùng Write/Rename cao và tập trung vào user_data (f21/f4 tương quan theo cơ chế “phá dữ liệu người dùng”).

**f5 — Số Rename (int)**  
Định nghĩa: \(f5 = |\{i:o_i=R\}|\).  
(A) Rename nổi bật trong nhiều kịch bản ransomware: append extension, đổi tên file, hoặc di chuyển để đánh dấu đã xử lý; nhiều mẫu ransomware quan sát được có hành vi đổi extension trên diện rộng. citeturn15view0turn17search13  
(B) Bổ trợ: Rename hợp lệ cũng phổ biến (editor lưu tạm, tool batch-rename). Cần kết hợp dấu “đổi extension” (f49), “dominant extension mới” (f50), và “cùng lúc Write/Delete tăng” (f3/f4) để tránh nhầm.

**f6 — Tốc độ sự kiện tổng (float)**  
Định nghĩa: \(f6 = \frac{N}{\Delta+\varepsilon}\).  
(A) Ransomware thường cố mã hóa nhanh để tối đa hóa thiệt hại trước khi bị chặn; các nghiên cứu quan sát I/O cho thấy tần suất thao tác file/crypto API có thể đạt hàng nghìn/giây trong sandbox. citeturn15view0turn3view0  
(B) Bổ trợ: Nếu malware “throttling” để né, f6 suy yếu; khi đó các đặc trưng cấu trúc (entropy thư mục, đa ổ đĩa, rename patterns) sẽ bù.

**f7 — Tỷ lệ Write (float)**  
Định nghĩa: \(f7 = \frac{f3}{N+\varepsilon}\).  
(A) Ransomware file-encrypting thường bị chi phối bởi thao tác ghi; do đó tỷ lệ Write trong op mix là tín hiệu mạnh hơn “đếm tuyệt đối” trong bối cảnh cửa sổ có thể ngắn. citeturn3view0turn17search3  
(B) Bổ trợ: Dùng cùng f10 (entropy op) và f19–f22 (đích user_data) để phân biệt “Write nhiều nhưng vào cache/Temp” với “Write nhiều vào tài liệu”.

**f8 — Tỷ lệ Delete (float)**  
Định nghĩa: \(f8 = \frac{f4}{N+\varepsilon}\).  
(A) Delete cao tương thích với wiper/cleanup phá hoại hoặc chiến lược xóa bản gốc sau khi tạo ciphertext; là một trong các nhóm I/O characteristics thường được xem xét trong I/O-based ransomware detection. citeturn17search3turn17search13  
(B) Bổ trợ: Delete cũng là cleanup bình thường; cần nhìn cùng (i) Write trước Delete (f47), (ii) vùng user_data (f21), (iii) mức lan rộng (f33–f38).

**f9 — Tỷ lệ Rename (float)**  
Định nghĩa: \(f9 = \frac{f5}{N+\varepsilon}\).  
(A) Append extension trên hàng loạt file sẽ làm tỷ lệ Rename tăng đáng kể; nhiều quan sát cho thấy đổi extension là dấu hiệu gắn với mã hóa. citeturn15view0  
(B) Bổ trợ: Nếu ransomware không rename (chỉ overwrite), f9 thấp; khi đó f3/f6/f14 (burstiness) và f30 (write_doclike) sẽ “gánh”.

**f10 — Entropy phối trộn op_type (float)**  
Định nghĩa: đặt \(p_o=\frac{|\{i:o_i=o\}|}{N+\varepsilon}\) với \(o\in\{C,W,D,R\}\).  
\(f10 = -\sum_{o} p_o \log_2(p_o+\varepsilon)\).  
(A) Entropy thấp nghĩa là tiến trình “chỉ làm một kiểu thao tác” (ví dụ: write-dominant), thường thấy trong pha mã hóa hàng loạt; entropy cao hơn có thể xuất hiện ở workflow benign phức tạp, nhưng cần xem theo đường dẫn. citeturn3view0turn5view0  
(B) Bổ trợ: f10 giảm nhiễu cho các count tuyệt đối (f2–f5) bằng cách mô tả “hình dạng” distribution; kết hợp với f32/f39 (entropy theo extension/dir) để phân biệt loại “đơn điệu nhưng lan rộng” (đáng ngờ) với “đơn điệu nhưng cục bộ” (ví dụ log writer).

### Nhóm B — Động học thời gian

Đặt inter-arrival times \(\tau_i = t_{i+1}-t_i\) cho \(i=1..N-1\) (nếu \(N<2\) thì các đặc trưng dựa trên \(\tau\) lấy 0). Chia cửa sổ thành \(m=10\) time-bin đều nhau, đếm \(n_j\) là số sự kiện trong bin \(j\).

**f11 — Trung bình inter-arrival (float)**  
Định nghĩa: \(f11=\frac{1}{\max(N-1,1)}\sum_{i=1}^{N-1}\tau_i\).  
(A) Ransomware thường tạo chuỗi thao tác dày → inter-arrival nhỏ; benign tương tác người dùng thường thưa/gián đoạn hơn. Quan sát về “burst” trong chuỗi sự kiện là nền tảng của nhiều thước đo temporal. citeturn12search12turn13search0  
(B) Bổ trợ: Bị yếu khi malware cố chèn delay; khi đó dùng f14/f17 (bin-level burstiness & khoảng lặng) và Nhóm C/D (đích & lan rộng) để vẫn bắt được footprint.

**f12 — Hệ số biến thiên inter-arrival (float)**  
Định nghĩa: \(\mu=\text{mean}(\tau),\ \sigma=\text{std}(\tau)\). \(f12=\frac{\sigma}{\mu+\varepsilon}\).  
(A) CV cao thường phản ánh burst xen kẽ khoảng lặng; nhiều tiến trình benign có nhịp đều hơn hoặc theo “chunk” có cấu trúc khác ransomware. citeturn13search0turn13search2  
(B) Bổ trợ: CV kết hợp với f14 (VMR) giúp tách “burst do công việc nặng” khỏi “burst do traversal phá hoại”; đi cùng f19–f22 để tránh nhầm các app sync hợp lệ.

**f13 — Bách phân vị 90% inter-arrival (float)**  
Định nghĩa: \(f13 = Q_{0.9}(\{\tau_i\})\).  
(A) Nếu tiến trình có nhiều khoảng nghỉ dài xen kẽ burst, Q90 sẽ lớn; ransomware “chạy liên tục” thường có Q90 nhỏ hơn trong pha mã hóa. citeturn15view0  
(B) Bổ trợ: f13 bổ sung cho f11/f12 bằng cách “nhìn đuôi”; kết hợp với f18 (mất cân bằng nửa cửa sổ) để bắt kịch bản ransomware bắt đầu muộn trong cửa sổ.

**f14 — Variance-to-mean ratio theo 10 bin (float)**  
Định nghĩa: \(\bar n=\frac{1}{m}\sum_{j=1}^m n_j,\ s^2=\frac{1}{m}\sum_{j=1}^m (n_j-\bar n)^2\).  
\(f14=\frac{s^2}{\bar n+\varepsilon}\).  
(A) VMR > 1 mô tả clustering/burstiness; các mô hình và thước đo burstiness được dùng rộng rãi để đặc trưng chuỗi sự kiện không-Poisson. citeturn13search0turn13search2turn12search12  
(B) Bổ trợ: f14 ít nhạy với reorder ở mức nhỏ, và vẫn hoạt động khi N lớn; kết hợp với f6 (rate) để phân biệt “nhiều sự kiện đều” vs “nhiều sự kiện dồn cục”.

**f15 — Tỷ lệ bin dày nhất (float)**  
Định nghĩa: \(f15=\frac{\max_j n_j}{N+\varepsilon}\).  
(A) Ransomware có thể tạo “đỉnh” khi vào pha mã hóa; benign tương tác người dùng thường ít tạo một đỉnh chiếm tỷ trọng quá lớn trong cửa sổ ngắn. citeturn15view0  
(B) Bổ trợ: f15 tương tác mạnh với f19–f22 (nếu đỉnh nằm trong user_data thì đáng ngờ hơn) và với f24 (nếu đỉnh nằm trong temp thì có thể là cache benign).

**f16 — Burstiness tham số B (float)**  
Định nghĩa (Goh–Barabási): \(B=\frac{\sigma-\mu}{\sigma+\mu+\varepsilon}\) với \(\mu=\text{mean}(\tau),\sigma=\text{std}(\tau)\). \(f16=B\). citeturn13search0turn13search8  
(A) \(B\to 1\) biểu thị cực bursty; \(B\to -1\) gần đều; ransomware thường có burstiness cao trong pha mã hóa. citeturn15view0  
(B) Bổ trợ: B có hiệu ứng “finite-size” khi N nhỏ; nghiên cứu đã bàn về điều này và đề xuất hiệu chỉnh. citeturn13search2 Vì vậy, trong triển khai nên dùng f16 cùng f14 (bin-level) để ổn định.

**f17 — Tỷ lệ bin trống (float)**  
Định nghĩa: \(f17=\frac{|\{j: n_j=0\}|}{m}\).  
(A) Malware “throttling” để né detector sẽ tạo nhiều khoảng lặng → f17 tăng; ngược lại ransomware chạy liên tục thì f17 thấp. citeturn15view0  
(B) Bổ trợ: f17 bù cho các đặc trưng “rate” (f6) bị vô hiệu bởi delay; kết hợp với f33/f34 (vẫn lan rộng dù chậm) để bắt kịch bản né tránh.

**f18 — Mất cân bằng nửa cửa sổ (float)**  
Định nghĩa: \(N_1\)=số sự kiện trong \([T_s, T_s+\Delta/2)\), \(N_2\)=số sự kiện trong \([T_s+\Delta/2,T_e)\).  
\(f18=\frac{|N_1-N_2|}{N+\varepsilon}\).  
(A) Hữu ích khi ransomware “bùng pha” (đầu hoặc cuối cửa sổ) hoặc khi benign có pha chuẩn bị rồi thao tác; UNVEIL mô tả “access patterns” và pha hoạt động rõ rệt trong runtime. citeturn3view0  
(B) Bổ trợ: f18 hỗ trợ giảm phụ thuộc vào việc cửa sổ có cắt ngang pha hoạt động hay không; đi cùng f15/f14 để mô tả hình thái trong thời gian.

### Nhóm C — Ngữ nghĩa đường dẫn & mục tiêu

Các đặc trưng này dựa trên phân loại đường dẫn theo vùng Windows. Đây là cách “đưa tri thức hệ điều hành” vào features mà không cần metadata khác, phù hợp với gợi ý của bạn về Known Folders. citeturn6search0turn6search1

**f19 — Số sự kiện trong user_data (int)**  
Định nghĩa: \(f19 = |\{i: p_i \in user\_data\}|\), với user_data ≈ Documents/Desktop/Downloads/Pictures/Music/Videos (và UNC/network share) dưới `\Users\<*>\`.  
(A) Ransomware nhắm vào dữ liệu người dùng nên tương tác mạnh với các thư mục này; UNVEIL nhấn mạnh điều kiện “tamper with user’s files” để tấn công thành công. citeturn3view0turn5view0  
(B) Bổ trợ: Kết hợp f19 với entropy theo thư mục (f38) và unique_dirs (f34) để phân biệt “editor chỉnh một thư mục” với “traversal nhiều thư mục user”.

**f20 — Số Write trong user_data (int)**  
Định nghĩa: \(f20 = |\{i: o_i=W \land p_i \in user\_data\}|\).  
(A) Đây là tín hiệu trực tiếp của “sửa dữ liệu người dùng”; ransomware file locker buộc phải ghi/ghi đè. citeturn17search3turn3view0  
(B) Bổ trợ: Nếu benign backup/sync cũng Write nhiều vào user_data, thì rename patterns (f49–f52) và “dominant extension mới” (f50) giúp giảm nhầm.

**f21 — Số Delete trong user_data (int)**  
Định nghĩa: \(f21 = |\{i: o_i=D \land p_i \in user\_data\}|\).  
(A) Delete tài liệu người dùng có thể là wiper hoặc xóa bản gốc sau mã hóa; là dấu hiệu phá hoại hơn so với delete temp. citeturn17search13turn5view0  
(B) Bổ trợ: f21 nên đi cùng f47 (Write→Delete) và f37 (Gini) để phân biệt “xóa hàng loạt” với “xóa một nhóm nhỏ theo thao tác người dùng”.

**f22 — Số Rename trong user_data (int)**  
Định nghĩa: \(f22 = |\{i: o_i=R \land p_i \in user\_data\}|\) (dựa trên **old path**).  
(A) Hành vi đổi tên/append extension trên tài liệu người dùng là mẫu thường gặp; nhiều đo đạc cho thấy phần lớn mẫu ransomware append extension mới khi mã hóa. citeturn15view0  
(B) Bổ trợ: f22 mạnh hơn khi f49 cao (đổi extension), hoặc f50 cao (hầu hết rename ra cùng extension), gợi ý “đánh dấu file đã mã hóa”.

**f23 — Số sự kiện trong AppData (int)**  
Định nghĩa: \(f23 = |\{i: p_i \in appdata\}|\), appdata ≈ `\Users\<*>\AppData\Roaming\` hoặc `\Users\<*>\AppData\Local\` (không bao gồm Temp).  
(A) Nhiều benign (browser, app) hoạt động mạnh ở AppData; ngược lại, malware/persistence cũng hay thả cấu hình, payload ở đây. Do thiếu process name, f23 giúp phân biệt “hành vi nặng nhưng ở vùng hợp lệ”. citeturn6search1turn8view0  
(B) Bổ trợ: f23 không mang tính kết luận; nó bù cho f1/f3 bằng “bối cảnh”. Ví dụ cùng mức Write, nếu phần lớn nằm ở AppData/Temp thì ít đáng ngờ hơn so với user_data.

**f24 — Số sự kiện trong Temp (int)**  
Định nghĩa: \(f24 = |\{i: p_i \in temp/cache\}|\), temp/cache ≈ `\Users\<*>\AppData\Local\Temp\` (và các vùng cache/tạm tương đương theo taxonomy). citeturn6search1  
(A) Rất nhiều phần mềm benign ghi temp; ransomware cũng có thể dùng temp staging. Do đó cần đọc như “điểm neo benignness”, không phải “điểm malware”. citeturn15view0  
(B) Bổ trợ: Khi f6/f3 cao nhưng f24 cũng rất cao và f20 thấp, mô hình có thể giảm điểm nghi ngờ; ngược lại, nếu f24 thấp nhưng f20 cao, tăng nghi ngờ.

**f25 — Số sự kiện trong System (int)**  
Định nghĩa: \(f25 = |\{i: p_i \in system\}|\), system ≈ \Windows\ và/hoặc \Windows\System32\.  
(A) Sửa file hệ thống thường hiếm đối với app thường, và có thể là dấu hiệu persistence/tampering. citeturn6search0turn5view0  
(B) Bổ trợ: Kết hợp với các đặc trưng ngữ cảnh vùng system/program để phân biệt installer hợp lệ (có thể ghi) với hành vi bất thường khác (đặc biệt nếu đồng thời f19–f22 cao).

**f28 — Số root khác nhau (int)**  
Định nghĩa: root(p) = drive letter (`C:`) hoặc UNC share prefix (`\\server\share`). \(f28 = |\{root(p_i)\}|\).  
(A) Một số ransomware duyệt nhiều ổ (thậm chí A:..Z:) và nhiều thư mục khi mã hóa; số root tăng phản ánh traversal rộng. citeturn15view0  
(B) Bổ trợ: f28 bị yếu nếu cửa sổ ngắn hoặc malware giới hạn phạm vi; khi đó f34/f38 (unique_dirs/dir_entropy) và f27 (UNC) bù.

**f29 — Entropy theo root (float)**  
Định nghĩa: với \(q_r=\frac{|\{i:root(p_i)=r\}|}{N+\varepsilon}\),  
\(f29 = -\sum_r q_r\log_2(q_r+\varepsilon)\).  
(A) Entropy cao nghĩa là hoạt động phân tán trên nhiều ổ/share; trong bối cảnh ransomware “quét” rộng, đây là dấu cấu trúc tốt hơn chỉ đếm f28. citeturn15view0  
(B) Bổ trợ: Kết hợp với f24/f23 để phân biệt “phân tán nhưng chủ yếu ở cache” vs “phân tán vào user_data/UNC”.

**f30 — Số Write vào doc-like (int)**  
Định nghĩa: \(f30 = |\{i:o_i=W \land g(ext(p_i))=\text{doc}\}|\).  
(A) Nhiều ransomware nhắm tài liệu người dùng (doc/xls/ppt/pdf…) vì giá trị cao; I/O-based detection thường xét “file type coverage”. citeturn17search3turn4view0turn11search13  
(B) Bổ trợ: Benign cũng Write doc-like (soạn thảo); cần đi cùng f33 (unique_files) và f6/f14 (burst) để tách “edit 1 file” khỏi “đụng 1000 file”.

**f31 — Số Write vào executable-like (int)**  
Định nghĩa: \(f31 = |\{i:o_i=W \land g(ext(p_i))=\text{exe}\}|\).  
(A) Việc ghi lên exe/dll/sys có thể gợi ý dropper hoặc tampering hệ thống; ít thấy ở người dùng thông thường ngoài cài đặt/cập nhật. citeturn10view0turn6search8  
(B) Bổ trợ: f31 tương tác với f25/f26 (vùng system/program files) để tăng độ phân biệt; nếu Write exe-like nằm trong Temp/AppData có thể là installer cache, cần f24/f23 để giảm nhầm.

**f32 — Entropy nhóm extension trong các Write (float)**  
Định nghĩa: xét tập Write events, đếm theo nhóm \(k \in \{\text{doc,exe,archive,media,image,code,other}\}\):  
\(w_k=\frac{|\{i:o_i=W \land g(ext(p_i))=k\}|}{f3+\varepsilon}\),  
\(f32=-\sum_k w_k\log_2(w_k+\varepsilon)\).  
(A) Một số ransomware “funnel” vào nhóm file giá trị (doc/image) → entropy thấp; trong khi một số benign (backup) có thể trải rộng → entropy cao. Các thảo luận về “file type coverage” xuất hiện trong tổng quan I/O-based detection. citeturn17search3turn4view0  
(B) Bổ trợ: Vì entropy thấp cũng có thể do workflow chuyên biệt (ví dụ công cụ xử lý ảnh), nên cần kết hợp chặt với f19–f22 (mục tiêu thư mục) và f49–f50 (đổi extension).

### Nhóm D — Đa dạng & tập trung theo namespace

**f33 — Số file khác nhau (int)**  
Định nghĩa: \(F=\{norm(p_i)\}\) trên **full_path** của mọi sự kiện; \(f33=|F|\).  
(A) Ransomware thường tác động rất nhiều file; UNVEIL và các phân tích ransomware nhấn mạnh traversal rộng. citeturn3view0turn15view0  
(B) Bổ trợ: f33 kết hợp với f36/f37 (tập trung hay lan rộng) để phân biệt “đụng nhiều file” (ransomware/backup) vs “đụng ít file nhưng nhiều event” (DB).

**f34 — Số thư mục khác nhau (int)**  
Định nghĩa: \(D=\{dir(norm(p_i))\}\). \(f34=|D|\).  
(A) Ransomware thường đi qua nhiều thư mục; đo đạc trong nghiên cứu cũng cho thấy số thư mục “touched” lớn. citeturn15view0  
(B) Bổ trợ: f34 giảm nhầm với ứng dụng thao tác nhiều file nhưng trong một thư mục (ví dụ build system); kết hợp f43 (same_dir_adjacent_rate) để nhận ra traversal theo cụm.

**f35 — Số extension khác nhau (int)**  
Định nghĩa: \(X=\{ext(norm(p_i))\}\). \(f35=|X|\).  
(A) Ransomware có thể nhắm nhiều loại file; hoặc ngược lại chỉ nhắm một số loại giá trị—cả hai đều có ý nghĩa. citeturn17search3turn4view0  
(B) Bổ trợ: f35 phải đọc cùng f32 (entropy) để biết “nhiều extension” có phân tán thật hay chỉ vài nhóm chiếm ưu thế. Nó cũng bù cho f30 (doc-like) khi ransomware nhắm ảnh/media.

**f36 — Trung bình sự kiện trên mỗi file (float)**  
Định nghĩa: \(f36=\frac{N}{|F|+\varepsilon}\).  
(A) Ransomware kiểu “đi qua từng file một, làm ít thao tác rồi chuyển file khác” thường cho f36 gần 1–vài; ngược lại app xử lý một file nhiều lần cho f36 lớn. citeturn3view0turn5view0  
(B) Bổ trợ: f36 kết hợp với f33 để phân biệt “many-files shallow” vs “few-files deep”; bù cho f1 vốn chỉ nói tổng khối lượng.

**f37 — Gini của số sự kiện theo file (float)**  
Định nghĩa: đặt \(c_f = |\{i:norm(p_i)=f\}|\) cho mỗi \(f\in F\), \(k=|F|\).  
\(f37 = \frac{\sum_{a=1}^{k}\sum_{b=1}^{k} |c_a-c_b|}{2k\sum_{a=1}^{k}c_a+\varepsilon}\).  
(A) Gini thấp → phân bố đều trên nhiều file (ransomware “quét”); Gini cao → tập trung vào vài file (nhiều benign như DB/log). citeturn3view0turn15view0  
(B) Bổ trợ: Gini giúp chống né tránh bằng “thêm nhiễu vài file”: nếu malware cố đẩy công việc về ít file để bắt chước benign, f33/f34 và f19–f22 sẽ thay đổi theo hướng khác, tạo tương tác hữu ích.

**f38 — Entropy phân bố theo thư mục (float)**  
Định nghĩa: với \(p_d=\frac{|\{i:dir(norm(p_i))=d\}|}{N+\varepsilon}\),  
\(f38=-\sum_{d\in D} p_d\log_2(p_d+\varepsilon)\).  
(A) Entropy cao phản ánh lan tỏa trên nhiều thư mục; ransomware traversal thường làm tăng. citeturn15view0turn3view0  
(B) Bổ trợ: Entropy cao cũng có thể do tool index/search. Kết hợp với op mix (f7–f9), đặc biệt rename/ext-change (f49), để tăng độ chắc.

**f39 — Entropy phân bố theo extension (float)**  
Định nghĩa: với \(p_x=\frac{|\{i:ext(norm(p_i))=x\}|}{N+\varepsilon}\),  
\(f39=-\sum_{x\in X} p_x\log_2(p_x+\varepsilon)\).  
(A) Nhiều ransomware nhắm tập file cụ thể → entropy thấp; một số nhắm rộng → entropy cao; cả hai đều hữu ích khi đặt trong ngữ cảnh thư mục (f19–f22). citeturn17search3turn4view0  
(B) Bổ trợ: f39 bổ sung cho f35 bằng cách đo “độ đều”; giúp mô hình phân biệt “nhiều extension nhưng chủ yếu vẫn một nhóm” vs “thực sự đa dạng”.

**f40 — Độ sâu đường dẫn trung bình (float)**  
Định nghĩa: \(f40=\frac{1}{N+\varepsilon}\sum_{i=1}^{N} depth(norm(p_i))\).  
(A) Truy cập dữ liệu người dùng thường nằm sâu dưới `Users\<user>\...`; traversal đệ quy qua nhiều nhánh tăng độ sâu trung bình. Nghiên cứu về namespace/độ sâu thư mục trong hệ file cũng cho thấy cấu trúc phân cấp là một thuộc tính đo được và biến thiên theo workload. citeturn11search13turn3view0  
(B) Bổ trợ: depth bị phụ thuộc môi trường; do đó cần kết hợp với phân loại known-folder (Nhóm C) để “giải nghĩa” độ sâu (sâu vì user profile hay sâu vì cache).

**f41 — Độ lệch chuẩn độ sâu đường dẫn (float)**  
Định nghĩa: \(f41=\sqrt{\frac{1}{N+\varepsilon}\sum_i (depth(norm(p_i)) - f40)^2}\).  
(A) Độ lệch chuẩn cao gợi ý tiến trình đụng cả file ở các mức rất khác nhau (root + sâu), có thể tương ứng traversal rộng. citeturn15view0  
(B) Bổ trợ: Kết hợp với f28/f29 (root diversity) để phân biệt “nhiều mức trong một ổ” vs “nhiều mức trên nhiều ổ/share”.

**f42 — Tỷ lệ thư mục chiếm ưu thế nhất (float)**  
Định nghĩa: \(f42=\frac{\max_{d\in D} |\{i:dir(norm(p_i))=d\}|}{N+\varepsilon}\).  
(A) Ransomware traversal thường giảm sự “độc chiếm” của một thư mục; ngược lại nhiều benign workload tập trung vào một working dir/cache dir. citeturn3view0turn15view0  
(B) Bổ trợ: f42 tương tác với f38: entropy thấp thường kéo theo f42 cao; kết hợp cả hai giúp mô hình không cần học quá nhiều từ raw path tokens (giảm rủi ro spurious feature). citeturn8view0

### Nhóm E — Cấu trúc chuỗi (sequential structure)

Các đặc trưng này dựa trên thứ tự thời gian của sự kiện. Lưu ý: đặc trưng chuỗi thuần túy có thể bị né tránh bằng cách chèn “noise operations” hoặc đổi thứ tự độc lập—điểm yếu đã được nêu trong nghiên cứu về mô hình hành vi dựa trên system call sequence. citeturn10view0 Vì vậy, chúng được thiết kế để **phối hợp** với Nhóm C/D/F.

**f43 — Tỷ lệ hai sự kiện liên tiếp cùng thư mục (float)**  
Định nghĩa: nếu \(N<2\) thì 0, ngược lại  
\(f43=\frac{|\{i: dir(p_i)=dir(p_{i+1})\}|}{N-1}\).  
(A) Ransomware thường xử lý nhiều file trong cùng thư mục trước khi chuyển sang thư mục khác → f43 có thể cao; đồng thời nhiều benign scan/index có thể thấp (đổi thư mục liên tục). citeturn15view0  
(B) Bổ trợ: f43 kết hợp với f34/f38 để phân biệt “làm sâu trong từng thư mục” vs “nhảy thư mục”; và kết hợp với f49–f50 để nhận ra pattern “append extension hàng loạt trong cùng folder”.

**f44 — Tỷ lệ hai sự kiện liên tiếp cùng full_path (float)**  
Định nghĩa: \(f44=\frac{|\{i: norm(p_i)=norm(p_{i+1})\}|}{N-1}\) (0 nếu \(N<2\)).  
(A) App như DB/log writer thường có nhiều thao tác lặp trên cùng file → f44 cao; ransomware quét nhiều file → f44 thấp. citeturn3view0turn5view0  
(B) Bổ trợ: f44 bổ sung cho f37 (Gini): f37 nhìn phân bố toàn cục, f44 nhìn “tính cục bộ theo thời gian”; cả hai cùng giúp phân biệt “few-files deep” vs “many-files shallow”.

**f45 — Xác suất chuyển Create→Write (float)**  
Định nghĩa: \(f45=\frac{|\{i:o_i=C \land o_{i+1}=W\}|}{N-1+\varepsilon}\).  
(A) Nhiều xử lý hợp lệ lẫn malware có Create→Write; nhưng trong ransomware kiểu “tạo bản mã hóa mới rồi thao tác”, mẫu này có thể tăng. citeturn17search13turn3view0  
(B) Bổ trợ: f45 mạnh hơn khi đồng thời f4/f47 (Delete sau đó) tăng, tạo motif “Create→Write→Delete” trên nhiều file; motif dạng này bù cho thiếu read/entropy.

**f46 — Xác suất chuyển Write→Rename (float)**  
Định nghĩa: \(f46=\frac{|\{i:o_i=W \land o_{i+1}=R\}|}{N-1+\varepsilon}\).  
(A) Nhiều ransomware ghi ciphertext rồi đổi tên/append extension để đánh dấu; quan sát về delete/rename sau khi tạo ciphertext được mô tả trong phân tích ransomware. citeturn17search13turn15view0  
(B) Bổ trợ: f46 kết hợp trực tiếp với f49/f50: nếu Write→Rename cao và đa số rename đổi extension theo một pattern, độ phân biệt tăng mạnh; nếu malware “không rename”, f46 không giúp, lúc đó dùng f47 + f30/f20.

**f47 — Xác suất chuyển Write→Delete (float)**  
Định nghĩa: \(f47=\frac{|\{i:o_i=W \land o_{i+1}=D\}|}{N-1+\varepsilon}\).  
(A) Gợi ý hành vi overwrite rồi xóa hoặc tạo file mới rồi xóa bản cũ (trong cửa sổ quan sát); phù hợp với mô tả “delete original after producing encrypted file”. citeturn17search13turn5view0  
(B) Bổ trợ: Write→Delete đôi khi benign (temp file). Vì vậy phải gắn với vùng user_data (f21) và “rename ext-change” thấp/cao để suy ra đúng motif (wiper vs ransomware vs temp cleanup).

**f48 — Run dài nhất của cùng op_type (int)**  
Định nghĩa: xét chuỗi \(o_1..o_N\), \(f48=\max\) độ dài đoạn liên tiếp có cùng op_type.  
(A) Ransomware thường có “đoạn dài” bị chi phối bởi Write hoặc Rename (một kiểu thao tác lặp lại hàng loạt). UNVEIL quan sát các mẫu I/O lặp mạnh do chiến lược tấn công giống nhau cho mỗi file. citeturn3view0turn5view0  
(B) Bổ trợ: Run-length là đặc trưng chuỗi tương đối bền trước việc đổi thứ tự giữa các nhóm file (nếu vẫn lặp cùng loại thao tác). Kết hợp với f10 (op entropy) để phân biệt “một run dài” với “phân bố đều”.

### Nhóm F — Rename & nhân bản tên file

**f49 — Tỷ lệ Rename đổi extension (float)**  
Định nghĩa: nếu \(f5=0\) thì 0, ngược lại  
\(f49=\frac{|\{i:o_i=R \land ext(p_i)\ne ext(p_i^{new})\}|}{f5+\varepsilon}\).  
(A) Đổi extension là dấu hiệu mạnh của “file type changing/marking” trong nhiều mẫu ransomware; đo đạc gần đây cho thấy đa số mẫu append extension mới. citeturn15view0turn17search3  
(B) Bổ trợ: Nếu malware randomize extension để né f50, f49 vẫn có thể cao; nếu malware không rename, f49=0 và cần dựa vào f20/f30/f33/f38.

**f50 — Dominant new-extension ratio trong Rename (float)**  
Định nghĩa: với mỗi extension \(e\), \(u_e = |\{i:o_i=R \land ext(p_i^{new})=e\}|\).  
\(f50=\frac{\max_e u_e}{f5+\varepsilon}\) (0 nếu \(f5=0\)).  
(A) Nhiều ransomware gắn một extension “thương hiệu” cho toàn bộ file đã mã hóa → một \(e\) chiếm ưu thế. citeturn15view0  
(B) Bổ trợ: f50 chống false positive của batch-rename “đa dạng” (ví dụ đổi tên ảnh có giữ extension) vì khi benign rename mà không đổi extension, f49 thấp. Cặp (f49,f50) là tương tác then chốt.

**f51 — Độ giống tiền tố tên file cũ–mới trung bình (float)**  
Định nghĩa: với rename event \(i\), đặt \(a=fname(p_i), b=fname(p_i^{new})\), \(LCP(a,b)\)=độ dài common prefix theo ký tự.  
\(s_i=\frac{LCP(a,b)}{\max(|a|,1)}\).  
\(f51=\text{mean}(s_i)\) trên mọi rename.  
(A) Append extension kiểu `report.docx` → `report.docx.locked` tạo LCP rất cao; đây là một fingerprint “string-level” không cần nội dung/bytes. citeturn15view0  
(B) Bổ trợ: Nếu malware đổi tên hoàn toàn (LCP thấp) thì f51 yếu; khi đó f49 (đổi extension) và f34/f38 (lan rộng) vẫn hỗ trợ. Đồng thời f51 giúp chống benign rename có mẫu khác (ví dụ đổi tên theo template nhưng không append).

**f52 — Tỷ lệ Rename giữ nguyên thư mục (float)**  
Định nghĩa: \(f52=\frac{|\{i:o_i=R \land dir(p_i)=dir(p_i^{new})\}|}{f5+\varepsilon}\) (0 nếu \(f5=0\)).  
(A) Nhiều ransomware chỉ đổi tên/extension ngay tại chỗ (same directory) thay vì di chuyển; do đó f52 thường cao trong append-extension scenario. citeturn15view0turn17search13  
(B) Bổ trợ: f52 kết hợp với f22 (rename trong user_data) giúp phân biệt “append extension tại chỗ” (ransomware) với “move/organize” (benign file manager).

**f53 — Mức nhân bản filename khi Create (int)**  
Định nghĩa: xét tập Create events \(I_C=\{i:o_i=C\}\). Với mỗi filename \(u=fname(norm(p_i))\), đặt \(S_u=\{dir(norm(p_i)) : i\in I_C \land fname(norm(p_i))=u\}\).  
\(f53=\max_u |S_u|\) (nếu không có Create thì 0).  
(A) Ransomware thường tạo **ransom note** với cùng tên trong nhiều thư mục; đặc trưng này bắt tín hiệu đó mà không cần biết tên note cụ thể. citeturn17search13turn15view0  
(B) Bổ trợ: f53 bù cho trường hợp ransomware không rename extension (f49 thấp) nhưng vẫn thả note; hoặc ngược lại, nếu note name ngẫu nhiên, f53 giảm và ta dựa vào f33/f38/f46. Nó cũng giúp giảm false positive của installer: installer ít khi tạo cùng một filename ở hàng trăm thư mục user.

## Tập đặc trưng lõi khuyến nghị

Một tập “lõi” (ít nhưng mạnh) nên ưu tiên các đặc trưng vừa (i) bắt được **bản chất bắt buộc** của ransomware (tamper nhiều file người dùng), vừa (ii) ít phụ thuộc môi trường, vừa (iii) có tính bổ trợ để chống false positive của backup/sync.

Tập lõi đề xuất (15 đặc trưng, lấy từ master):

- **Cường độ & mix:** f3 (Write), f4 (Delete), f5 (Rename), f6 (rate_total), f10 (op entropy).  
- **Temporal:** f14 (VMR_10), f17 (inactivity_frac).  
- **Targeting:** f20 (Write trong user_data), f22 (Rename trong user_data), f24 (Temp activity), f27 (UNC activity).  
- **Lan rộng:** f33 (unique_files), f34 (unique_dirs), f37 (Gini per-file), f38 (dir entropy).  
- **Rename cấu trúc:** f49 (ext-change frac), f50 (dominant new-ext ratio) *hoặc* f53 (create filename replication) tùy mục tiêu ransomware/không.

Lý do tập này mạnh:

- Nó ghép thành “tam giác” **cường độ (f6/f3) + lan rộng (f33/f38/f37) + mục tiêu (f20/f22 vs f24)**, bám sát insight rằng ransomware phải đụng vào file nạn nhân và thường thực hiện hàng loạt. citeturn3view0turn5view0turn4view0  
- Nó bổ sung một lớp “tín hiệu rename” (f49/f50) vốn rất hữu ích khi thiếu entropy/bytes; đồng thời có “escape hatch” f53 cho biến thể tạo note. citeturn15view0turn17search13  
- Nó giảm phụ thuộc vào dữ liệu sandbox: phần lớn dựa trên tỷ lệ/entropy/phân bố thay vì token cụ thể của path, phù hợp với khuyến nghị hạn chế học “spurious features” và xử lý phân phối endpoint khác nhau. citeturn8view0turn16search3  

## Họ đặc trưng mở rộng tùy chọn

Các họ dưới đây hữu ích khi bạn muốn tăng “độ bắt được biến thể” hoặc tăng chống né tránh, đổi lại vector lớn hơn và/hoặc tính toán nặng hơn. Tất cả vẫn computable từ strict schema.

### Histogram mặt nạ hành vi theo file

Ý tưởng: với mỗi file \(f\), xét trong cửa sổ các op-type đã xảy ra trên file đó, tạo mặt nạ 4-bit:  
\(\text{mask}(f)=1[C\in S_f]+2[W\in S_f]+4[D\in S_f]+8[R\in S_f]\) (16 trạng thái).  
Tạo histogram 16 chiều và entropy của mask distribution.

- Lợi ích: gần với “per-file access pattern repetitiveness” mà UNVEIL khai thác (dù UNVEIL có thêm read/entropy), nhưng ở mức coarse, vẫn có thể bắt motif như Write+Rename, Create+Write+Delete lặp trên hàng loạt file. citeturn3view0turn17search3  
- Trade-off: cần group by file path trong cửa sổ; rename làm “đứt” identity file (không có file ID), nhưng vẫn hữu ích nếu dùng old-path nhất quán hoặc nối bằng edges rename trong cửa sổ.

### Đặc trưng “tốc độ lan rộng theo thư mục”

Từ chuỗi thư mục theo thời gian \(d_i=dir(p_i)\), đo:
- số lần chuyển thư mục (switch count),
- phân bố độ dài “đoạn liên tiếp cùng thư mục”,
- khoảng thời gian trung bình spent trong một thư mục (từ timestamp đầu tới cuối của các event trong thư mục đó trong cửa sổ).

Lợi ích: phân biệt traversal kiểu ransomware (ở mỗi thư mục xử lý nhiều file rồi đi tiếp) với nhiều benign workload (ví dụ DB log: gần như một thư mục). citeturn15view0turn5view0  

### Vector hóa đường dẫn bằng hashing n-gram

Tạo **vector cố định kích thước \(M\)** (ví dụ 1024/4096) từ token/n-gram ký tự của \(norm(full\_path)\) hoặc \(dir\) hoặc \(ext\). Mỗi chiều \(j\) là tổng số lần các token hash vào \(j\). Điều này tương tự cách một số mô hình coi “behavior report như văn bản” để học đặc trưng tự động. citeturn9search7turn8view0  

- Lợi ích: bắt được tín hiệu tinh hơn (tên thư mục, pattern filename “random-looking”, GUID-like) mà không cần metadata bổ sung.  
- Rủi ro: dễ học artifact môi trường (đặc biệt sandbox) và giảm tổng quát; cần regularization, feature normalization, và đánh giá chéo theo máy/organization. citeturn8view0turn16search3  

### Mô hình point process/Hawkes đơn giản trên timestamps

Fitting một Hawkes process hoặc mô hình point process đơn giản để lấy tham số “self-excitation/branching ratio” (đặc trưng clustering) từ \(t_i\). Hawkes được dùng để mô tả chuỗi sự kiện có tính “kích hoạt” và clustering. citeturn12search18turn13search7  

- Lợi ích: tóm tắt temporal dynamics bằng vài tham số thay vì nhiều thống kê rời rạc.  
- Trade-off: fitting nặng, nhạy với N nhỏ, và không trực tiếp dùng ngữ nghĩa đường dẫn—vì vậy chỉ nên là feature bổ sung.

## Cân nhắc đối kháng và né tránh

### Benign có thể “giống ransomware” như thế nào

Một số phần mềm hợp lệ có thể tạo dấu vết gần như ransomware nếu chỉ nhìn count/rate:

- **Backup/sync clients**: chạm hàng loạt file, có thể rename/replace/cleanup, thậm chí thao tác trên network shares.  
- **Công cụ batch rename / organizer ảnh**: rename hàng loạt trong user_data.  
- **Công cụ mã hóa hợp lệ / nén giải nén lớn**: write dày và theo burst.  

Ngay cả CryptoDrop cũng thừa nhận giới hạn cơ bản: hệ thống quan sát thay đổi dữ liệu khó “hiểu ý định” và cần phối hợp nhiều chỉ báo để giảm false positives. citeturn4view0

Cách giảm nhầm trong khung feature của bạn là **bắt tương tác**:
- Nếu “nặng nhưng lành”: thường tập trung ở AppData/Temp (f23/f24 cao), Gini cao (f37 cao) hoặc f44 cao (lặp cùng file), rename không đổi extension (f49 thấp).  
- Nếu “nặng và phá”: Write/Delete/Rename cao (f3/f4/f5), lan rộng (f33/f38), tập trung vào user_data/UNC (f20/f27), và có dấu hiệu đổi extension hàng loạt (f49/f50) hoặc tạo cùng filename trên nhiều thư mục (f53). citeturn5view0turn3view0turn15view0turn17search13  

### Malware có thể né các đặc trưng “ngây thơ” ra sao

- **Throttling/slow encryption:** giảm f6 và làm f11 lớn, tăng f17; đây là kỹ thuật né dễ nhất đối với detector dựa trên rate.  
  - Giảm brittleness bằng cách dựa thêm vào lan rộng/entropy thư mục (f33/f38) và dấu rename cấu trúc (f49–f52).  
- **Chèn nhiễu / xen thao tác vô hại:** tương tự vấn đề đã được nêu trong nghiên cứu về mô hình chuỗi system call: chuỗi thuần túy có thể bị phá bằng reorder/chèn call độc lập. citeturn10view0  
  - Khắc phục bằng các đặc trưng phân bố (entropy/Gini) vốn khó “đánh lừa” nếu malware vẫn phải xử lý hàng loạt file thật.  
- **Randomize extension hoặc không đổi extension:** làm suy yếu f50/f49.  
  - Bù bằng motif Write→Delete (f47), lan rộng (f33/f34), và “tạo file note hàng loạt” (f53) nếu tồn tại. citeturn17search13turn15view0  
- **Chia nhỏ hành vi qua nhiều PID:** vì bạn quan sát “mỗi PID độc lập”, attacker có thể dùng multi-process để làm mỗi PID trông bình thường.  
  - Đây là giới hạn cấu trúc của bài toán (không giải bằng feature trong schema). Nếu giữ nguyên ràng buộc “một PID”, cách giảm tác hại là chọn cửa sổ đủ dài và tận dụng rename/note replication vốn khó phân tán hoàn toàn.  

### Rủi ro từ dữ liệu huấn luyện và triển khai

Nhiều nghiên cứu gần đây chỉ ra khoảng cách hiệu năng lớn khi mô hình hành vi huấn luyện trên sandbox đem ra endpoint do **distribution shift, label noise, và spurious features**; ngoài ra malware còn có thể né sandbox bằng đặc trưng “wear-and-tear”. citeturn8view0turn16search3turn16search6  

Hệ quả cho feature engineering trong schema của bạn:

- Ưu tiên đặc trưng **tương đối/chuẩn hóa** (tỷ lệ, entropy, Gini, VMR) hơn là token path cụ thể.  
- Các đặc trưng dựa trên “Known Folder” (Nhóm C) hữu ích nhưng cần viết rule robust (không hardcode username) và nên coi là “context features” thay vì quyết định một mình. citeturn6search0turn6search1turn8view0  
- Với đối thủ chủ động, ML model có thể bị tấn công né tránh; thực hành phổ biến là adversarial training/hardening, vốn cũng được thảo luận trong bối cảnh malware ML evasion. citeturn16search0