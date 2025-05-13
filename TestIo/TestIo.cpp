#include <windows.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>
#include <string>
#include <sstream>

namespace fs = std::filesystem;
const std::string TEST_DIR = "E:\\test";
const size_t RANDOM_SIZE = 5 * 1024; // 5 KB

// Sinh buffer ngẫu nhiên gồm các ký tự printable [32..126]
std::string genRandomData(size_t size) {
    static std::mt19937_64 rng((unsigned)time(nullptr));
    static std::uniform_int_distribution<int> dist(32, 126);
    std::string s;
    s.reserve(size);
    for (size_t i = 0; i < size; ++i)
        s.push_back(char(dist(rng)));
    return s;
}

// Viết RANDOM_SIZE bytes vào file path
void writeRandom(const std::string& path) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        std::cerr << "  [!] Cannot open for random write: " << path << "\n";
        return;
    }
    ofs.write(genRandomData(RANDOM_SIZE).data(), RANDOM_SIZE);
    ofs.close();
}

// Xóa hết trong thư mục TEST_DIR
void cleanDirectory() {
    for (auto& p : fs::directory_iterator(TEST_DIR))
        fs::remove_all(p);
    std::cout << "[*] Directory cleaned\n";
}

// Tạo 9 file test_i.txt và ghi 5KB ngẫu nhiên
void createTestFiles() {
    for (int i = 1; i <= 20; i++) {
        std::string path = TEST_DIR + "\\test_" + std::to_string(i) + ".txt";
        writeRandom(path);
    }
    std::cout << "[*] files created with random data\n";
}

