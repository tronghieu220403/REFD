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
#include "manager/etw_controller.h"

static void ServiceMain()
{
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	PrintDebugW(L"ServiceMain, current pid %d", GetCurrentProcessId());
	
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

	auto etwc = EtwController::GetInstance();
	if (etwc == nullptr) {
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

	etwc->Start();
}

static void StopService() {

	auto etwc = EtwController::GetInstance();
	if (etwc == nullptr) {
		return;
	}
	etwc->Stop();

	auto ft = type_iden::FileType::GetInstance();
	if (ft != nullptr) {
		ft->Uninit();
		return;
	}

	auto rcv = manager::Receiver::GetInstance();
	if (rcv != nullptr) {
		rcv->Uninit();
		return;
	}

	auto scanner = manager::Scanner::GetInstance();
	if (scanner != nullptr) {
		scanner->Uninit();
		return;
	}
}

int main()
{
#ifdef _DEBUG
	ServiceMain();
	Sleep(INFINITE);
#else
	// Service mode
	if (!ulti::IsRunningAsSystem())
	{
		srv::Service::RegisterService();
	}
	else
	{
		srv::Service::RegisterUnloadFunc(StopService);
		srv::Service::StartServiceMain(ServiceMain);
	}
#endif
	return 0;
}

#endif

