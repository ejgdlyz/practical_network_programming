#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "util/util.h"

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: \n\t" << "./nc_receiver ip port" << std::endl;
        exit(1);
    }

    const char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    std::cout << "sender sleep(10) 期间是否要输入数据(y/n): ";
    char c;
    std::cin >> c;
    std::string buf;
    if (c == 'y') {
        std::cin >> buf;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int rt = connect(sockfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr));
    assert(rt == 0);

    if (c == 'y') {
        int nw = Util::WriteFixedBytes(sockfd, buf.c_str(), buf.size());
        assert(nw == static_cast<int>(buf.size()));
    }

    const int BUF_SIZE = 4 * 1024000; 
    char read_buf[BUF_SIZE];
    int nr;
    int total_bytes = 0;
    while ( (nr = read(sockfd, read_buf, sizeof(buf))) > 0) {
        total_bytes += nr;
    }
    std::cout << "received file size is " << total_bytes  << " bytes." << std::endl;

    close(sockfd);
    return 0;
}
