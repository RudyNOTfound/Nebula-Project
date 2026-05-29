#include "snapshot.h"
#include <fstream>
#include <sstream>

void saveSnapshot(Store& db, const std::string& path) {
    auto entries = db.getAll();
    std::ofstream out(path, std::ios::trunc);
    for (auto& [key, entry] : entries) {
        // Format: key \t value \t expire_at
        out << key << "\t" << entry.value << "\t" << entry.expire_at << "\n";
    }
    out.flush();
}

void loadSnapshot(Store& db, const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string key, val;
        long long exp;
        std::getline(ss, key, '\t');
        std::getline(ss, val, '\t');
        ss >> exp;
        db.setRaw(key, val, exp);   // bypasses TTL calculation
    }
}