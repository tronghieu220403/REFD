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

		std::chrono::minutes elapsed = std::chrono::duration_cast<std::chrono::minutes>(current_time - last_evaluation_time);

		// Exceed 15 minutes then clear the global merged events and path hash map
		if (elapsed.count() >= 15) {
			last_evaluation_time = current_time;
			PrintDebugW(L"Clear global merged events and path hash map");
			// Clear the global merged events and path hash map
			global_merged_events_by_pid.clear();
			global_path_hash_to_merged_index_by_pid.clear();
		}

		// Exceed 3 hour then clear the temp files
		elapsed = std::chrono::duration_cast<std::chrono::minutes>(current_time - last_evaluation_time);
		if (elapsed.count() >= 60 * 3) {
			last_clear_tmp_time = current_time;
			PrintDebugW(L"Clear temp files");
			ClearTmpFiles();
		}

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

		// Move events from the queue into the map grouped by requestor_pid
		while (!file_io_list.empty())
		{
			FileIoInfo& event = file_io_list.front();
			auto pid = event.requestor_pid; // std::move will make event.requestor_pid invalid if we use: events_by_pid[event.requestor_pid].push_back(std::move(event));
			if (DiscardEventByPid(pid) == true) {
				file_io_list.pop();
				continue;
			}
			events_by_pid[pid].push_back(std::move(event));
			file_io_list.pop();
		}

		// PrintDebugW(L"Merging events");
		// Iterate through each group of events with the same requestor_pid
		for (auto& pid_events : events_by_pid)
		{
			std::vector<FileIoInfo>& events = pid_events.second;

			auto iterator_merged_events = global_merged_events_by_pid.find(pid_events.first);
			if (iterator_merged_events == global_merged_events_by_pid.end())
			{
				global_merged_events_by_pid[pid_events.first] = std::vector<FileIoInfo>();
				iterator_merged_events = global_merged_events_by_pid.find(pid_events.first);
			}
			std::vector<FileIoInfo>& merged_events = iterator_merged_events->second;
			auto pid = pid_events.first;

			// Map to store hash of new_path_list and its corresponding event index
			auto iterator_path_hash_to_merged_index = global_path_hash_to_merged_index_by_pid.find(pid);
			if (iterator_path_hash_to_merged_index == global_path_hash_to_merged_index_by_pid.end())
			{
				global_path_hash_to_merged_index_by_pid[pid] = std::unordered_map<ull, int>();
				iterator_path_hash_to_merged_index = global_path_hash_to_merged_index_by_pid.find(pid);
			}
			std::unordered_map<ull, int>& path_hash_to_merged_index = iterator_path_hash_to_merged_index->second;

			auto iterator_process_map = global_process_map.find(pid);
			if (iterator_process_map == global_process_map.end())
			{
				global_process_map[pid] = ProcessInfo();
				global_process_map[pid].pid = pid;
				iterator_process_map = global_process_map.find(pid);
			}

			//PrintDebugW(L"Pid %d: %d events", pid, events.size());
			// Process each event
			for (int i = 0; i < (int)events.size(); ++i)
			{
				FileIoInfo& current_event = events[i];

				// Only attempt to merge if the event is renamed
				ull current_hash = manager::GetPathHash(current_event.path_list[0]);

				// Try to find the matching event based on the hash map
				auto it = path_hash_to_merged_index.find(current_hash);

				if (it != path_hash_to_merged_index.end())
				{	// Begin to merge
					//PrintDebugW(L"Process %d: Found matching event in hash map", pid);

					// If a match is found, merge the events
					FileIoInfo& last_event = merged_events[it->second];

					// Store the last_event flags
					auto old_is_deleted = last_event.is_deleted;
					auto old_is_created = last_event.is_created;
					auto old_is_renamed = last_event.is_renamed;
					auto old_is_modified = last_event.is_modified;

					// Update the last_event with the current_event
					last_event.is_modified |= current_event.is_modified;

					if (last_event.is_deleted == true && current_event.is_created == true) {
						// File was deleted and then created again 
						last_event.is_deleted = false;
						last_event.is_created = false;
					}
					else {
						last_event.is_deleted |= current_event.is_deleted;
						last_event.is_created |= current_event.is_created;
						last_event.is_renamed |= current_event.is_renamed;
					}
					if (current_event.is_renamed) {
						//PrintDebugW(L"Process %d: Renamed event", pid);
						last_event.path_list.push_back(current_event.path_list.back());
						last_event.backup_name_list.push_back(current_event.backup_name_list.back());
						// Update the path hash map
						ull new_path_hash = manager::GetPathHash(last_event.path_list.back());
						auto index = it->second;
						path_hash_to_merged_index.erase(it);
						path_hash_to_merged_index[new_path_hash] = index;
					}

					// Update the global process map
					if (last_event.is_deleted == true && old_is_deleted == false)
					{
						iterator_process_map->second.deleted_count += 1;
					}
					else if ((old_is_modified + old_is_created) < 2 && last_event.is_created == true && last_event.is_modified == true)
					{
						iterator_process_map->second.created_write_count += 1;
						if (old_is_modified == true && old_is_created == false)
						{
							iterator_process_map->second.overwrite_count -= 1;
						}
					}
					else if (last_event.is_modified == true && old_is_modified == false)
					{
						iterator_process_map->second.overwrite_count += 1;
					}
				}
				else
				{	// If the event wasn't merged, add it as a new event
					merged_events.push_back(current_event);

					// Update the path hash map
					ull new_path_hash = manager::GetPathHash(current_event.path_list.back());
					path_hash_to_merged_index[new_path_hash] = merged_events.size() - 1;

					// Update the global process map
					if (current_event.is_deleted == true)
					{
						iterator_process_map->second.deleted_count += 1;
					}
					else if (current_event.is_created == true && current_event.is_modified == true)
					{
						iterator_process_map->second.created_write_count += 1;
					}
					else if (current_event.is_modified == true)
					{
						iterator_process_map->second.overwrite_count += 1;
					}
				}
				if (EvaluateProcess(pid) == true)
				{
					PrintDebugW(L"Ransomware detected");
				}
			}
		}
		PrintDebugW(L"End processing data queue");
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
#ifdef _DEBUG
		std::wstring paths_str = L"<";
		for (const auto& path : event.path_list)
		{
			paths_str += path + L", ";
		}
		paths_str[paths_str.size() - 2] = L'>';
		std::wstring backup_paths_str = L"<";
		for (const auto& path : event.backup_name_list)
		{
			backup_paths_str += path + L", ";
		}
		backup_paths_str[backup_paths_str.size() - 2] = L'>';
		PrintDebugW(L"AnalyzeEvent: %ws, is modified %d, is_created %d, is_renamed %d, is_deleted %d, backup list %ws", paths_str.c_str(), event.is_modified, event.is_created, event.is_renamed, event.is_deleted, backup_paths_str.c_str());
