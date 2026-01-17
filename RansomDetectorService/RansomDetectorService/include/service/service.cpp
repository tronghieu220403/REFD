#ifdef _WIN32

#include "service.h"

namespace srv
{
	DWORD Service::Create(const std::wstring& service_path, const DWORD& type, const DWORD& start_type)
	{
		if (h_services_control_manager_ == NULL) {
			PrintDebugW(L"Failed to open Service Control Manager");
			return ERROR_INVALID_HANDLE;
		}
		type_ = type;
		start_type_ = start_type;
		service_path_ = service_path;

		SC_HANDLE handle_service = OpenServiceW(h_services_control_manager_, service_name_.c_str(), SERVICE_QUERY_STATUS);
		if (handle_service != NULL)
		{
			return ERROR_SERVICE_EXISTS;
		}

		handle_service = CreateServiceW(h_services_control_manager_, service_name_.c_str(), service_name_.c_str(),
			SERVICE_ALL_ACCESS, type_,
			start_type_, SERVICE_ERROR_NORMAL,
			service_path_.c_str(), NULL, NULL, NULL, NULL, NULL);

		if (handle_service == NULL)
		{
			ULONG status = GetLastError();
			if (status == ERROR_DUPLICATE_SERVICE_NAME || status == ERROR_SERVICE_EXISTS)
			{
				return status;
			}
			return status;
		}
		CloseServiceHandle(handle_service);

		return ERROR_SUCCESS;
	}

	DWORD Service::Run()
	{
		if (h_services_control_manager_ == NULL) {
			PrintDebugW(L"Failed to open Service Control Manager");
			return ERROR_INVALID_HANDLE;
		}

		SC_HANDLE handle_service;
		std::wstring working_dir;
		working_dir.resize(MAX_PATH + 1);

		GetCurrentDirectoryW(MAX_PATH + 1, &working_dir[0]);
		LPCWSTR argv_start[] = { &service_name_[0], &working_dir[0] };

		handle_service = OpenServiceW(h_services_control_manager_, service_name_.data(), SERVICE_ALL_ACCESS);

		if (handle_service == NULL)
		{
			return GetLastError();
		}

		if (!StartServiceW(handle_service, 2, argv_start))
		{
			return GetLastError();
		}
		CloseServiceHandle(handle_service);
		return ERROR_SUCCESS;
	}

	DWORD Service::Stop()
	{
		if (h_services_control_manager_ == NULL) {
			PrintDebugW(L"Failed to open Service Control Manager");
			return ERROR_INVALID_HANDLE;
		}

		if (ulti::IsRunningAsSystem() == false)
		{
			std::string stop_srv_cmd = "sc stop " + ulti::WstrToStr(service_name_);
			std::system(stop_srv_cmd.c_str());
		}

		// Open the service to stop and delete.
		SC_HANDLE service_handle = OpenService(h_services_control_manager_, service_name_.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
		if (service_handle == NULL) {
			PrintDebugW(L"Failed to open service %ws: %d", service_name_.c_str(), GetLastError());
			return GetLastError();
		}

		// Check the status of the service.
		SERVICE_STATUS status;
		if (!QueryServiceStatus(service_handle, &status)) {
			PrintDebugW(L"Failed to query service status %ws: %d", service_name_.c_str(), GetLastError());
			CloseServiceHandle(service_handle);
			return GetLastError();
		}

		// If the service is running, stop the service.
		if (status.dwCurrentState == SERVICE_RUNNING) {
			if (!ControlService(service_handle, SERVICE_CONTROL_STOP, &status)) {
				PrintDebugW(L"Failed to stop the service %ws: %d", service_name_.c_str(), GetLastError());
				CloseServiceHandle(service_handle);
				return GetLastError();
			}
		}

		CloseServiceHandle(service_handle);
		return ERROR_SUCCESS;
	}

	DWORD Service::Delete()
	{
		if (h_services_control_manager_ == NULL) {
			PrintDebugW(L"Failed to open Service Control Manager");
			return ERROR_INVALID_HANDLE;
		}

		DWORD err = ERROR_SUCCESS;
		SC_HANDLE service = OpenService(h_services_control_manager_, service_name_.c_str(), DELETE);
		if (service)
		{
			// Delete the service
			if (DeleteService(service) == FALSE)
			{
				err = GetLastError();
				PrintDebugW(L"DeleteService %ws  failed with error %d, %ws", service_name_.c_str(), err, debug::GetErrorMessage(err).c_str());
			}
			else
			{
				// Recursively delete all subkeys
				LONG result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, (std::wstring(L"SYSTEM\\ControlSet001\\Services\\") + service_name_).c_str());
			}

			CloseServiceHandle(service);
		}
		else
		{
			err = GetLastError();
			PrintDebugW(L"OpenService %ws failed with error %d, %ws", service_name_.c_str(), err, debug::GetErrorMessage(err).c_str());
		}
		if (ulti::IsRunningAsSystem() == false)
		{
			std::string delete_srv_cmd = "sc delete " + ulti::WstrToStr(service_name_);
			std::system(delete_srv_cmd.c_str());
		}
		return err;
	}

