#include <iostream>
#include <thread>
#include <functional>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>

#include "util/util.h"

struct Message {
    int64_t request;
    int64_t response;
} __attribute__ ((__packed__));

void RunServer(const std::string& ip, int16_t port) {
    struct sockaddr_in serv_addr;

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock >= 0);

    int rt = bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(rt == 0);

    while (true) {
        Message message = {0, 0};

        struct sockaddr_in peer_addr;
        bzero(&peer_addr, sizeof(peer_addr));
        socklen_t addrlen = sizeof(peer_addr);
        ssize_t nr = recvfrom(sock, &message, sizeof(message), 0, (struct sockaddr*)&peer_addr, &addrlen);
        if (nr == sizeof(message)) {
            message.response = Util::GetNow();
            ssize_t nw = sendto(sock, &message, sizeof(message), 0, (struct sockaddr*)&peer_addr, addrlen);
            if (nw < 0) {
                perror("fail to send message");
            } else if (nw != sizeof(message)) {
                std::cout << "sent message of " << nw << "bytes, expect " 
                        << sizeof(message) << "bytes" << std::endl; 
            } else {
                // success
            }
        } else if (nr < 0) {
            perror("recv message");
        } else {
            std::cout << "received message of " << nr << "bytes, expect " 
                     << sizeof(message) << "bytes" << std::endl; 
        }
    }

}

void RunClient(const std::string& ip, int16_t port) {
    struct sockaddr_in serv_addr;
    int rt = 0;

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock >= 0);

    rt = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    std::cout << "rt = " << rt << std::endl;
    assert(rt == 0);

    std::thread thr([&sock, &serv_addr](){
        while (true) {
            Message message = {0, 0};
            message.request = Util::GetNow();
            ssize_t nw = sendto(sock, &message, sizeof(message), 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
            if (nw < 0) {
                perror("fail to send message");
            } else if (nw != sizeof(message)) {
                std::cout << "received message of " << nw << "bytes, expect " 
                        << sizeof(message) << "bytes" << std::endl; 
            }

            usleep(200 * 1000);  // wait for 200 us
        }
    });

    while (true) {
        Message message = {0, 0};
        struct sockaddr_in peer_addr;
        socklen_t addr_len;
        bzero(&peer_addr, sizeof(peer_addr));
        int nr = recvfrom(sock, &message, sizeof(message), 0, (struct sockaddr*)&peer_addr, &addr_len);
        if (nr == sizeof(message)) {
            int64_t back = Util::GetNow();
            int64_t mine = (back + message.request) / 2;
            std::cout << "now " << back 
                    << " round trip " << back - message.request 
                    << " clock error " << message.response - mine << std::endl;
        } else if (nr < 0) {
            perror("fail to receive Message");
        } else {
            std::cout << "received message of " << nr << "bytes, expect " 
                     << sizeof(message) << "bytes" << std::endl; 
        }
    }
}

int main(int argc, char const *argv[]) {
    if (argc > 2) {
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        if (strcmp(argv[1], "-s") == 0) {
            RunServer("127.0.0.1", port);
        } else {
            RunClient(argv[1], port);
        }
    } else {
        std::cout << "Usage:\n" 
                << argv[0] << " -s port\n" 
                << argv[0] << " ip port"<< std::endl;
    }

    return 0;
}
