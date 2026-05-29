#pragma once
#include <string>
#include <fstream>

class Store; // forward declaration

class WAL
{
public:
    WAL(const std::string &path);
    ~WAL();
    void log(const std::string &line); // append one command
    void replay(Store &db);            // called on startup
    void clear();                      // called after snapshot

private:
    std::string path_;
    std::ofstream out_;
    int write_count_ = 0;
};