#include <iostream>
#include <string>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int connect_to(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(fd, (sockaddr*)&addr, sizeof(addr));

    
    return fd;
}

double bench(int fd, const std::string& cmd, int n) {
    char buf[256];
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        send(fd, cmd.c_str(), cmd.size(), 0);
        recv(fd, buf, sizeof(buf), 0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

int main() {
    const int N = 100000;   // 100k operations per test
    int fd = connect_to(6379);

    std::cout << "Benchmarking mini-redis (" << N << " ops each)\n\n";

    // Warm up
    bench(fd, "PING\n", 1000);

    // SET benchmark
    double set_ms = bench(fd, "SET benchkey benchvalue\n", N);
    double set_rps = N / (set_ms / 1000.0);
    std::cout << "SET: " << (int)set_rps << " req/sec"
              << "  (" << set_ms / N << "ms avg)\n";

    // GET benchmark
    double get_ms = bench(fd, "GET benchkey\n", N);
    double get_rps = N / (get_ms / 1000.0);
    std::cout << "GET: " << (int)get_rps << " req/sec"
              << "  (" << get_ms / N << "ms avg)\n";

    // PING benchmark (baseline)
    double ping_ms = bench(fd, "PING\n", N);
    double ping_rps = N / (ping_ms / 1000.0);
    std::cout << "PING: " << (int)ping_rps << " req/sec"
              << "  (" << ping_ms / N << "ms avg)\n";

    close(fd);
    return 0;
}