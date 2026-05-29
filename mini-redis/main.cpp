#include <iostream>
#include "store.h"

int main() {
    Store db;

    db.set("name", "alice");
    db.set("city", "delhi");
    db.set("session", "tok123", 3);  // expires in 3 seconds

    std::cout << db.get("name")    << "\n";   // alice
    std::cout << db.get("missing") << "\n";   // (nil)
    std::cout << db.size()         << "\n";   // 3

    db.del("city");
    std::cout << db.get("city")    << "\n";   // (nil)
    std::cout << db.size()         << "\n";   // 2

    return 0;
}