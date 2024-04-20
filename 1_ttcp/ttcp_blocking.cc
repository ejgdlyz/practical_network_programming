#include <iostream>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iomanip>

#include "common.h"

static int AcceptOrDie(uint16_t port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int rt = 0;
    int yes = 1;
    rt = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (rt) {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    // inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    addr.sin_port = htons(port);

    rt = bind(listenfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (rt) {
        perror("bind");
        exit(1);
    }

    rt = listen(listenfd, 5);
    if (rt) {
        perror("listen");
        exit(1);
    }

    struct sockaddr_in peer_addr;
    bzero(&peer_addr, sizeof(peer_addr));
    socklen_t addr_len = 0;
    int connfd = accept(listenfd, reinterpret_cast<struct sockaddr*>(&peer_addr), &addr_len);
    if (connfd < 0) {
        perror("accept");
        exit(1);
    }

    close(listenfd);
    return connfd;
}

static int WriteFixedBytes(int sockfd, const void* buf, int length) {
    int written = 0;
    while (written < length) {
        ssize_t nw = write(sockfd, static_cast<const char*>(buf) + written, length - written);
        if (nw > 0) {
            written += static_cast<int>(nw);
        } else if (nw == 0) {
            break;
        } else if (errno != EINTR) {
            perror("write");
            break;
        }
    }
    return written;
}

static int ReadFixedBytes(int sockfd, void* buf, int length) {
    int nread = 0;
    while (nread < length) {
        ssize_t nr = read(sockfd, static_cast<char*>(buf) + nread, length - nread);
        if (nr > 0) {
            nread += static_cast<int>(nr);
        } else if (nr == 0) {
            break;
        } else if (errno != EINTR) {
            perror("read");
            break;
        }
    }
    return nread;
}

void transmit(const Options& opt) {
    struct sockaddr_in addr = ResolveOrDie(opt.host.c_str(), opt.port);
    std::cout << "connected to " << inet_ntoa(addr.sin_addr) << ":" << opt.port << std::endl;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int ret = connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (ret) {
        perror("connect");
        std::cout << "Unable to connect " << opt.host << std::endl;
        close(sockfd);
        return;
    }
    
    std::cout << "connected" << std::endl;
    int64_t start = GetNow();
    struct SessionMessage session_msg = {0, 0};
    session_msg.number = htonl(opt.number);
    session_msg.length = htonl(opt.length);

    if (WriteFixedBytes(sockfd, &session_msg, sizeof(session_msg)) != sizeof(session_msg)) {
        perror("write SessionMessage");
        exit(1);
    }

    const int total_len = static_cast<int>(sizeof(int32_t) + opt.length);
    // PayloadMessage* payload = static_cast<PayloadMessage*>(malloc(total_len));
    auto free_deleter = [](PayloadMessage* ptr) { free(ptr); };
    std::unique_ptr<PayloadMessage, decltype(free_deleter)> payload(
            static_cast<PayloadMessage*>(malloc(total_len)), free_deleter);
    assert(payload);

    payload->length = htonl(opt.length);
    for (int i = 0; i < opt.length; ++i) {
        payload->data[i] = "0123456789ABCDEF"[i % 16];
    }

    double total_mb = 1.0 * opt.length * opt.number / 1024 / 1024;
    std::cout << std::fixed << std::setprecision(3) << total_mb << " MiB in total" << std::endl;

    for (int i = 0; i < opt.number; ++i) {
        int nw = WriteFixedBytes(sockfd, payload.get(), total_len);
        assert(nw == total_len);

        int ack = 0;
        int nr = ReadFixedBytes(sockfd, &ack, sizeof(ack));
        assert(nr == sizeof(ack));
        ack = ntohl(ack);
        assert(ack == opt.length);
    }

    close(sockfd);
    double elapsed = (GetNow() - start) / 1000 / 1000;
    printf("%.3f seconds\n%.3f MiB/s\n", elapsed, total_mb / elapsed);
}

void receive(const Options& opt) {
    int sockfd = AcceptOrDie(opt.port);

    struct SessionMessage session_msg = {0, 0};
    if (ReadFixedBytes(sockfd, &session_msg, sizeof(session_msg)) != sizeof(session_msg)) {
        perror("read SessionMessage");
        exit(1);
    }

    session_msg.number = ntohl(session_msg.number);
    session_msg.length = ntohl(session_msg.length);
    std::cout << "received number = " << session_msg.number << std::endl
            << "received length = " << session_msg.length << std::endl;
    
    const int total_len = static_cast<int>(sizeof(int32_t) + session_msg.length);
    auto deleter = [](PayloadMessage* ptr){ free(ptr);};
    std::unique_ptr<PayloadMessage, decltype(deleter)> payload(
        static_cast<PayloadMessage*>(malloc(total_len)), deleter);
    assert(payload);

    for (int i = 0; i < session_msg.number; ++i) {
        payload->length = 0;
        // 读取 消息头长度
        if (ReadFixedBytes(sockfd, &payload->length, sizeof(payload->length)) != sizeof(payload->length)) {
            perror("read error");
            exit(1);
        }
        payload->length = ntohl(payload->length);
        assert(payload->length == session_msg.length);
        
        // 读取 payload 
        if (ReadFixedBytes(sockfd, payload->data, payload->length) != payload->length) {
            perror("read payload error");
            exit(1);
        }

        int32_t ack = htonl(payload->length);
        if (WriteFixedBytes(sockfd, &ack, sizeof(ack)) != sizeof(ack)) {
            perror("read payload error");
            exit(1);
        }
    }

    close(sockfd);
}