#include "known_folder.h"

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "runtimeobject.lib")

#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

KnownFolderChecker::KnownFolderChecker(const std::wstring& config_file) {
    Init(config_file);
}

std::wstring KnownFolderChecker::NormalizePath(const std::wstring& path) {
    std::wstring p = path;
    for (auto& c : p) {
        if (c == L'/') c = L'\\';
        else c = towlower(c);
    }
    return p;
}

bool KnownFolderChecker::IsBuiltInUser(const std::wstring& user) {
    static const std::vector<std::wstring> built_in = {
        L"public", L"default", L"all users", L"default user" };
    for (auto& u : built_in) {
        if (_wcsicmp(user.c_str(), u.c_str()) == 0) return true;
    }
    return false;
}

FolderRule KnownFolderChecker::PathToRule(const std::wstring& fullpath) {
    FolderRule rule{};
    std::wstring norm = NormalizePath(fullpath);

    const std::wstring users_prefix = L"c:\\users\\";
    if (norm.rfind(users_prefix, 0) == 0) {
        size_t pos_after_user = norm.find(L'\\', users_prefix.size());
        std::wstring user_name;
        if (pos_after_user != std::wstring::npos) {
            user_name = norm.substr(users_prefix.size(),
                pos_after_user - users_prefix.size());
        }
        else {
            user_name = norm.substr(users_prefix.size());
        }

        if (!IsBuiltInUser(user_name)) {
            rule.any_user = true;
            rule.prefix = users_prefix;
            if (pos_after_user != std::wstring::npos) {
                rule.suffix_after_user = norm.substr(pos_after_user);
            }
            return rule;
        }
    }

    rule.prefix = norm;
    return rule;
}

std::vector<GUID> KnownFolderChecker::LoadConfig(const std::wstring& file) {
    std::wifstream ifs(file);
    if (!ifs.is_open()) throw std::runtime_error("Cannot open config");

    std::wstringstream buffer;
    buffer << ifs.rdbuf();
    std::wstring json_text = buffer.str();

    std::vector<GUID> guids;

    HSTRING h_class;
    WindowsCreateString(RuntimeClass_Windows_Data_Json_JsonObject,
        (UINT32)wcslen(RuntimeClass_Windows_Data_Json_JsonObject),
        &h_class);

    ComPtr<ABI::Windows::Data::Json::IJsonObjectStatics> json_statics;
    if (h_class != nullptr &&
        RoGetActivationFactory(h_class, IID_PPV_ARGS(&json_statics)) != S_OK) {
        return guids;
    }
    WindowsDeleteString(h_class);

    HSTRING h_json;
    WindowsCreateString(json_text.c_str(),
        static_cast<UINT32>(json_text.size()), &h_json);

    ComPtr<ABI::Windows::Data::Json::IJsonObject> json_obj;
    json_statics->Parse(h_json, &json_obj);
    WindowsDeleteString(h_json);

    HSTRING h_key;
    WindowsCreateString(L"folders", 7, &h_key);
    ComPtr<ABI::Windows::Data::Json::IJsonArray> arr;
    json_obj->GetNamedArray(h_key, &arr);
    WindowsDeleteString(h_key);

    ComPtr<ABI::Windows::Foundation::Collections::IVector<
        ABI::Windows::Data::Json::IJsonValue*>>
        vec;
    arr.As(&vec);

    unsigned int size = 0;
    vec->get_Size(&size);

    for (unsigned int i = 0; i < size; i++) {
        ABI::Windows::Data::Json::IJsonValue* raw_val = nullptr;
        vec->GetAt(i, &raw_val);
        ComPtr<ABI::Windows::Data::Json::IJsonValue> val;
        val.Attach(raw_val);

        HSTRING h_str;
        val->GetString(&h_str);

        std::wstring guid_str(WindowsGetStringRawBuffer(h_str, nullptr));
        GUID gid;
        if (SUCCEEDED(CLSIDFromString(guid_str.c_str(), &gid))) {
            guids.push_back(gid);
        }
        WindowsDeleteString(h_str);
    }

    return guids;
}

std::wstring KnownFolderChecker::ResolveFullPath(
    std::unordered_map<GUID, FolderInfo, GuidHash, GuidEqual>& table,
    const GUID& fid,
    std::unordered_map<GUID, bool, GuidHash>& visiting) {
    auto it = table.find(fid);
    if (it == table.end()) return L"";

    FolderInfo* node = &it->second;
    if (!node->fullpath.empty()) return node->fullpath;

    if (visiting[fid]) return L"";
    visiting[fid] = true;

    std::vector<FolderInfo*> chain;
    chain.push_back(node);

    while (true) {
        auto pit = table.find(chain.back()->parent);
        if (pit == table.end()) break;
        FolderInfo* parent = &pit->second;
        if (!parent->fullpath.empty() || parent->has_path) {
            chain.push_back(parent);
            break;
        }
        if (visiting[parent->fid]) break;
        visiting[parent->fid] = true;
        chain.push_back(parent);
    }

    std::wstring cur_path;
    if (!chain.back()->fullpath.empty()) {
        cur_path = chain.back()->fullpath;
    }
    else if (chain.back()->has_path) {
        cur_path = chain.back()->fullpath;
    }
    else {
        cur_path = chain.back()->relative;
    }

    for (int i = static_cast<int>(chain.size()) - 2; i >= 0; --i) {
        FolderInfo* n = chain[i];
        if (!n->relative.empty()) cur_path = cur_path + L"\\" + n->relative;
        n->fullpath = cur_path;
    }

    visiting[fid] = false;
    return node->fullpath;
}

