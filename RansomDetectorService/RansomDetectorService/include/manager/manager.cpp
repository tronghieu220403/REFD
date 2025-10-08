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
		kFileCache = new FileCache();
		kFileIoManager = new FileIoManager();
		kEvaluator = new Evaluator();
	}

	void Cleanup()
	{
		PrintDebugW(L"Manager cleaned up");
		delete kEvaluator;
		delete kFileIoManager;
		delete kFileCache;
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
		struct QueueInfo {
			honeypot::HoneyType type = honeypot::HoneyType::kNotHoney;
			int change_count = 0; // change_count > 0 -> push in to queue
			ll last_scan = 0;
			ll first_add = 0;
		};

		auto get_now = []() -> int64_t {
			return chrono::steady_clock::now().time_since_epoch().count();
			};

		unordered_map<wstring, QueueInfo> mp;

		for (const auto& x : hp.GetHoneyFolders())
		{
			QueueInfo info = {};
			info.type = x.second;
			;			mp.insert({ x.first , info });
		}

		auto now = get_now();
		while (true)
		{
			defer{ ulti::ThreadPerfCtrlSleep(); };
			if (now - get_now() >= 3600LL * 2LL) {
				for (auto& x : mp) {
					auto& info = x.second;
					info.change_count = 0;
					info.last_scan = 0;
					info.first_add = 0;
				}
			}

			queue<FileIoInfo> file_io_list;
			kFileIoManager->LockMutex();
			kFileIoManager->MoveQueue(file_io_list);
			kFileIoManager->UnlockMutex();

			if (file_io_list.empty()) {
				Sleep(500);
			}

			vector<FileIoInfo> events;

			// Move events from the queue into the map grouped by requestor_pid
			while (file_io_list.empty() == false)
			{
				FileIoInfo event(move(file_io_list.front()));
				file_io_list.pop();

				const auto& path = fs::path(event.path).parent_path().wstring();
				kFileCache->Erase(event.path);

				if (DiscardEventByPid(event.requestor_pid) == true
					|| event.is_modified == false) {
					continue;
				}

				auto verdict = hp.GetHoneyFolderType(path);
				if (verdict == honeypot::HoneyType::kNotHoney) {
					continue;
				}
				auto& ele = mp[path];
				ele.change_count++;
				if (ele.first_add == 0) ele.first_add = get_now();
			}

			vector<pair<wstring, QueueInfo>> v;
			for (const auto& x : mp) {
				if (get_now() - x.second.last_scan >= 60LL * 5) {
					v.push_back({ x.first, x.second });
				}
			}
			auto it = std::max_element(v.begin(), v.end(),
				[](const pair<wstring, QueueInfo>& a, const pair<wstring, QueueInfo>& b) {
					const auto& a_ele = a.second;
					const auto& b_ele = b.second;
					if (a_ele.type != b_ele.type) {
						return a_ele.type < b_ele.type;
					}
					else if (a_ele.first_add != b_ele.first_add) {
						return a_ele.first_add > b_ele.first_add;
					}
					return a_ele.change_count < b_ele.change_count;
				});
			if (it == v.end())
			{
				Sleep(100);
				continue;
			}
			auto& x = mp[it->first];
			if (get_now() - x.first_add >= (x.type == HoneyType::kHoneyIsolated ? 10 : 60))
			{
				if (IsDirAttackedByRansomware(it->first, x.type) == true)
				{
					debug::DebugPrintW(L"Some process is ransomware. It attacked %ws", it->first.c_str());
				}
				else
				{
					PrintDebugW(L"Scanned %ws, no problem", it->first.c_str());
				}
				x.change_count = 0;
				x.first_add = 0;
				x.last_scan = get_now();
			}
			else
			{
				Sleep(100);
				continue;
			}
		}
	}

	bool Evaluator::IsDirAttackedByRansomware(const wstring& dir_path, const HoneyType& dir_type)
	{
		vector<wstring> paths;
		if (manager::DirExist(dir_path) == false) {
			return false;
		}
		auto honey_names(hp.GetHoneyNames());
		set<ull> s;
		for (const auto& name : honey_names)
		{
			s.insert(GetWstrHash(name));
		}

		try {
			for (const auto& entry : fs::directory_iterator(dir_path)) {
				const auto file_path = entry.path().wstring();
				if (dir_type == HoneyType::kHoneyBlendIn) {
					wstring name = ulti::ToLower(manager::GetFileNameNoExt(entry.path().filename()));
					auto h = GetWstrHash(name);
					if (s.find(h) == s.end()) {
						continue;
					}
				}
				paths.push_back(file_path);
				if (paths.size() > 30)
				{
					break;
				}
			}
		}
		catch (...) {}

		vector<vector<string>> dvvs;
		size_t valid_total_cnt = 0;
		for (auto& path : paths)
		{
#ifdef _M_IX86
			// Compatible with TRID
			if (ulti::StrToWstr(ulti::WstrToStr(path)) != path)
			{
				continue;
			}
#endif // _M_IX86
			DWORD status;
			ull file_size = 0;
			FileCacheInfo info = {};
			if (kFileCache->Get(path, info) == false)
			{
				auto types = kFileType->GetTypes(path, &status, &file_size);
				if (status == ERROR_SUCCESS)
				{
					++valid_total_cnt;
					dvvs.push_back(types);
				}
				if (file_size != 0) {
					info.size = file_size;
					kFileCache->Add(path, info);
				}
			}
			else
			{
				file_size = info.size;
				if (file_size < FILE_MIN_SIZE_SCAN || file_size > FILE_MAX_SIZE_SCAN)
				{
					continue;
				}
				++valid_total_cnt;
				dvvs.push_back(info.types);
			}
		}

		// Need config for file randomization for each dir
		vector<vector<string>> hvvs(hp.GetHoneyTypes());

		Matcher m;
		m.SetInput(dvvs, hvvs);
		auto ans = m.Solve();

		return BelowTypeThreshold((size_t)ans, valid_total_cnt);
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