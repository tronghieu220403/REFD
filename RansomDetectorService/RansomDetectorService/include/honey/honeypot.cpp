#include "honeypot.h"
#include "../ulti/debug.h"

#include <filesystem>
#include <iostream>
namespace honeypot {

    void HoneyPot::Init(const std::vector<std::wstring>& target_dirs,
        const std::wstring& source_dir) {
        namespace fs = std::filesystem;

        try {
            // Ensure source directory exists.
            if (!fs::exists(source_dir) || !fs::is_directory(source_dir)) {
                PrintDebugW(L"[HoneyPot] Source directory does not exist: %ws",
                    source_dir.c_str());
                return;
            }

            // Iterate all files in the source directory.
            for (const auto& entry : fs::directory_iterator(source_dir)) {
                if (!entry.is_regular_file()) continue;

                std::wstring filename = entry.path().filename().wstring();

                // Copy each file into all target directories.
                for (const auto& target : target_dirs) {
                    try {
                        // Create target directory if it doesn't exist.
                        if (!fs::exists(target)) {
                            fs::create_directories(target);
                        }

                        fs::path dest = fs::path(target) / filename;

                        // Copy honeypot file.
                        fs::copy_file(entry.path(), dest,
                            fs::copy_options::overwrite_existing);

                        PrintDebugW(L"[HoneyPot] Copied %ws to %ws",
                            filename.c_str(), target.c_str());
                    }
                    catch (const std::exception& e) {
                        PrintDebugW(L"[HoneyPot] Failed to copy to %ws: %hs",
                            target.c_str(), e.what());
                    }
                }
            }
        }
        catch (const std::exception& e) {
            PrintDebugW(L"[HoneyPot] Error initializing honeypot: %hs", e.what());
        }
    }

}  // namespace honeypot