void KnownFolderChecker::Init(const std::wstring& config_file) {
    rules_.clear();
    if (FAILED(CoInitialize(nullptr))) {
        throw std::runtime_error("CoInitialize failed");
    }

    auto target_guids = LoadConfig(config_file);

    IKnownFolderManager* kf_manager = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_KnownFolderManager, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&kf_manager));
    if (FAILED(hr)) {
        throw std::runtime_error("CoCreateInstance failed");
    }

    KNOWNFOLDERID* all_folders = nullptr;
    UINT count = 0;
    hr = kf_manager->GetFolderIds(&all_folders, &count);
    if (FAILED(hr)) {
        throw std::runtime_error("GetFolderIds failed");
    }

    std::unordered_map<GUID, FolderInfo, GuidHash, GuidEqual> table;

    for (UINT i = 0; i < count; i++) {
        IKnownFolder* folder = nullptr;
        if (SUCCEEDED(kf_manager->GetFolder(all_folders[i], &folder))) {
            KNOWNFOLDER_DEFINITION kfd{};
            if (SUCCEEDED(folder->GetFolderDefinition(&kfd))) {
                FolderInfo fi{};
                fi.fid = all_folders[i];
                fi.parent = kfd.fidParent;
                if (kfd.pszRelativePath) fi.relative = kfd.pszRelativePath;
                if (kfd.pszName) fi.name = kfd.pszName;
                fi.cat = static_cast<KF_CATEGORY>(kfd.category);
                fi.flags = static_cast<_KF_DEFINITION_FLAGS>(kfd.kfdFlags);

                PWSTR path = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(all_folders[i], 0, nullptr, &path))) {
                    fi.fullpath = path;
                    fi.has_path = true;
                    CoTaskMemFree(path);
                }
                table[fi.fid] = fi;
                FreeKnownFolderDefinitionFields(&kfd);
            }
            folder->Release();
        }
    }

    for (auto& gid : target_guids) {
        std::unordered_map<GUID, bool, GuidHash> visiting;
        std::wstring path = ResolveFullPath(table, gid, visiting);
        CharLowerBuffW(path.data(), path.size());
        if (auto p = path.find(L"c:\\windows\\system32\\config\\systemprofile\\"); p != std::string::npos) path.replace(p, 41, L"c:\\users\\hieunt\\");
        if (!path.empty()) {
            rules_.push_back(PathToRule(path));
        }
    }

    CoTaskMemFree(all_folders);
    kf_manager->Release();
}

bool KnownFolderChecker::IsPathInKnownFolders(
    const std::wstring& file_path) const {
    std::wstring norm = NormalizePath(file_path);

    for (auto& r : rules_) {
        if (r.any_user) {
            const std::wstring users_prefix = L"c:\\users\\";
            if (norm.rfind(users_prefix, 0) != 0) continue;

            size_t pos_after_user = norm.find(L'\\', users_prefix.size());
            if (pos_after_user == std::wstring::npos) continue;

            std::wstring suffix = norm.substr(pos_after_user);
            if (!r.suffix_after_user.empty() &&
                suffix.rfind(r.suffix_after_user, 0) == 0) {
                size_t expected_len = users_prefix.size() +
                    (pos_after_user - users_prefix.size()) +
                    r.suffix_after_user.size();
                if (norm.size() == expected_len ||
                    (norm.size() > expected_len && norm[expected_len] == L'\\' &&
                        norm.find(L'\\', expected_len + 1) == std::wstring::npos)) {
                    return true;
                }
            }
        }
        else {
            if (norm.rfind(r.prefix, 0) == 0) {
                size_t expected_len = r.prefix.size();
                if (norm.size() == expected_len ||
                    (norm.size() > expected_len && norm[expected_len] == L'\\' &&
                        norm.find(L'\\', expected_len + 1) == std::wstring::npos)) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::vector<std::wstring> KnownFolderChecker::GetKnownFolders()
{
    std::vector<std::wstring> result;

    // List of user folders to skip
    std::vector<std::wstring> skip_users = {
        L"public", L"default", L"all users", L"default user"
    };

    for (const auto& rule : rules_) {
        if (!rule.any_user) {
            // Case: rule does not require user expansion
            // Just add the prefix as a folder
            result.push_back(rule.prefix);
        }
        else {
            // Case: rule requires expanding all users in C:\Users
            std::wstring users_root = L"c:\\users";
            for (const auto& entry : std::filesystem::directory_iterator(users_root)) {
                if (!entry.is_directory()) continue;

                // Extract the user folder name
                std::wstring username = entry.path().filename().wstring();

                // Convert to lowercase for comparison
                std::wstring lower = username;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

                // Skip special system users
                if (std::find(skip_users.begin(), skip_users.end(), lower) != skip_users.end()) {
                    continue;
                }

                // Build the full folder path: prefix + username + suffix
                std::wstring full = rule.prefix + username + rule.suffix_after_user;
                result.push_back(full);
            }
        }
    }

    return result;
}
