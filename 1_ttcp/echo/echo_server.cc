#include <iostream>
#include <functional>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "util/util.h"

// a thread-per-connection current echo sever;
int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cout << "please input <ip> <port>" << std::endl;
        exit(1);
    }
    
    const char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int rt = bind(listenfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr));
    assert(rt == 0);

    rt = listen(listenfd, 5);
    assert(rt == 0);

    std::cout << "Accept... Ctrl-C to exit" << std::endl;
    int cnt = 0;
    while (true) {
        struct sockaddr_in peer_addr;
        socklen_t addr_len;
        bzero(&peer_addr, sizeof(peer_addr));
        int connfd = accept(listenfd, reinterpret_cast<struct sockaddr*>(&peer_addr), &addr_len);
        std::cout << "accepted no. " << ++cnt << " client" << std::endl;

        std::thread thr([cnt, connfd](){
            std::cout << "thread for no. " << cnt << " client started." << std::endl;
            char buf[4096];
            int nr = 0;
            while ((nr = read(connfd, buf, sizeof(buf))) > 0) {
                int nw = Util::WriteFixedBytes(connfd, buf, nr);
                if (nw < nr) {
                    break;
                }
            }
            std::cout << "thread for no. " << cnt << " client ended." << std::endl;
        });
        thr.detach();
    }

    return 0;
}
