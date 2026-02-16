#include <windows.h>
#include <iostream>
#include <string>

void RunCmd(const std::wstring& cmd)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi{};

    std::wstring fullCmd = L"cmd.exe /c " + cmd;

    if (CreateProcessW(
        nullptr,
        (wchar_t*)fullCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int wmain()
{
    RunCmd(L"sc stop REFDRecovery");
    RunCmd(L"taskkill /f /im REFDRecovery.exe");
    RunCmd(L"sc delete REFDRecovery");

    RunCmd(L"sc stop REFD");
    RunCmd(L"taskkill /f /im REFD.exe");
    RunCmd(L"sc delete REFD");

    return 0;
}
