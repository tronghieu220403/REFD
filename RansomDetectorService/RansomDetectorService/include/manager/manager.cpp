#include "manager.h"
#include "file_type_iden.h"
#include "known_folder.h"
#include "matcher.h"

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
			FileIoInfo event(std::move(file_io_list.front()));
			file_io_list.pop();

			auto pid = event.requestor_pid;
			if (DiscardEventByPid(pid) == true)
			{
				continue;
			}
			if (kf_checker.IsPathInKnownFolders(event.path) == true && GetFileName(event.path).find(L"hieunt-") != std::wstring::npos)
			{
				PrintDebugW(L"In known folder: %ws", event.path.c_str());
				events_by_pid[pid].push_back(std::move(event));
			}
		}

		// Xử lí chỉ cần mang tính local, cụ thể là tìm xem pid này thay đổi những folder nào rồi đem ra phân tích các folder (bỏ qua nếu trong cache chưa quá hạn hoặc trực tiếp đánh giá rồi cập nhật cache) chứ không lưu lại các lần xử lí sau.
		// Iterate through each group of events with the same requestor_pid
		for (auto& pid_events : events_by_pid)
		{
			
		}
	}

	bool Evaluator::IsDirAttackedByRansomware(const std::wstring& dir_path)
	{
		std::vector<std::wstring> files;
		if (manager::DirExist(dir_path) == false) {
			return false;
		}
		try {
			for (const auto& entry : fs::directory_iterator(dir_path)) {
				try {
					const auto file_path = entry.path().wstring();
					const auto sz = manager::GetFileSize(file_path);
					if (sz != 0 && sz <= FILE_MAX_SIZE_SCAN) {
						files.push_back(file_path);
					}
				}
				catch (...) {}
			}
		}
		catch (...) { }

		std::vector<std::vector<std::string>> vvs;
		for (auto& file_path : files)
		{
			if (ulti::StrToWstr(ulti::WstrToStr(file_path)) != file_path)
			{
				continue;
			}
			vvs.push_back(kTrID->GetTypes(file_path));
		}

		// Cần config để lấy vs thay vì ví dụ thế này
		std::vector<std::string> vs(60, "txt");

		Matcher m;
		m.SetInput(vvs, vs);
		auto ans = m.Solve();
		
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