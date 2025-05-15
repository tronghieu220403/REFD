#include "manager.h"
#include "file_type_iden.h"

namespace manager {

	void Init()
	{
		PrintDebugW(L"Manager initialized");
		kCurrentPid = GetCurrentProcessId();
		kFileIoManager = new FileIoManager();
	}

	void Cleanup()
	{
		PrintDebugW(L"Manager cleaned up");
		delete kFileIoManager;
	}

	void EvaluateProcess()
	{
		PrintDebugW(L"Start evaluating processes");

		std::queue<FileIoInfo> file_io_list;

		kFileIoManager->LockMutex();
		kFileIoManager->MoveQueue(file_io_list);
		kFileIoManager->UnlockMutex();

		PrintDebugW(L"Number of file I/O events: %d", file_io_list.size());

		std::unordered_map<ULONG, std::vector<FileIoInfo>> merged_list_by_pid;

		// If the event list is empty, return an empty queue
		if (file_io_list.empty()) {
			return;
		}

		// Store events grouped by requestor_pid
		std::unordered_map<ULONG, std::vector<FileIoInfo>> events_by_pid;

		// Move events from the queue into the map grouped by requestor_pid
		while (!file_io_list.empty()) {
			FileIoInfo& event = file_io_list.front();
            auto pid = event.requestor_pid; // std::move will make event.requestor_pid invalid if we use: events_by_pid[event.requestor_pid].push_back(std::move(event));
			events_by_pid[pid].push_back(std::move(event));
			file_io_list.pop();
		}

		PrintDebugW(L"Merge events");
		// Iterate through each group of events with the same requestor_pid
		for (auto& pid_events : events_by_pid) {
			std::vector<FileIoInfo>& events = pid_events.second;
			std::vector<FileIoInfo> merged_events;
			auto pid = pid_events.first;

			// Map to store hash of new_path_list and its corresponding event index
			std::unordered_map<ull, int> path_hash_to_merged_index;

			// Process each event
			for (size_t i = 0; i < events.size(); ++i)
			{
				FileIoInfo& current_event = events[i];
				bool merged = false;

				// Only attempt to merge if the event is renamed
				if (current_event.is_renamed) {
					ull current_hash = manager::GetPathHash(current_event.current_path);

					// Try to find the matching event based on the hash map
					auto it = path_hash_to_merged_index.find(current_hash);

					if (it != path_hash_to_merged_index.end()) {
						// If a match is found, merge the events
						FileIoInfo& last_event = merged_events[it->second];
						last_event.is_modified |= current_event.is_modified;
						if (last_event.is_deleted == true && current_event.is_created == true)
						{
                            last_event.is_deleted = false;
                            last_event.is_created = false;
						}
						else
						{
							last_event.is_deleted |= current_event.is_deleted;
							last_event.is_created |= current_event.is_created;
							last_event.is_renamed |= current_event.is_renamed;
						}
						
						if (current_event.new_path_list.back().empty() == false)
						{
							last_event.new_path_list.push_back(current_event.new_path_list.back());
							// Update the path hash map
							ull new_path_hash = manager::GetPathHash(last_event.new_path_list.back());
							auto index = it->second;
							path_hash_to_merged_index.erase(it);
							path_hash_to_merged_index[new_path_hash] = index;
						}

						if (last_event.current_backup_name == L"")
						{
							if (FileExist(TEMP_DIR + current_event.current_backup_name) == true)
							{
								last_event.current_backup_name = current_event.current_backup_name;
							}
                            else if (FileExist(current_event.current_path) == true)
                            {
                                last_event.current_backup_name = manager::CopyToTmp(current_event.current_path);
                            }
						}

						merged = true;
					}
				}

				// If the event wasn't merged, add it as a new event
				if (merged == false) {
					if (FileExist(TEMP_DIR + current_event.current_backup_name) == false) {
						if (FileExist(current_event.current_path) == true) {
                            current_event.current_backup_name = manager::CopyToTmp(current_event.current_path);
						}
						else {
							current_event.current_backup_name = L"";
						}
					}
					else
					{
                        current_event.current_backup_name = TEMP_DIR + current_event.current_backup_name;
					}
					
					merged_events.push_back(current_event);
					// Update the path hash map
					ull new_path_hash;
					if (current_event.new_path_list.back().empty() == false)
					{
						new_path_hash = manager::GetPathHash(current_event.new_path_list.back());
					}
					else
					{
                        new_path_hash = manager::GetPathHash(current_event.current_path);
					}
					path_hash_to_merged_index[new_path_hash] = merged_events.size() - 1;
				}

			}

			// Add all merged events to the result queue
			for (auto& event : merged_events)
			{
				if (event.is_modified == false
					|| (event.is_deleted == true && event.is_created == true)
					)
				{
					continue;
				}
				merged_list_by_pid[pid].push_back(std::move(event));
			}
		}

		PrintDebugW(L"Count events for each process");
		std::unordered_map<size_t, ProcessInfo> process_map; // PID -> ProcessInfo
		for (auto& pid_events : merged_list_by_pid) {
			std::vector<FileIoInfo>& events = pid_events.second;
			auto pid = pid_events.first;
            PrintDebugW(L"Process %d: %d events", pid, events.size());
            auto it = process_map.find(pid);
            if (it == process_map.end())
            {
                process_map[pid] = ProcessInfo();
                process_map[pid].pid = pid;
                it = process_map.find(pid);
            }
			for (auto& event : events)
			{
				if (event.is_deleted == true)
				{
                    PrintDebugW(L"Process %d: deleted file %ws", pid, event.current_path.c_str());
					it->second.deleted_count += 1;
				}
				else if (event.is_created == true && event.is_modified == true)
				{
                    PrintDebugW(L"Process %d: created and modified file %ws", pid, event.current_path.c_str());
                    it->second.created_write_count += 1;
                }
				else if (event.is_modified == true)
				{
                    PrintDebugW(L"Process %d: modified file %ws", pid, event.current_path.c_str());
					it->second.overwrite_count += 1;
				}
                if (event.current_backup_name != L"")
                {
                    PrintDebugW(L"Process %d: current backup name %ws", pid, event.current_backup_name.c_str());
                    event.current_types = kTrID->GetTypes(TEMP_DIR + event.current_backup_name);
					std::string new_types_str = "<";
					for (const auto& type : event.current_types)
					{
						new_types_str += type + ", ";
					}
					new_types_str += ">";
					PrintDebugW(L"Process %d: current types %ws", pid, ulti::StrToWStr(new_types_str).c_str());
                }

				if (event.is_renamed == true)
				{
					for (int i = event.new_path_list.size() - 1; i >= 0; --i)
					{
						if (manager::FileExist(event.new_path_list[i]) == true)
						{
							PrintDebugW(L"Process %d: new path exist %ws", pid, event.new_path_list[i].c_str());
							event.new_backup_name = manager::CopyToTmp(event.new_path_list[i]);
							event.new_types = kTrID->GetTypes(event.new_backup_name);
							std::string new_types_str = "<";
							for (const auto& type : event.new_types)
							{
								PrintDebugW(L"Process %d: new types %ws", pid, ulti::StrToWStr(type).c_str());
							}
							new_types_str += ">";
							PrintDebugW(L"Process %d: new types %ws", pid, ulti::StrToWStr(new_types_str).c_str());
							break;
						}
					}
					if (event.new_backup_name == L"")
					{
						PrintDebugW(L"Process %d: new backup path not exist", pid);
						event.new_types = { HIEUNT_ERROR_FILE_NOT_FOUND_STR };
					}
				}
			}
		}
#ifdef _DEBUG
        PrintDebugW(L"Print debug processes and event info:");
		for (auto& pid_events : merged_list_by_pid) {
			std::vector<FileIoInfo>& events = pid_events.second;
			auto pid = pid_events.first;
            PrintDebugW(L"Process %d: %d events", pid, events.size());
			auto it = process_map.find(pid);
			if (it == process_map.end())
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
                new_path_list_str += L">";
                std::string current_types_str = "<";
                for (const auto& type : event.current_types)
                {
                    current_types_str += type + ", ";
                }
                current_types_str += ">";
                std::string new_types_str = "<";
                for (const auto& type : event.new_types)
                {
                    new_types_str += type + ", ";
                }
                new_types_str += ">";
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

	bool OverallEventFilter(size_t issuing_pid)
	{
		if (issuing_pid == 0 || 
			//issuing_pid == 4
			issuing_pid == kCurrentPid)
		{
			return false;
		}
#ifdef _DEBUG
        if (issuing_pid == (DWORD)(-1) || issuing_pid == 4 || issuing_pid == 0)
        {
            return true;
        }
#endif // _DEBUG

		return true;
	}

}