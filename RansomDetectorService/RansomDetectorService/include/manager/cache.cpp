#include "cache.h"

FolderCache::FolderCache() = default;

void FolderCache::Add(const std::wstring& folder, Verdict verdict) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Exclusive lock.
    cache_[folder] = { verdict, std::chrono::steady_clock::now() };
}

std::pair<Verdict, bool> FolderCache::Get(const std::wstring& folder) {
    std::shared_lock<std::shared_mutex> lock(mutex_);  // Shared lock.
    auto it = cache_.find(folder);
    if (it == cache_.end()) {
        return { Verdict::kNotInCache, false };
    }

    auto now = std::chrono::steady_clock::now();
    bool expired = (now - it->second.timestamp > kCacheTtl);

    return { it->second.verdict, expired };
}

void FolderCache::Cleanup() {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Exclusive lock.
    auto now = std::chrono::steady_clock::now();

    for (auto it = cache_.begin(); it != cache_.end();) {
        if (now - it->second.timestamp > kCacheTtl) {
            it = cache_.erase(it);
        }
        else {
            ++it;
        }
    }
}

bool FolderCache::Delete(const std::wstring& folder) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Exclusive lock.
    return cache_.erase(folder) > 0;
}
