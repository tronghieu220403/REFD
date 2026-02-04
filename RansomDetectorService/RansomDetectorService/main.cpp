#ifndef RDS_RDS_MAIN
#define RDS_RDS_MAIN

/*
C/C++ -> General -> Additional Include Dir -> $(ProjectDir)include
*/
#include "ulti/support.h"
#include "service/service.h"
#include "manager/scanner.h"
#include "manager/receiver.h"
#include "manager/file_type_iden.h"
#include "ulti/file_helper.h"

constexpr auto SERVICE_NAME = L"REFD";

static void ServiceMain()
{
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	PrintDebugW(L"ServiceMain, current pid %d", GetCurrentProcessId());
	
	srv::InitServiceCtrlHandler(SERVICE_NAME);

	debug::InitDebugLog();

	helper::InitDosDeviceCache();

	auto ft = type_iden::FileType::GetInstance();
	if (ft == nullptr) {
		return;
	}

	auto rcv = manager::Receiver::GetInstance();
	if (rcv == nullptr) {
		return;
	}

	auto scanner = manager::Scanner::GetInstance();
	if (scanner == nullptr) {
		return;
	}

	if (ft->Init() == false) {
		return;
	}

	if (rcv->Init() == false) {
		return;
	}

	if (scanner->Init() == false) {
		return;
	}
}

static void RunService()
{
	if (ulti::IsRunningAsSystem() == false)
	{
		ShowWindow(GetConsoleWindow(), SW_HIDE);
		srv::Service service(SERVICE_NAME);
		service.Stop();
		service.Delete();
		std::wstring w_srv_path;
		w_srv_path.resize(1024);
		GetModuleFileNameW(nullptr, &w_srv_path[0], 1024);
		w_srv_path.resize(wcslen(&w_srv_path[0]));
		auto status = service.Create(w_srv_path, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START);
		if (status == ERROR_SUCCESS || status == ERROR_DUPLICATE_SERVICE_NAME || status == ERROR_SERVICE_EXISTS)
		{
			auto status = service.Run();
			if (status != ERROR_SUCCESS)
			{
				PrintDebugW(L"Service run failed %d", status);
			}
		}
	}
	else
	{
		SERVICE_TABLE_ENTRYW service_table[] = {
			{ (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
			{ nullptr, nullptr }
		};

		if (!StartServiceCtrlDispatcher(service_table)) {
			PrintDebugW(L"StartServiceCtrlDispatcher failed");
        }
		else {
			PrintDebugW(L"StartServiceCtrlDispatcher succeeded");
		}
	}

}

static void RunProgram()
{
#ifdef _DEBUG
	RunService();
	//ServiceMain();
#else
	RunService();
	//ServiceMain();
#endif // _DEBUG
}

int main()
{
	RunProgram();

	return 0;
}

#endif

