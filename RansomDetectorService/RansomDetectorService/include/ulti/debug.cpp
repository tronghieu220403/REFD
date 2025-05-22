#include "debug.h"
#include "support.h"

namespace debug {

	std::mutex debug_mutex;
	HANDLE log_file_handle = INVALID_HANDLE_VALUE;
	int debug_count = 0;
	std::wstring log;

	void InitDebugLog()
	{
		if (log_file_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(log_file_handle);
		}
		log_file_handle = CreateFileW(
			LOG_PATH,
			FILE_APPEND_DATA,
			FILE_SHARE_READ,
			nullptr,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);
		if (log_file_handle == INVALID_HANDLE_VALUE || log_file_handle == 0)
		{
			// Handle error (optional: log to another output)
		}
		debug_count = 0;
	}

	void CleanupDebugLog()
	{
		if (log_file_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(log_file_handle);
			log_file_handle = INVALID_HANDLE_VALUE;
		}
	}

	void WriteDebugToFileW(const std::wstring& message)
	{
		try {
			if (log_file_handle == INVALID_HANDLE_VALUE || log_file_handle == 0)
			{
				InitDebugLog();
				if (log_file_handle == INVALID_HANDLE_VALUE || log_file_handle == 0)
				{
					return;
				}
			}
			debug_count++;

            size_t size = wcslen(message.c_str());

            WCHAR* buffer = (WCHAR*)message.c_str();

            if (size == 0 || buffer[size - 1] != L'\n') {
                buffer[size] = L'\n';
                size++;
            }

			DWORD bytes_written = 0;
			WriteFile(log_file_handle, buffer, size * sizeof(wchar_t), &bytes_written, nullptr);
			FlushFileBuffers(log_file_handle);

			if (debug_count >= DEBUG_LOG_THRESHOLD) {
				CleanupDebugLog();
				InitDebugLog();
			}
		}
		catch (...) {
			// Handle exception if needed
		}
	}

	void DebugPrintW(const wchar_t* pwsz_format, ...) {
		if (pwsz_format == nullptr) return;

		std::lock_guard<std::mutex> lock(debug_mutex);
		if (log.size() < 16384 * 512)
		{
			log.resize(16384 * 512);
		}

		va_list args;
		va_start(args, pwsz_format);
		int len = 0;
		while (true) {
			try {
				len = vswprintf_s(&log[0], log.size(), pwsz_format, args);
			}
			catch (...) {
				if (errno == ERANGE) {
					log.resize(log.size() * 2);
					continue;
				}
			}
			break;
		}
		if (len > 0) {
			SYSTEMTIME time;
			GetLocalTime(&time);

			wchar_t time_str[64];
			swprintf_s(time_str, 64, L"[%d/%02d/%02d - %02d:%02d:%02d][RansomDetectorService] ",
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond);

			log.insert(0, time_str);
			OutputDebugStringW(log.c_str());
			WriteDebugToFileW(log.c_str());
		}

		va_end(args);
	}

	std::wstring GetErrorMessage(DWORD errorCode) {
		LPWSTR messageBuffer = nullptr;
		size_t size = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&messageBuffer, 0, nullptr);

		std::wstring message(messageBuffer, size);
		LocalFree(messageBuffer);
		return message;
	}

}  // namespace debug
