#ifndef HONEY_HONEYPOT_H_
#define HONEY_HONEYPOT_H_

#include "../ulti/include.h"

namespace honeypot
{
    // Folder scan result verdict.
    enum class HoneyType {
        kNotHoney,
        kHoneyBlendIn,
        kHoneyIsolated
    };

    // HoneyPot is responsible for deploying honeypot files
    // into target directories from a source directory.
    class HoneyPot
    {
    private:
        unordered_map<wstring, HoneyType> honey_folders_;
        vector<string> honey_types_;
        vector<wstring> honey_names_;
        wstring source_dir_;
    public:
        // Initializes honeypots in the given target directories.
        // - target_known_folders: list of directories where honeypots should be deployed.
        // - source_dir: directory containing the original honeypot files.
        bool Init(const std::vector<std::wstring>& target_known_folders,
            const std::wstring& source_dir);

        HoneyType GetHoneyFolderType(const std::wstring& file_path);
        vector<pair<wstring, HoneyType>> GetHoneyFolders();
        vector<string> GetHoneyTypes();
        vector<wstring> GetHoneyNames();
    };


}  // namespace honeypot

inline honeypot::HoneyPot hp;

#endif  // HONEY_HONEYPOT_H_
