#include <Windows.h>
#include "service/service.h"

static bool IsRefdRunning()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM)
    {
        PrintDebugW(L"OpenSCManagerW failed: %d", GetLastError());
        return false;
    }
    defer{ CloseServiceHandle(hSCM); };

    SC_HANDLE hService = OpenServiceW(hSCM, L"REFD", SERVICE_QUERY_STATUS);

    if (!hService)
    {
        PrintDebugW(L"OpenServiceW failed: %d", GetLastError());
        return false;
    }
    defer{ CloseServiceHandle(hService); };

    SERVICE_STATUS_PROCESS ssp{};
    DWORD bytesNeeded = 0;

    if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytesNeeded))
    {
        PrintDebugW(L"QueryServiceStatusEx failed: %d", GetLastError());
        return false;
    }

    if (ssp.dwCurrentState != SERVICE_RUNNING)
    {
        PrintDebugW(L"REFD is not running.");
        return false;
    }

    return true;
}

static std::atomic<int> status{ 0 };

static void ServiceMain()
{
	while (true) {
        if (status.load(std::memory_order_acquire) != 0)
            break;

        if (IsRefdRunning() == false) {
            system("sc start REFD");
        }
        Sleep(200);
	}
    status.store(2, std::memory_order_release);
}

static void StopService()
{
    status.store(1, std::memory_order_release);

    while (status.load(std::memory_order_acquire) == 1)
        Sleep(200);
}

int main()
{
	if (!ulti::IsRunningAsSystem())
	{
		srv::Service::RegisterService();
	}
	else
	{
		srv::Service::RegisterUnloadFunc(StopService);
		srv::Service::StartServiceMain(ServiceMain);
	}
}
