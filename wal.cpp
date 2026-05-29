#include "wal.h"
#include "store.h"
#include <fstream>
#include <sstream>

WAL::WAL(const std::string& path) : path_(path) {
    out_.open(path, std::ios::app);   // append mode — never overwrites
}

WAL::~WAL() {
    if (out_.is_open()) out_.close();
}

void WAL::log(const std::string& line) {
    if (!out_.is_open()) return;
    out_ << line << "\n";
    write_count_++;
    if (write_count_ % 100 == 0) {   // flush every 100 writes
        out_.flush();
        write_count_ = 0;
    }
}

void WAL::replay(Store& db) {
    std::ifstream in(path_);
    if (!in.is_open()) return;   // no WAL file yet — first startup

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string op, key, val;
        int ttl = -1;
        ss >> op >> key;
        if (op == "SET") {
            ss >> val >> ttl;
            db.set(key, val, ttl);
        } else if (op == "DEL") {
            db.del(key);
        }
    }
}

void WAL::clear() {
    if (out_.is_open()) out_.close();
    out_.open(path_, std::ios::trunc);   // trunc = wipe the file
}