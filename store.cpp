#include "store.h"
#include <chrono>

// In store.cpp — implement them:
void Store::setRaw(const std::string &key, const std::string &val, long long expire_at)
{
    std::lock_guard<std::mutex> lock(mtx);
    data[key] = {val, expire_at};
}

std::unordered_map<std::string, Entry> Store::getAll()
{
    std::lock_guard<std::mutex> lock(mtx);
    return data; // returns a copy — safe to use outside the lock
}

Store::Store(int max_keys) : max_keys_(max_keys) {}

// Current unix timestamp in seconds
long long Store::now()
{
    using namespace std::chrono;
    return duration_cast<seconds>(
               system_clock::now().time_since_epoch())
        .count();
}

// Move key to front of LRU list — called on every access
void Store::touch(const std::string &key)
{
    auto it = data.find(key);
    if (it == data.end())
        return;
    lru_list.erase(it->second.lru_it);    // remove from current position
    lru_list.push_front(key);             // move to front
    it->second.lru_it = lru_list.begin(); // update the iterator
}

// Remove the least recently used key (back of list)
void Store::evict()
{
    if (lru_list.empty())
        return;
    std::string lru_key = lru_list.back(); // least recently used
    lru_list.pop_back();
    data.erase(lru_key);
}

// SET key value [ttl]
void Store::set(const std::string &key, const std::string &val, int ttl)
{
    std::lock_guard<std::mutex> lock(mtx);
    // If key already exists, remove from LRU list first
    auto it = data.find(key);
    if (it != data.end()) {
        lru_list.erase(it->second.lru_it);
        data.erase(it);
    }

    // Evict if at capacity
    if ((int)data.size() >= max_keys_) evict();

    // Insert new entry at front of LRU list
    lru_list.push_front(key);
    long long exp = (ttl > 0) ? now() + ttl : -1;
    data[key] = {val, exp, lru_list.begin()};
}

// GET key → value string, or "(nil)"
std::string Store::get(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) return "(nil)";

    Entry& e = it->second;
    if (e.expire_at != -1 && now() > e.expire_at) {
        lru_list.erase(e.lru_it);
        data.erase(it);
        return "(nil)";
    }

    touch(key);   // ← accessing a key keeps it alive
    return e.value;
}

// DELETE key → 1 if deleted, 0 if not found
int Store::del(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) return 0;
    lru_list.erase(it->second.lru_it);
    data.erase(it);
    return 1;
}

// How many keys are currently stored
int Store::size()
{
    std::lock_guard<std::mutex> lock(mtx);
    return data.size();
}