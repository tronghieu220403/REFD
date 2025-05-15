#ifndef ETWSERVICE_ETWSERVICE_MAIN
#define ETWSERVICE_ETWSERVICE_MAIN

/*
C/C++ -> General -> Additional Include Dir -> $(ProjectDir)include
*/
#include "ulti/support.h"
#include "service/service.h"
#include "manager/manager.h"

constexpr auto SERVICE_NAME = L"REFD";

static void StartEventCollector()
{
    
}

static void ServiceMain()
{
    PrintDebugW(L"ServiceMain");
#ifndef _DEBUG
    srv::InitServiceCtrlHandler(SERVICE_NAME);
#endif // _DEBUG

    if (ulti::CreateDir(MAIN_DIR) == false)
    {
        PrintDebugW(L"Create main dir %ws failed", MAIN_DIR);
        return;
    }

    manager::Init();
    std::jthread collector_thread(([]() {
        while (true)
        {
            StartEventCollector();
            Sleep(50);
        }
        }));
    std::jthread manager_thread([]() {
        if (ulti::CreateDir(TEMP_DIR) == false)
        {
            return;
        }
        while (true)
        {
            auto start_time = std::chrono::high_resolution_clock::now();
            //manager::EvaluateProcess();
            if (manager::FileExist(L"C:\\Users\\hieu\\Documents\\ggez.txt"))
            {
                ExitProcess(0);
            }
            auto end_time = std::chrono::high_resolution_clock::now();
            DWORD duration = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            //Sleep(duration < (DWORD)EVALUATATION_INTERVAL_MS ? EVALUATATION_INTERVAL_MS - duration : 0);
            Sleep(50);
        }
        });
    manager_thread.join();
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
    }

}

static void RunProgram()
{
#ifdef _DEBUG
    ServiceMain();
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

