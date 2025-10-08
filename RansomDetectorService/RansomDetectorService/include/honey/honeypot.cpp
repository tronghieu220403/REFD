#include "honeypot.h"
#include "../ulti/debug.h"
#include "../ulti/support.h"
#include "../manager/known_folder.h"
#include "../manager/file_type_iden.h"
#include "../manager/file_manager.h"

namespace honeypot {

    bool HoneyPot::Init(const std::vector<std::wstring>& target_known_folders,
        const std::wstring& source_dir) {
        namespace fs = std::filesystem;

        try {
            // Ensure source directory exists.
            if (!fs::exists(source_dir) || !fs::is_directory(source_dir)) {
                PrintDebugW(L"Source directory does not exist: %ws",
                    source_dir.c_str());
                return false;
            }
            source_dir_ = source_dir;
            for (const auto& dir : target_known_folders)
            {
                honey_folders_.insert({ ulti::ToLower(dir), HoneyType::kHoneyBlendIn });
                honey_folders_.insert({ ulti::ToLower(dir) + L"\\avhpot", HoneyType::kHoneyIsolated});
                /*
                try
                {
                    fs::remove_all(ulti::ToLower(dir) + L"\\avhpot");
                }
                catch (...) {}
                */
            }

            for (const auto& hdir : honey_folders_)
            {
                //PrintDebugW(L"%ws", hdir.first.c_str());
            }

            // Iterate all tar_dir directories first, then copy all files into each tar_dir.
            for (const auto& target : honey_folders_)
            {
                const auto& tar_dir = target.first;
                try {
                    // Create tar_dir directory if it doesn't exist (do this once per tar_dir).
                    if (!fs::exists(tar_dir)) {
                        fs::create_directories(tar_dir);
                    }
                }
                catch (const std::exception& e) {
                    PrintDebugW(L"Failed to create target dir %ws: %hs", tar_dir.c_str(), e.what());
                    // Skip this tar_dir if we can't create it.
                    continue;
                }

                // Iterate all files in the source directory and copy into current tar_dir.
                for (const auto& entry : fs::directory_iterator(source_dir)) {
                    try {
                        if (!entry.is_regular_file()) continue;

                        std::wstring filename = entry.path().filename().wstring();
                        fs::path dest = fs::path(tar_dir) / filename;

                        // Copy honeypot file (overwrite if exists).
                        fs::copy_file(entry.path(), dest, fs::copy_options::skip_existing);
                        //fs::remove(dest);
                        //PrintDebugW(L"Copied %ws to %ws", filename.c_str(), tar_dir.c_str());
                    }
                    catch (const std::exception& e) {
                        PrintDebugW(L"Failed to copy %ws to %ws: %hs",
                            entry.path().filename().wstring().c_str(),
                            tar_dir.c_str(),
                            e.what());
                        // continue with next file
                    }
                }
            }
        }
        catch (const std::exception& e) {
            PrintDebugW(L"Error initializing honeypot: %hs", e.what());
            return false;
        }
        return true;
    }

    HoneyType HoneyPot::GetHoneyFolderType(const std::wstring& dir_path)
    {
        auto file_path_lower = KnownFolderChecker::NormalizePath(dir_path);
        auto it = honey_folders_.find(file_path_lower);
        if (it != honey_folders_.end())
        {
            return it->second;
        }
        return HoneyType::kNotHoney;
    }

    vector<pair<wstring, HoneyType>> HoneyPot::GetHoneyFolders()
    {
        vector<pair<wstring, HoneyType>> dirs;

        for (const auto& dir_info : honey_folders_)
        {
            dirs.push_back(dir_info);
        }
        return dirs;
    }

    vector<vector<string>> HoneyPot::GetHoneyTypes()
    {
        if (honey_types_.size() != 0)
        {
            return honey_types_;
        }
        for (const auto& entry : fs::directory_iterator(source_dir_)) {
            try {
                if (!entry.is_regular_file()) continue;
                std::wstring file_path = entry.path().wstring();
                DWORD status = 0;
                ull fize_size = 0;
                honey_types_.push_back(kFileType->GetTypes(file_path, &status, &fize_size)); // should be in config
            }
            catch (...) {

            }
        }

        return honey_types_;
    }

    vector<wstring> HoneyPot::GetHoneyNames()
    {
        if (honey_names_.size() != 0)
        {
            return honey_names_;
        }
        for (const auto& entry : fs::directory_iterator(source_dir_)) {
            try {
                if (!entry.is_regular_file()) continue;
                std::wstring name = ulti::ToLower(manager::GetFileNameNoExt(entry.path().filename()));
                DWORD status = 0;
                honey_names_.push_back(name);
            }
            catch (...) {

            }
        }

        return honey_names_;
    }

}  // namespace honeypot
