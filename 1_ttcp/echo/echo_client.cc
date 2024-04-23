#include <iostream>
#include <memory>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "util/util.h"

// if client run as `./echo_client 127.0.0.1 3100 20480000`, the client and server will be blocked.
int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cout << "please input <ip> <port> <length>" << std::endl;
        exit(1);
    }

    const char* ip = argv[1];
    uint16_t port = atoi(argv[2]);  
    const int len = atoi(argv[3]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int rt = connect(sockfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr));
    assert(rt == 0);

    std::cout << "connected..., sending " << len << " bytes" << std::endl;

    std::string message(len, 's');
    int nw = Util::WriteFixedBytes(sockfd, message.c_str(), message.size());
    std::cout << "sent " << nw << " bytes" << std::endl;

    auto deleter = [](char* ptr){ delete ptr;};
    std::unique_ptr<char, decltype(deleter)> receive(new char[len], deleter);
    int nr = Util::ReadFixedBytes(sockfd, receive.get(), len);
    std::cout << "received " << nr << " bytes" << std::endl;
    return 0;
}
