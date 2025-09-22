#ifndef MANAGER_CACHE_H_
#define MANAGER_CACHE_H_
#include <chrono>
#include <shared_mutex>
#include <string>
#include <unordered_map>

// Folder scan result verdict.
enum class Verdict {
    kClean,
    kRansomware,
    kNotInCache
};

// Cache entry data.
struct CacheEntry {
    Verdict verdict;
    std::chrono::steady_clock::time_point timestamp;
};

class FolderCache {
public:
    FolderCache();

    // Adds or updates a folder in the cache.
    void Add(const std::wstring& folder, Verdict verdict);

    // Gets a folder result from the cache.
    // Returns: {verdict, expired_flag}.
    std::pair<Verdict, bool> Get(const std::wstring& folder);

    // Removes all expired entries from the cache.
    void Cleanup();

    // Deletes a folder from the cache.
    // Returns true if removed successfully.
    bool Delete(const std::wstring& folder);

private:
    std::unordered_map<std::wstring, CacheEntry> cache_;
    mutable std::shared_mutex mutex_;  // Shared for read, exclusive for write.

    static constexpr std::chrono::minutes kCacheTtl{ 5 };  // TTL = 5 minutes.
};

#endif  // PROJECT_FOLDER_CACHE_H_