#endif // _DEBUG

		int old_index = -1;
		int new_index = -1;

        PrintDebugW(L"Get old types");
		if (event.is_created == false && event.old_types.size() == 0) {
			for (int i = 0; i < (int)event.path_list.size(); i++) {
				if (event.backup_name_list[i].size() == 0)
				{
					continue;
				}
				auto backup_path = TEMP_DIR + event.backup_name_list[i];
				if (FileExist(backup_path) == true)
				{
					event.old_types = std::move(kTrID->GetTypes(backup_path));
					DeleteFileW(backup_path.c_str());
				}
			}
		}
		if (old_index == -1) {
			old_index = 0;
		}

        PrintDebugW(L"Get new types");

		if (event.new_types.size() == 0)
		{
			for (int i = (int)event.path_list.size() - 1; i >= old_index; --i) {
				if (FileExist(event.path_list[i]) == true)
				{
					new_index = i;
					std::wstring backup_path = TEMP_DIR + CopyToTmp(event.path_list[i], true);
					event.new_types = std::move(kTrID->GetTypes(backup_path));
					DeleteFileW(backup_path.c_str());
				}
			}

			if (new_index == -1)
			{
				//event.type_match = TYPE_NO_EVALUATION;
				PrintDebugW(L"Skipped this event.");
				return false;
			}
		}

        PrintDebugW("End getting types");

		bool is_default_types_matched = false;
		std::string ext;
		for (int i = 0; i < (int)event.path_list.size(); i++) {
			auto ext_tmp = ulti::WstrToStr(GetFileExtension(event.path_list[i]));
			if (type_iden::kExtensionMap.find(ext_tmp) != type_iden::kExtensionMap.end()) {
				ext = ext_tmp;
			}
		}
		if (ext != "") {
			const std::vector<std::string>& ext_accepted_types = type_iden::kExtensionMap[ext];
			is_default_types_matched = type_iden::HasCommonType(ext_accepted_types, event.new_types);
			PrintDebugW(L"is_default_types_matched: %d", is_default_types_matched);
		}
		else
		{
			is_default_types_matched = true;
		}

		bool is_types_matched_after_modified = false;
		if (event.is_created == false)
		{
            if ((event.old_types.size() == 1 && event.old_types.front() == "") || event.old_types.size() == 0)
            {
				is_types_matched_after_modified = true;
            }
			else
			{
				is_types_matched_after_modified = type_iden::HasCommonType(event.old_types, event.new_types);
			}
			PrintDebugW(L"is_types_matched_after_modified: %d", is_types_matched_after_modified);
		}

		if (ext == "" && event.is_created == true) {
			event.type_match = ((event.new_types.size() == 1 && event.new_types.front() == "") || event.new_types.size() == 0) ? TYPE_NULL : TYPE_NOT_NULL;
		}
		else {
			event.type_match = ((is_default_types_matched | is_types_matched_after_modified) == true) ? TYPE_HAS_COMMON : TYPE_MISMATCH ;
		}

        PrintDebugW(L"old_types %ws, new_types %ws, default %ws", type_iden::CovertTypesToString(event.old_types).c_str(), type_iden::CovertTypesToString(event.new_types).c_str(),
			type_iden::CovertTypesToString((type_iden::kExtensionMap.find(ext) != type_iden::kExtensionMap.end()) ? type_iden::kExtensionMap[ext] : std::vector<std::string>()).c_str());

