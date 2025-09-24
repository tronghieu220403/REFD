#pragma once
#ifndef MANAGER_H_
#define MANAGER_H_

#include "../ulti/support.h"
#include "file_manager.h"

namespace manager {
	inline FileIoManager* kFileIoManager = nullptr;
	inline ULONG kCurrentPid = 0;

	void Init();
	void Cleanup();

	class Evaluator
	{
	private:
		// Event mutex
		std::mutex event_mutex_;

		std::chrono::steady_clock::time_point last_evaluation_time = std::chrono::steady_clock::now();

		std::chrono::steady_clock::time_point last_clear_tmp_time = std::chrono::steady_clock::now();
	public:

		void LockMutex();
		void UnlockMutex();

		void Evaluate();
		
		bool IsDirAttackedByRansomware(const std::wstring& dir_path);

		bool DiscardEventByPid(ULONG issuing_pid);
	};

	inline Evaluator* kEvaluator = nullptr;
}
#endif  // MANAGER_H_