// Xử lý một test theo số
void runTest(int test) {
    srand((unsigned)time(NULL));

    std::string base = TEST_DIR + "\\test_" + std::to_string(test) + ".txt";
    if (test != 10 && test != 9)
        writeRandom(base);
    switch (test) {
    case 1: {   // Write
        HANDLE h = CreateFileA(base.c_str(),
            GENERIC_WRITE, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { std::cerr << "[1] Open failed\n"; break; }
        std::string data = "Test Write\n";
        DWORD w;
        WriteFile(h, data.data(), DWORD(data.size()), &w, NULL);
        CloseHandle(h);
        std::cout << "[1] Performed Write\n";
        break;
    }
    case 2: {   // Rename
        std::string to = TEST_DIR + "\\test_2_renamed.txtggez";
        if (fs::exists(base)) {
            fs::rename(base, to);
            std::cout << "[2] Renamed\n";
        }
        else std::cerr << "[2] File missing\n";
        break;
    }
    case 3: {   // Delete
        if (DeleteFileA(base.c_str()))
            std::cout << "[3] Deleted\n";
        else std::cerr << "[3] Delete failed\n";
        break;
    }
    case 4: {   // Create new + random write
        HANDLE h = CreateFileA(base.c_str(),
            GENERIC_READ | GENERIC_WRITE, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            std::cerr << "[4] Create new failed\n";
        }
        else {
            CloseHandle(h);
            writeRandom(base);
            std::cout << "[4] Created new + random data\n";
        }
        break;
    }
    case 5: {   // Memory‐map no write
        HANDLE h = CreateFileA(base.c_str(),
            GENERIC_READ | GENERIC_WRITE, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { std::cerr << "[5] Open failed\n"; break; }
        HANDLE m = CreateFileMappingA(h, NULL, PAGE_READWRITE, 0, 0, NULL);
        if (!m) { std::cerr << "[5] Map failed\n"; CloseHandle(h); break; }
        LPVOID v = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0);
        if (v) { UnmapViewOfFile(v); std::cout << "[5] Mapped (no write)\n"; }
        CloseHandle(m); CloseHandle(h);
        break;
    }
    case 6: {   // Async write + rename
        HANDLE h = CreateFileA(base.c_str(),
            GENERIC_WRITE, 0, NULL,
            OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
        if (h == INVALID_HANDLE_VALUE) { std::cerr << "[6] Open fail\n"; break; }
        OVERLAPPED ov{ 0 }; ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        const char* buf = "Async Write\n"; DWORD sent;
        WriteFile(h, buf, DWORD(strlen(buf)), &sent, &ov);
        MoveFileA(base.c_str(), (TEST_DIR + "\\test_6_renamed.txt").c_str());
        WaitForSingleObject(ov.hEvent, INFINITE);
        CloseHandle(ov.hEvent); CloseHandle(h);
        std::cout << "[6] Async write + renamed\n";
        break;
    }
    case 7: {   // Create new + write (constant)
        HANDLE h = CreateFileA(base.c_str(),
            GENERIC_WRITE, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { std::cerr << "[7] Fail\n"; break; }
        const char* txt = "Create+Write\n"; DWORD w;
        WriteFile(h, txt, DWORD(strlen(txt)), &w, NULL);
        CloseHandle(h);
        std::cout << "[7] Created & wrote const text\n";
        break;
    }
    case 8: {   // Mem‐map write
        HANDLE h = CreateFileA(base.c_str(),
            GENERIC_READ | GENERIC_WRITE, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { std::cerr << "[8] Open fail\n"; break; }
        LARGE_INTEGER L; L.QuadPart = 1024;
        SetFilePointerEx(h, L, NULL, FILE_BEGIN);
        SetEndOfFile(h);
        HANDLE m = CreateFileMappingA(h, NULL, PAGE_READWRITE, 0, 0, NULL);
        LPVOID v = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0);
        if (v) {
            memcpy(v, "MemMap Write\n", 13);
            FlushViewOfFile(v, 0);
            UnmapViewOfFile(v);
            std::cout << "[8] Mem‐mapped write\n";
        }
        CloseHandle(m); CloseHandle(h);
        break;
    }
    case 9: {   // Mem‐map + rename handle open
        HANDLE h = CreateFileA(base.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { std::cerr << "[9] Open fail\n"; break; }

        // Set file size to 100MB
        LARGE_INTEGER fileSize;
        fileSize.QuadPart = 100 * 1024 * 1024; // 100MB
        if (!SetFilePointerEx(h, fileSize, NULL, FILE_BEGIN) || !SetEndOfFile(h)) {
            std::cerr << "[9] Failed to set file size\n";
            CloseHandle(h);
            break;
        }

        Sleep(5000);

        // Create file mapping with full 100MB
        HANDLE m = CreateFileMappingA(h, NULL, PAGE_READWRITE, 0, 100 * 1024 * 1024, NULL);
        if (!m) {
            std::cerr << "[9] CreateFileMapping failed\n";
            CloseHandle(h);
            break;
        }

        LPVOID v = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 100 * 1024 * 1024);
        if (v) {
            // Ghi dữ liệu mẫu vào đầu file (không cần ghi toàn bộ 100MB)
            char c = 'A' + rand() % 25;
            memset(v, 'A', 100 * 1024 * 1024);
            //FlushViewOfFile(v, 0);

            MoveFileA(base.c_str(), (TEST_DIR + "\\test_9_renamed.txt").c_str());
            UnmapViewOfFile(v);
            std::cout << "[9] Mem-mapped + renamed\n";
        } else {
            std::cerr << "[9] MapViewOfFile failed\n";
        }

        CloseHandle(m);
        CloseHandle(h);
        break;
    }
    case 10: { // Create new + rename
        HANDLE h = CreateFileA(base.c_str(),
            SPECIFIC_RIGHTS_ALL | STANDARD_RIGHTS_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { std::cerr << "[10] Create new failed: " << GetLastError() << "\n";  break; }
        std::string to = TEST_DIR + "\\test_10.txtggez";
        // Ghi 5KB ký tự printable ngẫu nhiên (ví dụ ký tự 'A')
        DWORD written = 0;
        std::string data(5120, 'A');
        WriteFile(h, data.c_str(), data.size(), &written, NULL);

        // Chuẩn bị cấu trúc FILE_RENAME_INFO
        std::wstring newName = L"test_10.txtggez";
        std::wstring fullNewPath = L"\\??\\" + std::wstring(TEST_DIR.begin(), TEST_DIR.end()) + L"\\" + newName;

        size_t size = sizeof(FILE_RENAME_INFO) + fullNewPath.size() * sizeof(wchar_t);
        FILE_RENAME_INFO* fni = (FILE_RENAME_INFO*)malloc(size);
        if (!fni) {
            std::cerr << "[10] Memory allocation failed\n";
            CloseHandle(h);
            break;
        }
        ZeroMemory(fni, size);
        fni->ReplaceIfExists = TRUE;
        fni->RootDirectory = NULL;
        fni->FileNameLength = (DWORD)(fullNewPath.size() * sizeof(wchar_t));
        memcpy(fni->FileName, fullNewPath.c_str(), fni->FileNameLength);

        // Rename file
        if (SetFileInformationByHandle(h, FileRenameInfo, fni, (DWORD)size)) {
            std::cout << "[10] Created new + renamed\n";
        }
        else {
            std::cerr << "[10] Rename failed: " << GetLastError() << "\n";
        }

        free(fni);
        CloseHandle(h);
        break;
    }
    default:
        std::cerr << "[!] Unknown test: " << test << "\n";
    }
}

int main(int argc, char* argv[]) {
    // Chuẩn bị thư mục
    if (!fs::exists(TEST_DIR)) fs::create_directories(TEST_DIR);

    auto processTokens = [&](const std::vector<std::string>& toks) {
        for (auto& tok : toks) {
            if (tok == "d")           cleanDirectory();
            else if (tok == "c")      createTestFiles();
            else if (int t = std::atoi(tok.c_str()); t >= 1 && t <= 20)
                runTest(t);
            else if (tok == "exit" || tok == "quit") return false;
            else std::cerr << "[!] Invalid token: " << tok << "\n";
        }
        return true;
        };

    // Xử lý argv
    if (argc > 1) {
        std::vector<std::string> tokens;
        for (int i = 1; i < argc; i++) tokens.emplace_back(argv[i]);
        if (!processTokens(tokens)) return 0;
    }
    return 0;
    // Vào chế độ interactive
    std::cout << "\n=== Interactive mode (type d, c, 1..9, exit) ===\n";
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        std::istringstream iss(line);
        std::vector<std::string> toks;
        for (std::string w; iss >> w;) toks.push_back(w);
        if (!processTokens(toks)) break;
    }

    return 0;
}
