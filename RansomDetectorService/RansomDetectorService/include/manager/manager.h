#pragma once
#ifndef MANAGER_MANAGER_H_
#define MANAGER_MANAGER_H_

#include "../ulti/support.h"
#include "file_manager.h"
#include "../honey/honeypot.h"

using namespace honeypot;

#define THRESHOLD_PERCENTAGE 80
#define BelowTypeThreshold(part, total) (part <= total * THRESHOLD_PERCENTAGE / 100)

namespace manager {
	inline FileIoManager* kFileIoManager = nullptr;
	inline FileCache* kFileCache = nullptr;
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
		
		bool IsDirAttackedByRansomware(const std::wstring& dir_path, const HoneyType& dir_type);

		bool DiscardEventByPid(ULONG issuing_pid);
	};

	inline Evaluator* kEvaluator = nullptr;
}
#endif  // MANAGER_MANAGER_H_
