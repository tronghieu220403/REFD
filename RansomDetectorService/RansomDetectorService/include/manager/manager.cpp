#include "manager.h"
#include "file_type_iden.h"

namespace manager {

	void Init()
	{
		PrintDebugW(L"Manager initialized");
		kCurrentPid = GetCurrentProcessId();
		InitDosDeviceCache();
		kFileIoManager = new FileIoManager();
		kEvaluator = new Evaluator();
	}

	void Cleanup()
	{
		PrintDebugW(L"Manager cleaned up");
		delete kEvaluator;
		delete kFileIoManager;
	}

	void Evaluator::LockMutex()
	{
		event_mutex_.lock();
	}

	void Evaluator::UnlockMutex()
	{
		event_mutex_.unlock();
	}

	void Evaluator::ProcessDataQueue()
	{
		auto current_time = std::chrono::steady_clock::now();

		PrintDebugW(L"Start processing data queue");
		std::queue<FileIoInfo> file_io_list;
		kFileIoManager->LockMutex();
		kFileIoManager->MoveQueue(file_io_list);
		kFileIoManager->UnlockMutex();
		PrintDebugW(L"Number of file I/O events: %d", file_io_list.size());
		// If the event list is empty, return an empty queue

		if (file_io_list.empty()) {
			return;
		}
		// Store events grouped by requestor_pid
		std::unordered_map<ULONG, std::vector<FileIoInfo>> events_by_pid;
		
        static std::unordered_map<ULONG, std::wstring> pid_to_name_map;

		// Move events from the queue into the map grouped by requestor_pid
		while (!file_io_list.empty())
		{
			FileIoInfo& event = file_io_list.front();
			auto pid = event.requestor_pid; // std::move will make event.requestor_pid invalid if we use: events_by_pid[event.requestor_pid].push_back(std::move(event));
			
			// Discard not honey pots file

			events_by_pid[pid].push_back(std::move(event));
			file_io_list.pop();
		}

		PrintDebugW(L"Merging events");
		// Iterate through each group of events with the same requestor_pid
		for (auto& pid_events : events_by_pid)
		{
			
		}
	}

	void Evaluator::EvaluateProcesses()
	{
		PrintDebugW(L"Start evaluating processes");

		for (auto& pid_events : global_merged_events_by_pid) {
			auto pid = pid_events.first;
			EvaluateProcess(pid);
		}
		PrintDebugW(L"End evaluating processes");
	}

	bool Evaluator::AnalyzeEvent(FileIoInfo& event)
	{
		PrintDebugW(L"AnalyzeEvent: %ws, pid %d", event.path.c_str(), event.requestor_pid);

		if (event.types.size() == 0)
		{
			if (FileExist(event.path) == true)
			{
				event.types = std::move(kTrID->GetTypes(event.path));
			}
			else
			{
				return false;
			}
		}

		const std::vector<std::string>& ext_accepted_types = ...;

		event.type_match = (type_iden::HasCommonType(ext_accepted_types, event.types)) ? TYPE_HAS_COMMON : TYPE_MISMATCH;

#ifdef _DEBUG
		switch (event.type_match)
		{
		case TYPE_MISMATCH:
			PrintDebugW(L"TYPE_MISMATCH: types %ws, ext_accepted_types %ws", type_iden::CovertTypesToString(event.types).c_str(), type_iden::CovertTypesToString(ext_accepted_types));
			break;

		case TYPE_HAS_COMMON:
			PrintDebugW(L"TYPE_HAS_COMMON: types %ws, ext_accepted_types %ws", type_iden::CovertTypesToString(event.types).c_str(), type_iden::CovertTypesToString(ext_accepted_types));
			break;

		default:
			break;
		}
#endif // _DEBUG

		return true;
	}


	bool Evaluator::EvaluateProcess(ULONG pid)
	{
		return is_ransomware;
	}

	bool Evaluator::DiscardEventByPid(ULONG issuing_pid)
	{
		if (issuing_pid == 0 || issuing_pid == (DWORD)(-1) || issuing_pid == 4
			|| issuing_pid == kCurrentPid)
		{
			return true;
		}

		return false;
	}

}