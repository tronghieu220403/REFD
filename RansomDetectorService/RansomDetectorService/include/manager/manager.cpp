#include "manager.h"
#include "file_type_iden.h"
#include "known_folder.h"

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

	void Evaluator::Evaluate()
	{
		auto current_time = std::chrono::steady_clock::now();

		std::queue<FileIoInfo> file_io_list;
		kFileIoManager->LockMutex();
		kFileIoManager->MoveQueue(file_io_list);
		kFileIoManager->UnlockMutex();
		
		if (file_io_list.empty()) {
			return;
		}

		// Store events grouped by requestor_pid
		std::unordered_map<ULONG, std::vector<FileIoInfo>> events_by_pid;
		
		// Move events from the queue into the map grouped by requestor_pid
		while (file_io_list.empty() == false)
		{
			FileIoInfo& event = file_io_list.front();
			auto pid = event.requestor_pid; // std::move will make event.requestor_pid invalid if we use: events_by_pid[event.requestor_pid].push_back(std::move(event));
			
			if (kf_checker.IsPathInKnownFolders(event.path) == true && GetFileName(event.path).find(L"hieunt-") != std::wstring::npos);
			{
				events_by_pid[pid].push_back(std::move(event));
				PrintDebugW(L"In known folder: %ws", event.path.c_str());
			}
			file_io_list.pop();
		}

		// Xử lí chỉ cần mang tính local, cụ thể là tìm xem pid này thay đổi những folder nào rồi đem ra phân tích các folder (bỏ qua nếu trong cache chưa quá hạn hoặc trực tiếp đánh giá rồi cập nhật cache) chứ không lưu lại các lần xử lí sau.
		// Iterate through each group of events with the same requestor_pid
		for (auto& pid_events : events_by_pid)
		{
			
		}
	}

	void Evaluator::EvaluateProcesses()
	{
		PrintDebugW(L"Start evaluating processes");

		PrintDebugW(L"End evaluating processes");
	}

	bool Evaluator::AnalyzeEvent(FileIoInfo& event)
	{
		/*
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
		*/
		return true;
	}


	bool Evaluator::EvaluateProcess(ULONG pid)
	{
		return false;
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