#ifndef ETWSERVICE_ETWSERVICE_MAIN
#define ETWSERVICE_ETWSERVICE_MAIN

/*
C/C++ -> General -> Additional Include Dir -> $(ProjectDir)include
*/
#include "ulti/support.h"
#include "service/service.h"
#include "manager/manager.h"
#include "com/minifilter_comm.h"
#include "manager/file_type_iden.h"

constexpr auto SERVICE_NAME = L"REFD";

#define PORT_NAME L"\\hieunt_mf"

struct COMPORT_MESSAGE
{
	FILTER_MESSAGE_HEADER header;
	manager::RawFileIoInfo raw_file_io_info;
};

static void StartEventCollector()
{
	// Create a communication port
	FltComPort com_port;

	size_t buffer_size = sizeof(COMPORT_MESSAGE);
	void* buffer = new char[buffer_size];

	while (true)
	{
		if (manager::FileExist(L"C:\\Users\\hieu\\Documents\\ggez.txt"))
		{
			PrintDebugW(L"ggez.txt detected, terminate service");
			ExitProcess(0);
		}

		//PrintDebugW(L"Creating communication port %ws", PORT_NAME);
		HRESULT hr = com_port.Create(PORT_NAME);
		if (FAILED(hr))
		{
			//PrintDebugW(L"Failed to create communication port: %08X", hr);
			Sleep(1000); // Retry after 1 second
		}

		while (true)
		{
			//PrintDebugW(L"Wait for a message from the communication port");
			hr = com_port.Get((PFILTER_MESSAGE_HEADER)buffer, buffer_size);
			if (SUCCEEDED(hr))
			{
				//PrintDebugW(L"Received message from communication port");
				COMPORT_MESSAGE* msg = (COMPORT_MESSAGE*)buffer;
				// Process the message
				manager::RawFileIoInfo* raw_file_io_info = &msg->raw_file_io_info;
				// Push it to a queue
				manager::kFileIoManager->LockMutex();
				manager::kFileIoManager->PushFileEventToQueue(raw_file_io_info);
				manager::kFileIoManager->UnlockMutex();
			}
			else
			{
				PrintDebugW(L"Failed to receive message: %08X", hr);
				break; // Exit the loop on failure
			}
		}
	}
}

static void ServiceMain()
{
	PrintDebugW(L"ServiceMain, current pid %d", GetCurrentProcessId());
#ifndef _DEBUG
	srv::InitServiceCtrlHandler(SERVICE_NAME);
#endif // _DEBUG

	/*
	if (ulti::CreateDir(MAIN_DIR) == false)
	{
		PrintDebugW(L"Create main dir %ws failed", MAIN_DIR);
		return;
	}
	*/

	debug::InitDebugLog();

	manager::Init();

	if (ulti::CreateDir(TEMP_DIR) == false)
	{
		PrintDebugW(L"Create temp dir %ws failed", TEMP_DIR);
		return;
	}
	kTrID = new type_iden::TrID();
	if (kTrID == nullptr || kTrID->Init(PRODUCT_PATH, (std::wstring(PRODUCT_PATH) + L"TrIDLib.dll").c_str()) == false)
	{
		PrintDebugW(L"TrID init failed");
		return;
	}

	std::jthread collector_thread(([]() {
		while (true)
		{
			StartEventCollector();
			if (manager::FileExist(L"C:\\Users\\hieu\\Documents\\ggez.txt"))
			{
				PrintDebugW(L"ggez.txt detected, terminate service");
				ExitProcess(0);
			}
			Sleep(50);
		}
		}));

	std::thread processing_thread([]() {
        manager::ClearTmpFiles();
		while (true)
		{
			manager::kEvaluator->LockMutex();
			manager::kEvaluator->ProcessDataQueue();
			manager::kEvaluator->UnlockMutex();
			if (manager::FileExist(L"C:\\Users\\hieu\\Documents\\ggez.txt"))
			{
				PrintDebugW(L"ggez.txt detected, terminate service");
				ExitProcess(0);
			}
			Sleep(1000);
		}
		});

	std::jthread manager_thread([]() {
		while (true)
		{
			auto start_time = std::chrono::high_resolution_clock::now();
			manager::kEvaluator->LockMutex();
			manager::kEvaluator->EvaluateProcesses();
			manager::kEvaluator->UnlockMutex();
			if (manager::FileExist(L"C:\\Users\\hieu\\Documents\\ggez.txt"))
			{
				PrintDebugW(L"ggez.txt detected, terminate service");
				ExitProcess(0);
			}
			auto end_time = std::chrono::high_resolution_clock::now();
			DWORD duration = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
			DWORD sleep_ms = duration < (DWORD)EVALUATATION_INTERVAL_MS ? EVALUATATION_INTERVAL_MS - duration : 0;
			PrintDebugW(L"Evaluation took %d ms, sleeping for %d ms", duration, sleep_ms);
			Sleep(sleep_ms);
		}
		});
	processing_thread.join();
	collector_thread.join();
	manager_thread.join();
	debug::CleanupDebugLog();
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
#ifndef _DEBUG
	RunService();
	//ServiceMain();
#else
	//RunService();
	ServiceMain();
#endif // _DEBUG
}

int main()
{
	RunProgram();

	return 0;
}

#endif