#ifdef _DEBUG
		switch (event.type_match)
		{
		case TYPE_MATCH_NOT_EVALUATED:
			PrintDebugW(L"event.type_match: TYPE_MATCH_NOT_EVALUATED");
			break;

		case TYPE_MISMATCH:
			PrintDebugW(L"event.type_match: TYPE_MISMATCH");
			break;

		case TYPE_HAS_COMMON:
			PrintDebugW(L"event.type_match: TYPE_HAS_COMMON");
			break;

		case TYPE_NULL:
			PrintDebugW(L"event.type_match: TYPE_NULL");
			break;

		case TYPE_NOT_NULL:
			PrintDebugW(L"event.type_match: TYPE_NOT_NULL");
			break;

		case TYPE_NO_EVALUATION:
			PrintDebugW(L"event.type_match: TYPE_NO_EVALUATION");
			break;

		default:
			break;
		}
#endif // _DEBUG

		return true;
	}


	bool Evaluator::EvaluateProcess(ULONG pid)
	{
		auto it = global_process_map.find(pid);
		if (it == global_process_map.end())
		{
			global_process_map[pid] = ProcessInfo();
			global_process_map[pid].pid = pid;
			it = global_process_map.find(pid);
		}
		auto& process_info = it->second;
		std::chrono::seconds elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - process_info.last_evaluation_time);
		if (process_info.is_first_evaluation == false && elapsed.count() < INTERVAL_SECONDS_PROCESS_EVALUATION)
		{
			return false;
		}

		PrintDebugW(L"Start evaluating process, pid %d", pid);

		bool overwrite_mismatch_inc = false;
		bool created_write_null_count = false;

		auto& events = global_merged_events_by_pid[pid];
		bool is_ransomware = false;

		if (process_info.overwrite_count > MIN_FILE_COUNT)
		{
			for (int i = (int)process_info.last_index; i < (int)events.size(); ++i)
			{
				auto& event = events[i];
				if (event.type_match == TYPE_MATCH_NOT_EVALUATED && event.is_modified == true && event.is_created == false && event.is_deleted == false) {
					if (AnalyzeEvent(event) == true) {
						if (event.type_match == TYPE_MISMATCH) {
							process_info.overwrite_mismatch_count++;
							overwrite_mismatch_inc = true;
							if (process_info.overwrite_mismatch_count > MIN_FILE_COUNT)
							{
								break;
							}
						}
					}
				}
			}
			if (process_info.overwrite_mismatch_count > MIN_FILE_COUNT)
			{
				is_ransomware = true;
			}
		}
		else if (process_info.deleted_count > MIN_FILE_COUNT && process_info.created_write_count > MIN_FILE_COUNT)
		{
			for (int i = (int)process_info.last_index; i < (int)events.size(); ++i)
			{
				auto& event = events[i];
				if (event.is_deleted == true) {
					event.type_match = TYPE_NO_EVALUATION;
					//process_info.true_deleted_count += 1;

					for (const auto& path : event.path_list)
					{
						auto ext = ulti::WstrToStr(GetFileExtension(path));
						if (type_iden::kExtensionMap.find(ext) != type_iden::kExtensionMap.end())
						{
							process_info.true_deleted_count += 1;
							break;
						}
					}
				}
				else if (event.type_match == TYPE_MATCH_NOT_EVALUATED && event.is_modified == true && event.is_created == true)
				{
					if (AnalyzeEvent(event) == true) {
						if (event.type_match == TYPE_MISMATCH || event.type_match == TYPE_NULL) {
							process_info.created_write_null_count++;
							created_write_null_count = true;
							if (process_info.created_write_null_count > MIN_FILE_COUNT)
							{
								break;
							}
						}
					}
				}
			}

			if (process_info.true_deleted_count < MIN_FILE_COUNT)
			{
				goto end_evaluate_process;
			}

			if (process_info.created_write_null_count > MIN_FILE_COUNT)
			{
				is_ransomware = true;
			}
		}
		else
		{
			goto end_evaluate_process;
		}

		if (is_ransomware == true)
		{
			PrintDebugW(L"DANGER: Process %d is ransomware", pid);
			kFileIoManager->LockMutex();
			kFileIoManager->AddPidToWhitelist(pid);
			kFileIoManager->UnlockMutex();
		}

		if (overwrite_mismatch_inc == false && created_write_null_count == false)
		{
			PrintDebugW(L"Updating evaluation time", pid);
			process_info.is_first_evaluation = false;
			process_info.last_evaluation_time = std::chrono::steady_clock::now();
		}
		//process_info.last_index = events.size();
	end_evaluate_process:
		PrintDebugW("overwrite_count %d, overwrite_mismatch_count %d, deleted_count %d, created_write_count %d, true_deleted_count %d, created_write_null_count %d", process_info.overwrite_count, process_info.overwrite_mismatch_count, process_info.deleted_count, process_info.created_write_count, process_info.true_deleted_count, process_info.created_write_null_count);

		if (is_ransomware == true)
		{
			auto iterator_merged_events = global_merged_events_by_pid.find(pid);
			if (iterator_merged_events != global_merged_events_by_pid.end())
			{
				iterator_merged_events->second.clear();
			}

			auto iterator_path_hash_to_merged_index = global_path_hash_to_merged_index_by_pid.find(pid);
			if (iterator_path_hash_to_merged_index != global_path_hash_to_merged_index_by_pid.end())
			{
				iterator_path_hash_to_merged_index->second.clear();
			}

			auto iterator_process_map = global_process_map.find(pid);
			if (iterator_process_map != global_process_map.end())
			{
				iterator_process_map->second = manager::ProcessInfo();
			}
		}

		PrintDebugW(L"End evaluating process, pid %d", pid);
		return is_ransomware;
	}

	bool Evaluator::DiscardEventByPid(ULONG issuing_pid)
	{
		if (issuing_pid == 0 || issuing_pid == (DWORD)(-1) || issuing_pid == 4
			|| issuing_pid == kCurrentPid || kFileIoManager->IsPidInWhiteList(issuing_pid) == true)
		{
			return true;
		}

		return false;
	}

}