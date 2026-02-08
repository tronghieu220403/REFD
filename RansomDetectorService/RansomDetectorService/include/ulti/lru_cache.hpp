#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <unordered_map>
#include <list>
#include <utility>
#include <cstddef>

/*
 * NOTE:
 * This header is intentionally header-only.
 * Template implementations must be visible at the point of instantiation
 * to avoid unresolved external symbol linker errors.
 */

 // ================================================================
 // LRUMap<K, V> declaration
 // ================================================================

template<typename K, typename V>
class LruMap {
public:
    LruMap();
    explicit LruMap(size_t capacity);

    void set_capacity(size_t capacity);

    bool contains(const K& key);
    bool get(const K& key, V& outValue);
    void put(const K& key, const V& value);
    void erase(const K& key);

    size_t size() const;
    void clear();

private:
    size_t m_capacity;
    std::list<std::pair<K, V>> m_list;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> m_map;
};

// ================================================================
// LruSet<K> declaration
// ================================================================

template<typename K>
class LruSet {
public:
    LruSet();
    explicit LruSet(size_t capacity);

    void set_capacity(size_t capacity);

    bool contains(const K& key);
    void insert(const K& key);
    void erase(const K& key);

    size_t size() const;
    void clear();

private:
    size_t m_capacity;
    std::list<K> m_list;
    std::unordered_map<K, typename std::list<K>::iterator> m_map;
};

// ================================================================
// LRUMap<K, V> inline implementation
// ================================================================

template<typename K, typename V>
inline LruMap<K, V>::LruMap()
    : m_capacity(0) {
}

template<typename K, typename V>
inline LruMap<K, V>::LruMap(size_t capacity)
    : m_capacity(capacity) {
}

template<typename K, typename V>
inline void LruMap<K, V>::set_capacity(size_t capacity)
{
    m_capacity = capacity;

    if (m_capacity == 0) {
        clear();
        return;
    }

    // Evict LRU until size <= capacity
    while (m_list.size() > m_capacity) {
        auto& lruNode = m_list.back();
        m_map.erase(lruNode.first);
        m_list.pop_back();
    }
}

template<typename K, typename V>
inline bool LruMap<K, V>::contains(const K& key)
{
    auto it = m_map.find(key);
    if (it == m_map.end())
        return false;

    // Promote accessed entry to most-recently-used (MRU)
    m_list.splice(m_list.begin(), m_list, it->second);
    return true;
}

template<typename K, typename V>
inline bool LruMap<K, V>::get(const K& key, V& outValue)
{
    auto it = m_map.find(key);
    if (it == m_map.end())
        return false;

    // Promote to most-recently-used (MRU) (Move to front)
    m_list.splice(m_list.begin(), m_list, it->second);
    outValue = it->second->second;
    return true;
}

template<typename K, typename V>
inline void LruMap<K, V>::put(const K& key, const V& value)
{
    auto it = m_map.find(key);

    if (it != m_map.end()) {
        // Update existing entry and promote to MRU
        it->second->second = value;
        m_list.splice(m_list.begin(), m_list, it->second);
        return;
    }

    if (m_capacity == 0)
        return;

    // Evict least-recently-used (LRU) entry
    if (m_list.size() >= m_capacity) {
        auto& lruNode = m_list.back();
        m_map.erase(lruNode.first);
        m_list.pop_back();
    }

    // Insert new entry as MRU
    m_list.emplace_front(key, value);
    m_map[key] = m_list.begin();
}

template<typename K, typename V>
inline void LruMap<K, V>::erase(const K& key)
{
    auto it = m_map.find(key);
    if (it == m_map.end())
        return;

    m_list.erase(it->second);
    m_map.erase(it);
}

template<typename K, typename V>
inline size_t LruMap<K, V>::size() const
{
    return m_map.size();
}

template<typename K, typename V>
inline void LruMap<K, V>::clear()
{
    m_map.clear();
    m_list.clear();
}

// ================================================================
// LruSet<K> inline implementation
// ================================================================

template<typename K>
inline LruSet<K>::LruSet()
    : m_capacity(0) {
}

template<typename K>
inline LruSet<K>::LruSet(size_t capacity)
    : m_capacity(capacity) {
}

template<typename K>
inline void LruSet<K>::set_capacity(size_t capacity)
{
    m_capacity = capacity;

    if (m_capacity == 0) {
        clear();
        return;
    }

    // Evict LRU until size <= capacity
    while (m_list.size() > m_capacity) {
        const K& lruKey = m_list.back();
        m_map.erase(lruKey);
        m_list.pop_back();
    }
}

template<typename K>
inline bool LruSet<K>::contains(const K& key)
{
    auto it = m_map.find(key);
    if (it == m_map.end())
        return false;

    // Promote accessed key to most recently used (MRU)
    m_list.splice(m_list.begin(), m_list, it->second);
    return true;
}

template<typename K>
inline void LruSet<K>::insert(const K& key)
{
    auto it = m_map.find(key);

    if (it != m_map.end()) {
        // Already exists -> promote to MRU
        m_list.splice(m_list.begin(), m_list, it->second);
        return;
    }

    if (m_capacity == 0)
        return;

    // Evict least recently used key if capacity is reached
    if (m_list.size() >= m_capacity) {
        const K& lruKey = m_list.back();
        m_map.erase(lruKey);
        m_list.pop_back();
    }

    // Insert new key as MRU
    m_list.emplace_front(key);
    m_map[key] = m_list.begin();
}

template<typename K>
inline void LruSet<K>::erase(const K& key)
{
    auto it = m_map.find(key);
    if (it == m_map.end())
        return;

    m_list.erase(it->second);
    m_map.erase(it);
}

template<typename K>
inline size_t LruSet<K>::size() const
{
    return m_map.size();
}

template<typename K>
inline void LruSet<K>::clear()
{
    m_map.clear();
    m_list.clear();
}

#endif // LRU_CACHE_H
