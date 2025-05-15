#pragma once
#ifndef MANAGER_H_
#define MANAGER_H_

#include "../ulti/support.h"
#include "file_manager.h"
#include "process_manager.h"

namespace manager {
	inline ProcessManager* kProcMan = nullptr;
	inline FileIoManager* kFileIoManager = nullptr;
	inline uint64_t kCurrentPid = 0;

	void Init();
	void Cleanup();

	void EvaluateProcess();

	bool OverallEventFilter(uint64_t issuing_pid);
}
#endif  // MANAGER_H_
