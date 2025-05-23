#include "process_manager.h"

namespace manager
{
	bool KillProcessByPID(DWORD pid)
	{
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
		if (hProcess == NULL)
		{
			std::cerr << "Failed to open process. Error: " << GetLastError() << "\n";
			return false;
		}

		BOOL result = TerminateProcess(hProcess, 1);  // Exit code 1
		if (!result)
		{
			std::cerr << "Failed to terminate process. Error: " << GetLastError() << "\n";
			CloseHandle(hProcess);
			return false;
		}

		std::cout << "Successfully terminated process with PID: " << pid << "\n";
		CloseHandle(hProcess);
		return true;
	}
}
