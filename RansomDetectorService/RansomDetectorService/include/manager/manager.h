#pragma once
#ifndef MANAGER_H_
#define MANAGER_H_

#include "ulti/support.h"
#include "file_manager.h"
#include "process_manager.h"

namespace manager {
	inline FileIoManager* kFileIoManager = nullptr;
	inline size_t kCurrentPid = 0;

	void Init();
	void Cleanup();

	void EvaluateProcesses();
	
	void EvaluateProcess(ULONG pid);

	void ProcessDataQueue();

	bool DiscardEventByPid(size_t issuing_pid);
}
#endif  // MANAGER_H_
