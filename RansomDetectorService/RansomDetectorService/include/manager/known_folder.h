#ifndef KNOWN_FOLDER_H_
#define KNOWN_FOLDER_H_

#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include "../ulti/debug.h"

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <roapi.h>
#include <wrl/client.h>
#include <windows.data.json.h>

#include <string>
#include <vector>
#include <unordered_map>

struct GuidHash {
    size_t operator()(const GUID& g) const noexcept {
        const uint64_t* p = reinterpret_cast<const uint64_t*>(&g);
        return std::hash<uint64_t>()(p[0]) ^ std::hash<uint64_t>()(p[1]);
    }
};

struct GuidEqual {
    bool operator()(const GUID& a, const GUID& b) const noexcept {
        return InlineIsEqualGUID(a, b);
    }
};

struct FolderInfo {
    KNOWNFOLDERID fid{};
    KNOWNFOLDERID parent{};
    std::wstring relative;
    std::wstring fullpath;
    std::wstring name;
    KF_CATEGORY cat{};
    _KF_DEFINITION_FLAGS flags{};
    bool has_path = false;
};

struct FolderRule {
    std::wstring prefix;
    std::wstring suffix_after_user;
    bool any_user = false;
};

class KnownFolderChecker {
public:
    KnownFolderChecker() = default;
    explicit KnownFolderChecker(const std::wstring& config_file);

    void Init(const std::wstring& config_file);
    bool IsPathInKnownFolders(const std::wstring& file_path) const;

private:
    std::vector<FolderRule> rules_;

    static std::wstring NormalizePath(const std::wstring& path);
    static bool IsBuiltInUser(const std::wstring& user);
    static FolderRule PathToRule(const std::wstring& fullpath);
    static std::vector<GUID> LoadConfig(const std::wstring& file);

    static std::wstring ResolveFullPath(
        std::unordered_map<GUID, FolderInfo, GuidHash, GuidEqual>& table,
        const GUID& fid,
        std::unordered_map<GUID, bool, GuidHash>& visiting);
};

inline KnownFolderChecker kf_checker;

#endif  // KNOWN_FOLDER_H_
