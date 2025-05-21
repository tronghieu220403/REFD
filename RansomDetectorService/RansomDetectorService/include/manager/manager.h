#pragma once
#ifndef MANAGER_H_
#define MANAGER_H_

#include "ulti/support.h"
#include "file_manager.h"
#include "process_manager.h"

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
		// Store grouped events by requestor_pid
		std::unordered_map<ULONG, std::vector<FileIoInfo>> global_merged_events_by_pid;
		// Map to store hash of new_path_list and its corresponding event index per PID
		std::unordered_map<ULONG, std::unordered_map<ull, int>> global_path_hash_to_merged_index_by_pid;
		// ProcessInfo structure to store process information
		std::unordered_map<ULONG, ProcessInfo> global_process_map; // PID -> ProcessInfo

		std::chrono::steady_clock::time_point last_evaluation_time = std::chrono::steady_clock::now();

		std::chrono::steady_clock::time_point last_clear_tmp_time = std::chrono::steady_clock::now();
	public:

		void LockMutex();
		void UnlockMutex();

		void ProcessDataQueue();

		bool AnalyzeEvent(FileIoInfo& event);

		bool EvaluateProcess(ULONG pid);

		void EvaluateProcesses();

		bool DiscardEventByPid(ULONG issuing_pid);
	};

	inline Evaluator* kEvaluator = nullptr;
}
#endif  // MANAGER_H_
