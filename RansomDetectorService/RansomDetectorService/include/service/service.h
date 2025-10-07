#ifdef _WIN32

#ifndef SERVICE_SERVICE_H_
#define SERVICE_SERVICE_H_

#include "ulti/support.h"
#include "ulti/debug.h"

constexpr auto SERVICE_CONTROL_START = 128;

namespace srv
{
	class Service
	{
	private:
		std::wstring service_name_;
		std::wstring service_path_;
		SC_HANDLE h_services_control_manager_ = NULL;
		DWORD type_ = NULL;
		DWORD start_type_ = NULL;

	public:

		Service() = default;

		DWORD Create(const std::wstring& service_path, const DWORD& type, const DWORD& start_type);
		DWORD Run();
		DWORD Stop();
		DWORD Delete();

		Service(const std::wstring& name);

		~Service();
	};

	void ServiceCtrlHandler(DWORD ctrl_code);
	void InitServiceCtrlHandler(const wchar_t* service_name);
}

#endif // SERVICE_SERVICE_H_

#endif // _WIN32