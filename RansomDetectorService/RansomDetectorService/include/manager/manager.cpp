#include "manager.h"

namespace manager {

	void Init()
	{
		PrintDebugW(L"Manager initialized");
		kCurrentPid = GetCurrentProcessId();
		kProcMan = new ProcessManager();
		kFileIoManager = new FileIoManager();
	}

	void Cleanup()
	{
		PrintDebugW(L"Manager cleaned up");
		delete kFileIoManager;
		delete kProcMan;
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
			FileIoInfo event = file_io_list.front();
			file_io_list.pop();
			events_by_pid[event.requestor_pid].push_back(event);
		}

		// Iterate through each group of events with the same requestor_pid
		for (auto& pid_events : events_by_pid) {
			std::vector<FileIoInfo>& events = pid_events.second;
			std::vector<FileIoInfo> merged_events;

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
						last_event.is_deleted |= current_event.is_deleted;
						last_event.is_created |= current_event.is_created;
						last_event.is_renamed |= current_event.is_renamed;
						
						last_event.new_path_list.push_back(current_event.new_path_list.back());
						// If current_event.new_backup_name exists, update it

						merged = true;
						// Update the path hash map
						ull new_path_hash = manager::GetPathHash(last_event.new_path_list.back());
						auto index = it->second;
						path_hash_to_merged_index.erase(it);
						path_hash_to_merged_index[new_path_hash] = index;
					}
				}

				// If the event wasn't merged, add it as a new event
				if (merged == false) {
					// If current_event.current_backup_name not exists, skip events.
					if (FileExist(TEMP_DIR + current_event.current_backup_name) == false)
					{
						continue;
					}
					
					merged_events.push_back(current_event);
					// Update the path hash map
					ull new_path_hash = manager::GetPathHash(current_event.new_path_list.back());
					path_hash_to_merged_index[new_path_hash] = merged_events.size() - 1;
				}

			}

			// Add all merged events to the result queue
			for (auto& event : merged_events) {
				merged_list_by_pid[pid_events.first].push_back(event);
			}
		}

	}

	bool OverallEventFilter(uint64_t issuing_pid)
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