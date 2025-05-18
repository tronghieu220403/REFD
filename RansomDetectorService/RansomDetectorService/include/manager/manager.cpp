#include "manager.h"
#include "file_type_iden.h"

namespace manager {

	void Init()
	{
		PrintDebugW(L"Manager initialized");
		kCurrentPid = GetCurrentProcessId();
		kFileIoManager = new FileIoManager();
		InitDosDeviceCache();
	}

	void Cleanup()
	{
		PrintDebugW(L"Manager cleaned up");
		delete kFileIoManager;
	}

	// Event mutex
    std::mutex event_mutex_;
    // Store grouped events by requestor_pid
	std::unordered_map<size_t, std::vector<FileIoInfo>> global_merged_events_by_pid;
	// Map to store hash of new_path_list and its corresponding event index per PID
	std::unordered_map<size_t, std::unordered_map<size_t, int>> path_hash_to_merged_index_by_pid;
    // ProcessInfo structure to store process information
	std::unordered_map<size_t, ProcessInfo> global_process_map; // PID -> ProcessInfo

    std::chrono::steady_clock::time_point last_evaluation_time = std::chrono::steady_clock::now();

    std::chrono::steady_clock::time_point last_clear_tmp_time = std::chrono::steady_clock::now();

	void ProcessDataQueue()
    {
		std::lock_guard<std::mutex> lock(event_mutex_);

		auto current_time = std::chrono::steady_clock::now();
		
		std::chrono::minutes elapsed = std::chrono::duration_cast<std::chrono::minutes>(current_time - last_evaluation_time);

        // Exceed 15 minutes then clear the global merged events and path hash map
		if (elapsed.count() >= 15) {
            last_evaluation_time = current_time;
            PrintDebugW(L"Clear global merged events and path hash map");
            // Clear the global merged events and path hash map
            global_merged_events_by_pid.clear();
            path_hash_to_merged_index_by_pid.clear();
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

		PrintDebugW(L"Merging events");
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
            auto iterator_path_hash_to_merged_index = path_hash_to_merged_index_by_pid.find(pid);
            if (iterator_path_hash_to_merged_index == path_hash_to_merged_index_by_pid.end())
            {
                path_hash_to_merged_index_by_pid[pid] = std::unordered_map<size_t, int>();
                iterator_path_hash_to_merged_index = path_hash_to_merged_index_by_pid.find(pid);
            }
			std::unordered_map<size_t, int>& path_hash_to_merged_index = iterator_path_hash_to_merged_index->second;

			auto iterator_process_map = global_process_map.find(pid);
			if (iterator_process_map == global_process_map.end())
			{
				global_process_map[pid] = ProcessInfo();
				global_process_map[pid].pid = pid;
				iterator_process_map = global_process_map.find(pid);
			}

            //PrintDebugW(L"Pid %d: %d events", pid, events.size());
			// Process each event
			for (size_t i = 0; i < events.size(); ++i)
			{
				FileIoInfo& current_event = events[i];
				bool merged = false;

				// Only attempt to merge if the event is renamed
				if (current_event.is_renamed) {
                    //PrintDebugW(L"Process %d: Renamed event", pid);
					size_t current_hash = manager::GetPathHash(current_event.current_path);

					// Try to find the matching event based on the hash map
					auto it = path_hash_to_merged_index.find(current_hash);

					if (it != path_hash_to_merged_index.end())
					{ // Begin to merge
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

						if (last_event.is_renamed == true && current_event.new_path_list.back().empty() == false)
						{
							last_event.new_path_list.push_back(current_event.new_path_list.back());
							// Update the path hash map
							size_t new_path_hash = manager::GetPathHash(last_event.new_path_list.back());

                            // Nếu trước đó chưa bị modified mà bây giờ bị modified thì phải lấy type
							if (current_event.is_modified == true && last_event.is_modified == false && last_event.is_new_get_type == false)
							{
								if (FileExist(TEMP_DIR + std::to_wstring(new_path_hash)) == true)
								{
									last_event.new_backup_name = std::to_wstring(new_path_hash);
									last_event.new_types = kTrID->GetTypes(TEMP_DIR + last_event.new_backup_name);
									last_event.is_new_get_type = true;
								}
								else if (FileExist(current_event.new_path_list.back()) == true)
								{
									last_event.new_backup_name = manager::CopyToTmp(current_event.new_path_list.back());
									if (last_event.new_backup_name != L"") {
										last_event.new_types = kTrID->GetTypes(TEMP_DIR + last_event.new_backup_name);
										last_event.is_new_get_type = true;
									}
								}
							}
							auto index = it->second;
							path_hash_to_merged_index.erase(it);
							path_hash_to_merged_index[new_path_hash] = index;
						}
						
                        // Nếu trước đó chưa lấy type mà có đã bị thay đổi
						if (last_event.is_current_get_type == false && last_event.is_modified == true)
						{
							if (last_event.current_backup_name == L"")
							{
								if (FileExist(TEMP_DIR + current_event.current_backup_name) == true)
								{
									last_event.current_backup_name = current_event.current_backup_name;
                                    last_event.current_types = std::move(kTrID->GetTypes(TEMP_DIR + last_event.current_backup_name));
                                    last_event.is_current_get_type = true;
								}
								else if (FileExist(current_event.current_path) == true)
								{
									last_event.current_backup_name = manager::CopyToTmp(current_event.current_path);
                                    if (last_event.current_backup_name != L"") {
                                        last_event.current_types = std::move(kTrID->GetTypes(TEMP_DIR + last_event.current_backup_name));
                                        last_event.is_current_get_type = true;
                                    }
								}
							}
							else
							{
                                if (FileExist(TEMP_DIR + current_event.current_backup_name) == true)
                                {
                                    last_event.current_backup_name = current_event.current_backup_name;
                                    last_event.current_types = std::move(kTrID->GetTypes(TEMP_DIR + last_event.current_backup_name));
                                    last_event.is_current_get_type = true;
                                }
                                else if (FileExist(current_event.current_path) == true)
                                {
                                    last_event.current_backup_name = manager::CopyToTmp(current_event.current_path);
                                    if (last_event.current_backup_name != L"") {
                                        last_event.current_types = std::move(kTrID->GetTypes(TEMP_DIR + last_event.current_backup_name));
                                        last_event.is_current_get_type = true;
                                    }
                                }
							}
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
                        EvaluateProcess(pid);
						merged = true;
					}
				}

				// If the event wasn't merged, add it as a new event
				if (merged == false) {
                    //PrintDebugW(L"Process %d: New event", pid);
                    // Reupdate the current back up name
					if (current_event.current_backup_name.empty() == false && FileExist(TEMP_DIR + current_event.current_backup_name) == false) {
						if (current_event.is_created == false && (current_event.is_renamed | current_event.is_deleted | current_event.is_created) == false && FileExist(current_event.current_path) == true)
						{
							current_event.current_backup_name = manager::CopyToTmp(current_event.current_path);
                        }
						else {
							current_event.current_backup_name = L"";
						}
					}
					else {
						current_event.current_backup_name = current_event.current_backup_name;
					}

					if (current_event.current_backup_name.empty() == false && current_event.is_created == false && (current_event.is_modified | current_event.is_deleted | current_event.is_created) == false)
					{
						current_event.current_types = kTrID->GetTypes(current_event.current_backup_name);
						current_event.is_current_get_type = true;
					}

					if (current_event.is_renamed == true && current_event.is_modified == true)
					{
                        if (FileExist(current_event.new_backup_name) == false)
                        {
                            if (current_event.new_path_list.back().empty() == false && FileExist(current_event.new_path_list.back()) == true)
                            {
                                current_event.new_backup_name = manager::CopyToTmp(current_event.new_path_list.back());
                                if (current_event.new_backup_name != L"") {
                                    current_event.new_types = kTrID->GetTypes(TEMP_DIR + current_event.new_backup_name);
                                    current_event.is_new_get_type = true;
                                }
                            }
                            else
                            {
                                current_event.new_backup_name = L"";
                            }
                        }
                        else
                        {
                            current_event.new_backup_name = current_event.new_backup_name;
                            current_event.new_types = kTrID->GetTypes(current_event.new_backup_name);
                            current_event.is_new_get_type = true;
                        }	
					}
					/*
					PrintDebugW(L"Add new event: requestor_pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d, current_path: %ws, new_path_list: %ws",
						current_event.requestor_pid,
						current_event.is_modified,
						current_event.is_deleted,
						current_event.is_created,
						current_event.is_renamed,
						current_event.current_path.c_str(),
                        current_event.new_path_list.back().c_str());
					*/
					merged_events.push_back(current_event);
					
					// Update the path hash map
					size_t new_path_hash;
					if (current_event.new_path_list.back().empty() == false)
					{
						new_path_hash = manager::GetPathHash(current_event.new_path_list.back());
					}
					else
					{
						new_path_hash = manager::GetPathHash(current_event.current_path);
					}

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
					path_hash_to_merged_index[new_path_hash] = merged_events.size() - 1;
					EvaluateProcess(pid);
				}
			}
		}
        PrintDebugW(L"End processing data queue");
	}

	void EvaluateProcesses()
	{
		std::lock_guard<std::mutex> lock(event_mutex_);

		PrintDebugW(L"Start evaluating processes");

		PrintDebugW(L"Count events for each process");
		for (auto& pid_events : global_merged_events_by_pid) {
			std::vector<FileIoInfo>& events = pid_events.second;
			auto pid = pid_events.first;
            auto it = global_process_map.find(pid);
            if (it == global_process_map.end())
            {
                global_process_map[pid] = ProcessInfo();
                global_process_map[pid].pid = pid;
                it = global_process_map.find(pid);
            }
			for (auto& event : events)
			{
				if (event.is_deleted == true)
				{
                    //PrintDebugW(L"Process %d: deleted file %ws", pid, event.current_path.c_str());
					it->second.deleted_count += 1;
				}
				else if (event.is_created == true && event.is_modified == true)
				{
                    //PrintDebugW(L"Process %d: created and modified file %ws", pid, event.current_path.c_str());
                    it->second.created_write_count += 1;
                }
				else if (event.is_modified == true)
				{
                    //PrintDebugW(L"Process %d: modified file %ws", pid, event.current_path.c_str());
					it->second.overwrite_count += 1;
				}
			}
		}
#ifdef _DEBUG
        PrintDebugW(L"Print debug processes and event info:");
		for (auto& pid_events : global_merged_events_by_pid) {
			std::vector<FileIoInfo>& events = pid_events.second;
			auto pid = pid_events.first;
            PrintDebugW(L"Process %d: %d events", pid, events.size());
			auto it = global_process_map.find(pid);
			if (it == global_process_map.end())
			{
				continue;
			}
            PrintDebugW(L"Process %d: deleted_count: %d, created_write_count: %d, overwrite_count: %d",
                it->second.pid,
                it->second.deleted_count,
                it->second.created_write_count,
                it->second.overwrite_count);
			for (const auto& event : events)
			{
				std::wstring new_path_list_str = L"<";
				for (const auto& new_path : event.new_path_list)
				{
                    new_path_list_str += new_path + L", ";
				}
				new_path_list_str[new_path_list_str.size() - 2] = L'>';
				std::string current_types_str = "<";
                for (const auto& type : event.current_types)
                {
                    current_types_str += type + ", ";
                }
				current_types_str[current_types_str.size() - 2] = L'>';
				std::string new_types_str = "<";
                for (const auto& type : event.new_types)
                {
                    new_types_str += type + ", ";
                }
                new_types_str[new_types_str.size() - 2] = L'>';
                PrintDebugW(L"Event: requestor_pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d, current_path: %ws, new_path_list: %ws, current_types: %ws, new_types: %ws",
                    event.requestor_pid,
                    event.is_modified,
                    event.is_deleted,
                    event.is_created,
                    event.is_renamed,
                    event.current_path.c_str(),
                    new_path_list_str.c_str(),
                    ulti::StrToWStr(current_types_str).c_str(),
					ulti::StrToWStr(new_types_str).c_str());
			}
		}
#endif // _DEBUG



        PrintDebugW(L"End evaluating processes");
	}

	void EvaluateProcess(ULONG pid)
	{
        PrintDebugW(L"Start evaluating process, pid %d", pid);
        auto it = global_process_map.find(pid);
        if (it == global_process_map.end())
        {
            global_process_map[pid] = ProcessInfo();
            global_process_map[pid].pid = pid;
            it = global_process_map.find(pid);
        }
        auto& process_info = it->second;
        std::chrono::minutes elapsed = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - process_info.last_evaluation_time);
		if (process_info.is_first_evaluation == false && elapsed.count() < INTERVAL_MINUTE_PROCESS_EVALUATION)
		{
			return;
		}

		auto& events = global_merged_events_by_pid[pid];
        bool is_ransomware = false;

		if (process_info.overwrite_count > 5)
		{
			int cnt = 0;
			for (auto& event : events)
			{
                if (event.is_modified == true && event.is_deleted == false && event.is_created == false)
                {
					if (event.type_match == TYPE_MATCH_NOT_EVALUATED)
					{
						if (event.is_current_get_type == false)
						{
							PrintDebugW(L"WARNING: Process %d: File %ws is modified, but not evaluated", pid, event.current_path.c_str());
							continue;
						}
						std::vector<std::string> newest_types;
						if (event.is_renamed)
						{
                            if (event.is_new_get_type == false)
                            {
                                PrintDebugW(L"WARNING: Process %d: File %ws is modified and renamed, but not evaluated", pid, event.current_path.c_str());
                                continue;
                            }
                            newest_types = event.new_types;
						}
						else
						{
							auto newest_backup_path = CopyToTmp(event.current_path, true);
							if (newest_backup_path == L"") {
								PrintDebugW(L"WARNING: Process %d: Can not create backup for file %ws", pid, event.current_path.c_str());
								continue;
							}
							newest_types = kTrID->GetTypes(TEMP_DIR + event.current_backup_name);
						}

						bool is_valid_file = type_iden::HasCommonType(newest_types, event.current_types);
						if (is_valid_file == false)
						{
							auto ext = ulti::WstrToStr(GetFileExtension(event.current_path));
							if (event.is_renamed == true && type_iden::kExtensionMap.find(ext) != type_iden::kExtensionMap.end())
							{
								is_valid_file = type_iden::HasCommonType(newest_types, type_iden::kExtensionMap[ext]);
							}
							else if (event.is_renamed == false) {
                                ext = ulti::WstrToStr(GetFileExtension(event.new_path_list.back()));
                                if (type_iden::kExtensionMap.find(ext) != type_iden::kExtensionMap.end())
                                {
                                    is_valid_file = type_iden::HasCommonType(newest_types, type_iden::kExtensionMap[ext]);
                                }
                                else
                                {
                                    PrintDebugW(L"WARNING: Process %d: File %ws is modified, but can not ext", pid, event.current_path.c_str());
                                    continue;
                                }
							}
						}
						event.type_match = (is_valid_file == false) ? TYPE_HAS_COMMON : TYPE_MISMATCH;
					}
					if (event.type_match == TYPE_MISMATCH)
					{
						PrintDebugW(L"Type mismatch: %ws", event.current_path.c_str());
						cnt += 1;
					}
					else
					{
                        PrintDebugW(L"Type match: %ws", event.current_path.c_str());
					}
				}
			}
            if (cnt > 1)
            {
                is_ransomware = true;
            }
		}
		else if (process_info.deleted_count > 5 && process_info.created_write_count > 5)
		{
            int true_deleted_count = 0;
			for (const auto& event : events)
			{
				if (event.is_deleted == true)
				{
					auto ext = ulti::WstrToStr(GetFileExtension(event.current_path));
					if (type_iden::kExtensionMap.find(ext) != type_iden::kExtensionMap.end())
					{
						true_deleted_count += 1;
					}
					else if (event.is_renamed == true)
					{
						ext = ulti::WstrToStr(GetFileExtension(event.new_path_list.back()));
						if (type_iden::kExtensionMap.find(ext) != type_iden::kExtensionMap.end())
						{
							true_deleted_count += 1;
						}
					}
				}
            }
			if (true_deleted_count < 5)
			{
                goto end_evaluate_process;
			}
			int cnt = 0;
			for (auto& event : events)
			{
				if (event.is_deleted == true)
				{
					continue;
				}
				if (event.is_created == true && event.is_modified == true)
				{
					if (event.type_match == TYPE_MATCH_NOT_EVALUATED)
					{
						if (event.is_current_get_type == false)
						{
							PrintDebugW(L"WARNING: Process %d: File %ws is created and modified, but not evaluated", pid, event.current_path.c_str());
							continue;
						}
						std::vector<std::string> newest_types;
						std::wstring current_path;
						if (event.is_renamed)
						{
							if (FileExist(event.new_path_list.back()) == true)
							{
                                current_path = event.new_path_list.back();
                                auto newest_backup_path = CopyToTmp(current_path, true);
                                if (newest_backup_path == L"") {
                                    PrintDebugW(L"WARNING: Process %d: Can not create backup for file %ws", pid, current_path.c_str());
                                    continue;
                                }
                                newest_types = kTrID->GetTypes(TEMP_DIR + event.new_backup_name);
							}
						}
						else
						{
                            current_path = event.current_path;
                            auto newest_backup_path = CopyToTmp(current_path, true);
                            if (newest_backup_path == L"") {
                                PrintDebugW(L"WARNING: Process %d: Can not create backup for file %ws", pid, current_path.c_str());
                                continue;
                            }
                            newest_types = kTrID->GetTypes(TEMP_DIR + event.current_backup_name);
						}
						if (newest_types.size() == 1 && newest_types.back() == "")
						{
                            event.type_match = TYPE_MISMATCH;
                            cnt += 1;
						}
						else
						{
                            event.type_match = TYPE_HAS_COMMON;
						}
					}
                    if (event.type_match == TYPE_MISMATCH)
                    {
                        PrintDebugW(L"Type null: %ws", event.current_path.c_str());
                        cnt += 1;
                    }
                    else
                    {
                        PrintDebugW(L"Type match: %ws", event.current_path.c_str());
                    }
				}
			}
			if (cnt > 5)
			{
				is_ransomware = true;
			}
		}
		else
		{
            PrintDebugW(L"Process %d: Not enough events to evaluate", pid);
			return;
		}

		if (is_ransomware == true)
		{
            PrintDebugW(L"DANGER: Process %d is ransomware", pid);
		}
	end_evaluate_process:
		process_info.is_first_evaluation = false;
		process_info.last_evaluation_time = std::chrono::steady_clock::now();
        PrintDebugW(L"End evaluating process, pid %d", pid);
	}

	bool DiscardEventByPid(size_t issuing_pid)
	{
		if (issuing_pid == 0 || 
			//issuing_pid == 4
			issuing_pid == kCurrentPid)
		{
			return true;
		}
#ifdef _DEBUG
        if (issuing_pid == (DWORD)(-1) || issuing_pid == 4 || issuing_pid == 0)
        {
            return true;
        }
#endif // _DEBUG

		return false;
	}

}