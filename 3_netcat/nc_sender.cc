#include <iostream>
#include <thread>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include "util/util.h"

void sender (const std::string& filename, int sockfd) {
    // FILE* fp = fopen(filename.c_str(), "rb");
    // if (!fp) {
    //     return;
    // }
    std::ifstream ifs;
    ifs.open(filename, std::ios_base::app);
    if (!ifs.is_open()) {
        return;
    }
    
    std::cout << "sleeping 10 seconds" << std::endl;
    sleep(10);

    std::cout << "starting sending file " << filename << std::endl;
    char buf[8192];
    // size_t nr = 0;
    // while((nr = fread(buf, 1, sizeof(buf), fp)) > 0) {
    while ( !ifs.eof()) {
        ifs.read(buf, sizeof(buf));
        Util::WriteFixedBytes(sockfd, buf, ifs.gcount());
    }
    //fclose(fp);
    std::cout << "Finish sending file " << filename << std::endl;
    
    // safe close connection
    // std::cout << "shutdown write and read until EOF" << std::endl;
    // shutdown(sockfd, SHUT_WR);  // 关闭写端
    // size_t nr = 0;
    // // 直到读到 EOF (read 返回 0)，表明对方已经关闭链接，再去调用 close()。
    // while ( (nr = read(sockfd, buf, sizeof(buf))) > 0) {
    //     // do something
    // }

    std::cout << "All done." << std::endl;
    close(sockfd);
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: \n\t" << "./sender filename port" << std::endl;
        exit(1);
    }

    int port = atoi(argv[2]);
    int listenfd = Util::GetListenfd(port);
    std::cout  << "Accepting... Ctrl-C to exit" << std::endl;
    int cnt = 0;
    while (true) {
        struct sockaddr_in peer_addr;
        socklen_t addr_len;
        bzero(&peer_addr, sizeof(peer_addr)); 
        int connfd = accept(listenfd, reinterpret_cast<struct sockaddr*>(&peer_addr), &addr_len);
        std::cout << "accepted no. " << ++cnt << " client" << std::endl;
        
        std::thread thr(sender, argv[1], connfd);
        thr.detach();
    }

    close(listenfd);
    return 0;
}
