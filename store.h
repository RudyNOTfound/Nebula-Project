#pragma once
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>

// One value stored in our database
struct Entry
{
    std::string value;
    long long expire_at;                     // unix seconds, -1 = never expires
    std::list<std::string>::iterator lru_it; // ← points to position in LRU list
};

struct SnapEntry {
    std::string value;
    long long   expire_at;
};

// The store — one class that wraps everything
class Store
{
public:
    Store(int max_keys = 1000); // ← max before eviction kicks in

    void set(const std::string &key, const std::string &val, int ttl = -1);
    std::string get(const std::string &key);
    int del(const std::string &key);
    int size();
    // In store.h — add these two to the public section:
    void setRaw(const std::string &key, const std::string &val, long long expire_at);
    std::unordered_map<std::string, SnapEntry> getAll();

private:
    std::unordered_map<std::string, Entry> data; // THE hashmap
    std::list<std::string> lru_list;             // front = MRU, back = LRU
    int max_keys_;
    std::mutex mtx;
    long long now();
    void touch(const std::string &key); // move to front
    void evict();                       // remove LRU key
};