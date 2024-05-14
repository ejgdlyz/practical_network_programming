#include <iostream>
#include <thread>
#include <vector>

#include "sylar/socket.h"
#include "util/util.h"

// netcat implementation base on thread-per-connection

void run(sylar::Socket::ptr sock) {
    // caution: a bad example for closing connection
    std::thread thr([&sock](){  // 网络线程 start: 网络线程不断地 socket 读，并将读到的数据发送到 stdout
        char buf[8192];
        int nr = 0;
        while ( (nr = sock->recv(buf, sizeof(buf))) > 0) {
            int nw = Util::WriteFixedBytes(STDOUT_FILENO, buf, nr);
            if (nw < nr) {
                break;
            }
        }
        ::exit(0);  // should somehow notify main thread instead.
    }); // 网络线程 end

    // 主线程：不断地从 stdin 读 8K 缓冲区，并将读到的数据发送到 socket
    char buf[8192];
    int nr = 0;
    while ( (nr = ::read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        int nw = Util::WriteFixedBytes(sock, buf, nr);
        if (nw < nr) {
            break;
        }
    }
    sock->shutdownWrite();
    thr.join();
}

void run_grace(sylar::Socket::ptr sock) {
    // use select() to exit gracefully

    int pipefds[2];
    int rt = pipe(pipefds);
    assert(!rt);

    std::thread thr([&sock, &pipefds](){  // 网络线程 start: 网络线程不断地 socket 读，并将读到的数据发送到 stdout
        char buf[8192];
        int nr = 0;
        while ( (nr = sock->recv(buf, sizeof(buf))) > 0) {
            int nw = Util::WriteFixedBytes(STDOUT_FILENO, buf, nr);
            if (nw < nr) {
                break;
            }
        }
        int rt = ::write(pipefds[1], "t", 1);
        assert(rt == 1);
        sock->shutdownRead();
    }); // 网络线程 end

    // 主线程：不断地从 stdin 读 8K 缓冲区，并将读到的数据发送到 socket
    char buf[8192];
    int nr = 0;
    fd_set readset;
    int result = 0;
    do {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(pipefds[0], &readset);

        result = select(std::max(STDIN_FILENO, pipefds[0]) + 1, &readset, nullptr, nullptr, nullptr);
        if (result > 0) {
            if (FD_ISSET(STDIN_FILENO, &readset)) {
                nr = ::read(STDIN_FILENO, buf, sizeof(buf));
                if (nr > 0) {
                    int nw = Util::WriteFixedBytes(sock, buf, nr);
                    if (nw < nr) {
                        break;
                    }
                } else {  // 读 stdin 结束 
                    break;
                }
            }

            if (FD_ISSET(pipefds[0], &readset)) {
                // 套接字已关闭，sock->recv() 得到 0
                nr = sock->recv(buf, sizeof(buf));
                if (nr <= 0) {
                    break;  // 套接字关闭
                }
            }
        }
    } while (result != -1);
    
    sock->shutdownWrite();
        
    thr.join();
    close(pipefds[0]);
    close(pipefds[1]);
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage:" << std::endl 
                << "  " << argv[0] << " hostname port" << std::endl
                << "  " << argv[0] << " -l port" << std::endl; 
        return 0;
    }

    int port = atoi(argv[2]);
    if (strcmp(argv[1], "-l") == 0) {       // as server
        std::string host = "localhost:" + std::to_string(port);
        auto addr = sylar::Address::LookupAny(host);
        if (addr) {
            auto listen_sock = sylar::Socket::CreateTCPSocket();
            listen_sock->bind(addr);
            listen_sock->listen();
            auto sock = listen_sock->accept();
            if (sock) {
                listen_sock.reset();    // stop listening
                // run(sock);              // unique_ptr more suitable
                run_grace(sock);
            } else {
                perror("accept");
            }
        } else {
            std::cout << "Unable reslove " << host << std::endl;
            return 0;
        }
    } else {        // as client
        std::string hostname = std::string(argv[1]) + ":" + std::to_string(port);
        auto addr = sylar::Address::LookupAny(hostname);
        if (addr) {
            auto client_sock = sylar::Socket::CreateTCPSocket();
            client_sock->connect(addr);
            if (client_sock) {
                // run(client_sock);
                run_grace(client_sock);
            } else {
                std::cout << "Unable to connect " << *addr << std::endl;
            }
        } else {
            std::cout << "Unable reslove " << hostname << std::endl;
        }
    }
    return 0;
}
