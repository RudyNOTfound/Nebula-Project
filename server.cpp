#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "store.h"
#include <thread>
#include "wal.h"
#include "snapshot.h"

#define PORT 6379

// Split "SET name alice" into ["SET", "name", "alice"]
std::vector<std::string> splitCmd(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string w;
    while (ss >> w)
        out.push_back(w);
    return out;
}

// Run one command, return the response string
std::string handleCommand(Store &db, WAL &wal, const std::string &raw)
{
    auto p = splitCmd(raw);
    if (p.empty())
        return "";

    std::string cmd = p[0];
    for (auto &c : cmd)
        c = toupper(c); // case-insensitive

    if (cmd == "PING")
        return "+PONG\r\n";

    if (cmd == "SET")
    {
        if (p.size() < 3)
            return "-ERR wrong args\r\n";
        int ttl = -1;
        if (p.size() >= 5)
        {
            std::string ex = p[3];
            for (auto &c : ex)
                c = toupper(c);
            if (ex == "EX")
                ttl = std::stoi(p[4]);
        }
        db.set(p[1], p[2], ttl);
        wal.log("SET " + p[1] + " " + p[2] + (ttl > 0 ? " " + std::to_string(ttl) : ""));
        return "+OK\r\n";
    }

    if (cmd == "GET")
    {
        if (p.size() < 2)
            return "-ERR wrong args\r\n";
        std::string val = db.get(p[1]);
        if (val == "(nil)")
            return "$-1\r\n"; // nil bulk string
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    }

    if (cmd == "DEL")
    {
        if (p.size() < 2)
            return "-ERR wrong args\r\n";
        int n = db.del(p[1]);
        if (n > 0)
            wal.log("DEL " + p[1]);
        return ":" + std::to_string(db.del(p[1])) + "\r\n";
    }

    if (cmd == "DBSIZE")
        return ":" + std::to_string(db.size()) + "\r\n";

    return "-ERR unknown command '" + p[0] + "'\r\n";
}

// Handle one connected client until they disconnect
void handleClient(int client_fd, Store &db, WAL &wal)
{
    char buf[1024];
    std::string leftover; // holds incomplete data between recv() calls

    while (true)
    {
        int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            break; // 0 = clean disconnect, -1 = error
        buf[n] = '\0';
        leftover += buf;

        // Process every complete line (commands end with \n)
        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos)
        {
            std::string line = leftover.substr(0, pos);
            leftover = leftover.substr(pos + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;

            std::string resp = handleCommand(db, wal, line); // add wal here
            if (!resp.empty())
                send(client_fd, resp.c_str(), resp.size(), 0);
        }
    }
}

int main()
{
    Store db(100000); // ← evict LRU keys when store exceeds 100k keys
    WAL wal("redis.wal");

    // On startup: load snapshot first, then replay WAL on top
    loadSnapshot(db, "redis.snap");
    wal.replay(db);

    std::cout << "Loaded " << db.size() << " keys from disk.\n";

    // Background thread: snapshot every 60 seconds
    // std::thread([&db, &wal]()
    //             {
    //     while (true) {
    //         std::this_thread::sleep_for(std::chrono::seconds(60));
    //         saveSnapshot(db, "redis.snap");
    //         wal.clear();
    //         std::cout << "Snapshot saved.\n";
    //     } })
    //     .detach();

    // Step 1: Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    // Step 2: Bind to port 6379
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    // Step 3: Start listening
    listen(server_fd, 10);
    std::cout << "Mini Redis listening on port " << PORT << "...\n";

    // Step 4 + 5: Accept clients and handle their commands
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
            continue;

        // Spawn a new thread for this client — it runs handleClient independently
        std::thread([client_fd, &db, &wal]()
                    {
    handleClient(client_fd, db, wal);
    close(client_fd); })
            .detach();
    }

    close(server_fd);
    return 0;
}