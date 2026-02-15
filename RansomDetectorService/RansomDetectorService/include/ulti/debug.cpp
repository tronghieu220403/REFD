#include "debug.h"
#include "support.h"

namespace debug {

	std::mutex file_debug_mutex;
	HANDLE log_file_handle = INVALID_HANDLE_VALUE;
	int debug_count = 0;

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
		std::lock_guard<std::mutex> lock(file_debug_mutex);

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

			// Never write into message.c_str(); that is undefined behavior.
			std::wstring line = message;
			if (line.empty() || line.back() != L'\n') {
				line.push_back(L'\n');
			}

			DWORD bytes_written = 0;
			WriteFile(log_file_handle, line.data(), static_cast<DWORD>(line.size() * sizeof(wchar_t)), &bytes_written, nullptr);
			//FlushFileBuffers(log_file_handle);

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
#ifndef _DEBUG
		return;
#endif // !_DEBUG
		if (pwsz_format == nullptr) return;

		// Reusable buffer per thread
		thread_local std::vector<wchar_t> buffer;
		buffer.clear();

		// Build timestamp string first
		SYSTEMTIME time;
		GetLocalTime(&time);

		wchar_t time_str[64];
		int prefix_len = swprintf_s(time_str, 64,
			L"[%d/%02d/%02d - %02d:%02d:%02d][REFD] ",
			time.wYear, time.wMonth, time.wDay,
			time.wHour, time.wMinute, time.wSecond);

		if (prefix_len <= 0) return;

		va_list args;
		va_start(args, pwsz_format);

		va_list args_copy;
		va_copy(args_copy, args);
		int msg_len = _vscwprintf(pwsz_format, args_copy);
		va_end(args_copy);

		if (msg_len <= 0) {
			va_end(args);
			return;
		}

		// Allocate buffer once: prefix + message + null terminator
		buffer.resize(prefix_len + msg_len + 1);

		// Copy prefix into buffer
		wmemcpy(buffer.data(), time_str, prefix_len);

		// Format message into buffer right after prefix
		vswprintf_s(buffer.data() + prefix_len, msg_len + 1, pwsz_format, args);

		va_end(args);

		// Null-terminated already, safe to print
		OutputDebugStringW(buffer.data());
		//WriteDebugToFileW(buffer.data());
	}

	void WriteLogW(const wchar_t* format, ...)
	{
		if (!format)
			return;

		// 1KB max message per thread
		constexpr size_t kMaxChars = 1024;

		thread_local std::wstring buffer;

		// Ensure capacity once
		if (buffer.capacity() < kMaxChars)
			buffer.reserve(kMaxChars);

		buffer.resize(kMaxChars);

		va_list args;
		va_start(args, format);

		int written = vswprintf_s(
			buffer.data(),
			buffer.size(),
			format,
			args
		);

		va_end(args);

		if (written <= 0)
			return;  // failed or truncated

		buffer.resize(static_cast<size_t>(written));

		WriteDebugToFileW(buffer.c_str());
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