	Service::Service(const std::wstring& name)
		: service_name_(name),
		h_services_control_manager_(OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS))
	{
		if (h_services_control_manager_ == NULL) {
			auto err = GetLastError();
			PrintDebugW(L"Failed to open Service Control Manager: %d, %ws", err, debug::GetErrorMessage(err).c_str());
		}
	}

	Service::~Service()
	{
		if (h_services_control_manager_ != NULL)
		{
			CloseServiceHandle(h_services_control_manager_);
		}
		h_services_control_manager_ = 0;
	}

	inline SERVICE_STATUS_HANDLE status_handle = NULL;

	SERVICE_STATUS service_status = { 0 };
	void ServiceCtrlHandler(DWORD ctrl_code)
	{
		PrintDebugW(L"Control code %d", ctrl_code);
		PrintDebugW(L"dwControlsAccepted: %x", service_status.dwControlsAccepted);
		service_status.dwWin32ExitCode = NO_ERROR;
		if (ctrl_code == SERVICE_CONTROL_HIEUNT_ACCEPT_STOP)
		{
			service_status.dwControlsAccepted |= SERVICE_ACCEPT_STOP;
			SetServiceStatus(status_handle, &service_status);
		}
		else if (ctrl_code == SERVICE_CONTROL_HIEUNT_BLOCK_STOP)
		{
			service_status.dwControlsAccepted &= ~SERVICE_ACCEPT_STOP;
			SetServiceStatus(status_handle, &service_status);
		}
		else if (ctrl_code == SERVICE_CONTROL_STOP)
		{
			service_status.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(status_handle, &service_status);
			service_status.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(status_handle, &service_status);
		}
		else if (ctrl_code == SERVICE_CONTROL_SHUTDOWN)
		{
			service_status.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(status_handle, &service_status);
			service_status.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(status_handle, &service_status);
		}
		else if (ctrl_code == SERVICE_CONTROL_START)
		{
			PrintDebugW(L"Service start");
			service_status.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(status_handle, &service_status);
		}
		else if (ctrl_code == SERVICE_CONTROL_INTERROGATE)
		{
			SetServiceStatus(status_handle, &service_status);
		}
		else
		{
			PrintDebugW(L"Service control code %d is not handled", ctrl_code);
			service_status.dwWin32ExitCode = ERROR_CALL_NOT_IMPLEMENTED;
			SetServiceStatus(status_handle, &service_status);
		}
	}

	void InitServiceCtrlHandler(const wchar_t* service_name)
	{
		PrintDebugW(L"Begin");

		service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		service_status.dwWin32ExitCode = NO_ERROR;
		service_status.dwWaitHint = 0;
		service_status.dwControlsAccepted = SERVICE_USER_DEFINED_CONTROL | SERVICE_ACCEPT_SHUTDOWN;

		status_handle = RegisterServiceCtrlHandlerW(service_name, (LPHANDLER_FUNCTION)ServiceCtrlHandler);
		if (status_handle == NULL)
		{
			PrintDebugW(L"RegisterServiceCtrlHandler failed %d", GetLastError());
		}
		ServiceCtrlHandler(SERVICE_CONTROL_START);

	}
}

#endif