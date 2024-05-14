#include <iostream>
#include <thread>
#include <string.h>
#include <time.h>
#include <atomic>

#include "sylar/socket.h"
#include "util/util.h"

// a thread-per-connection current chargen server and client

std::atomic<int64_t> g_bytes;

std::string GetMessage() {
    std::string line;
    for (int i = 33; i < 127; ++i) {
        line.push_back(char(i));
    }
    line += line;
    
    std::string message;
    for (size_t i = 0; i < 127 - 33; ++i) {
        message += line.substr(i, 72) + '\n';
    }
    return message;
}

void measure() {
    int64_t start = Util::GetNow();
    while (true) {
        struct timespec ts = {1, 0};   // s, ns
        ::nanosleep(&ts, NULL);
        
        int64_t bytes = g_bytes.exchange(0);
        int64_t end = Util::GetNow();
        double eplased = (end - start) / 1000 / 1000;
        start = end;
        if (bytes) {
            printf("%.3f MiB/s\n", bytes / (1024 * 1024.0) / eplased);
        }
    }
}

void chargen(sylar::Socket::ptr sock) {
    std::string message = GetMessage();
    while (true) {
        int nw = Util::WriteFixedBytes(sock, message.c_str(), message.size());
        g_bytes.fetch_add(nw);
        if (nw < static_cast<int>(message.size())) {
            break;
        }
    }
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage:\n"
                << "  " << argv[0] << " hostname port\n"        // client
                << "  " << argv[0] << " -l port" << std::endl;  // server
        std::string message = GetMessage();
        std::cout << "message size = " << message.size() << std::endl;
        return 0;
    } 

    std::thread(measure).detach();

    int port = atoi(argv[2]);
    if (strcmp(argv[1], "-l") == 0) {       // as server
        std::string hostname = "localhost:" + std::to_string(port);
        auto addr = sylar::Address::LookupAny(hostname);
        if (!addr) {
            std::cout << "Unable to resolve " << hostname << std::endl;
            return 0;
        }
        auto serv_sock = sylar::Socket::CreateTCPSocket();
        serv_sock->bind(addr);
        serv_sock->listen();
        std::cout << "Accepting... Ctrl-C to exit" << std::endl;
        int cnt = 0;
        while (true) {
            auto client_sock = serv_sock->accept();
            std::cout << "accepted no. " << ++cnt << " client" << std::endl;

            std::thread thr(chargen, client_sock);
            thr.detach();
        }
    } else {        // as client
        std::string serv_host = std::string(argv[1]) + ":" + std::to_string(port);
        auto serv_addr = sylar::Address::LookupAny(serv_host);
        if (!serv_addr) {
            std::cout << "Unable to resolve " << serv_host << std::endl;
            return 0;
        }
        auto sock = sylar::Socket::CreateTCPSocket();
        bool rt = sock->connect(serv_addr);
        if (rt) {
            chargen(sock);
        } else {
            std::cout << "Unable to connect " << *serv_addr << std::endl;
            perror("");
        }
    }
    return 0;
}
