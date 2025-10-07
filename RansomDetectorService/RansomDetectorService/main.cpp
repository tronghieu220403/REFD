#ifndef RDS_RDS_MAIN
#define RDS_RDS_MAIN

/*
C/C++ -> General -> Additional Include Dir -> $(ProjectDir)include
*/
#include "ulti/support.h"
#include "service/service.h"
#include "manager/manager.h"
#include "com/minifilter_comm.h"
#include "manager/file_type_iden.h"
#include "manager/known_folder.h"

#include "honey/honeypot.h"

constexpr auto SERVICE_NAME = L"REFD";

#define PORT_NAME L"\\hieunt_mf"

struct COMPORT_MESSAGE
{
	FILTER_MESSAGE_HEADER header;
	manager::RawFileIoInfo raw_file_io_info;
};

std::chrono::steady_clock::time_point last_process_time;

static void StartEventCollector()
{
	// Create a communication port
	FltComPort com_port;

	size_t buffer_size = sizeof(COMPORT_MESSAGE);
	void* buffer = new char[buffer_size];

	while (true)
	{
		//PrintDebugW(L"Creating communication port %ws", PORT_NAME);
		HRESULT hr = com_port.Create(PORT_NAME);
		if (FAILED(hr))
		{
			PrintDebugW(L"Failed to create communication port: 0x%08X", hr);
			Sleep(1000); // Retry after 1 second
		}
		//PrintDebugW(L"Communication port created");

		while (true)
		{
			/*
			//PrintDebugW(L"Check every 6 seconds");
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_process_time);
			if (elapsed.count() >= 6) // Check every 6 seconds
			{
				manager::kEvaluator->LockMutex();
				manager::kEvaluator->Evaluate();
				manager::kEvaluator->UnlockMutex();
                last_process_time = current_time;
			}
			*/
			//PrintDebugW(L"Wait for a message from the communication port");
			hr = com_port.Get((PFILTER_MESSAGE_HEADER)buffer, buffer_size);
			if (SUCCEEDED(hr))
			{
				//PrintDebugW(L"Message received, size %d", ((PFILTER_MESSAGE_HEADER)buffer)->ReplyLength);
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
				//PrintDebugW(L"Failed to receive message: 0x%08X", hr);
				break; // Exit the loop on failure
			}
		}
	}
}

static void ServiceMain()
{
	PrintDebugW(L"ServiceMain, current pid %d", GetCurrentProcessId());
	
	srv::InitServiceCtrlHandler(SERVICE_NAME);

	/*
	if (ulti::CreateDir(MAIN_DIR) == false)
	{
		PrintDebugW(L"Create main dir %ws failed", MAIN_DIR);
		return;
	}
	*/

	debug::InitDebugLog();

#ifdef _M_IX86
	if (ulti::CreateDir(TEMP_DIR) == false)
	{
		PrintDebugW(L"Create temp dir %ws failed", TEMP_DIR);
		return;
	}
#endif // _M_IX86

	kFileType = new type_iden::FileType();
	if (kFileType == nullptr)
	{
		PrintDebugW(L"kFileType init failed");
		return;
	}
#ifdef _M_IX86
	std::wstring trid_dir = std::wstring(PRODUCT_PATH) + L"TrID";
	if (kFileType->InitTrid(trid_dir, trid_dir + L"TrIDLib.dll") == false)
	{
		PrintDebugW(L"TrID init failed");
		return;
	}
#endif // _M_IX86

	try {
		// Cần config động, thư mục nào là honeypot folder (chỉ toàn honeypot file), thư mục nào có honeypot file lẩn giữa các file khác
		// Sửa cấu trúc của config.
		kfc.Init(L"E:\\hieunt210330\\hieunt210330\\knownfolders.json");
	}
	catch (std::exception& ex) {
		PrintDebugW("KnownFolderChecker error: %ws", ulti::StrToWstr(ex.what()).c_str());
		return;
	}

	// Cần config động, thư mục này có gì có những file nào
	// Cần file config v2 để lưu
	if (hp.Init(kfc.GetKnownFolders(), L"E:\\hieunt210330\\honeypot") == false)
	{
		return;
	}

	manager::Init();

	auto last_process_time = std::chrono::steady_clock::now();

	std::jthread collector_thread(([]() {
		while (true)
		{
			StartEventCollector();
			Sleep(50);
		}
		}));

	std::thread processing_thread([&last_process_time]() {
		while (true)
		{
			manager::kEvaluator->LockMutex();
			manager::kEvaluator->Evaluate();
            last_process_time = std::chrono::steady_clock::now();
			manager::kEvaluator->UnlockMutex();
			Sleep(1000);
		}
		});

	processing_thread.join();
	collector_thread.join();
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
#ifdef _DEBUG
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

