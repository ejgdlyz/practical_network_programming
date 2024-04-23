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
#include "util/util.h"

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

    if (Util::WriteFixedBytes(sockfd, &session_msg, sizeof(session_msg)) != sizeof(session_msg)) {
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
        int nw = Util::WriteFixedBytes(sockfd, payload.get(), total_len);
        assert(nw == total_len);

        int ack = 0;
        int nr = Util::ReadFixedBytes(sockfd, &ack, sizeof(ack));
        assert(nr == sizeof(ack));
        ack = ntohl(ack);
        assert(ack == opt.length);
    }

    close(sockfd);
    double elapsed = (GetNow() - start) / 1000 / 1000;
    printf("%.3f seconds\n%.3f MiB/s\n", elapsed, total_mb / elapsed);
}

void receive(const Options& opt) {
    int sockfd = Util::AcceptOrDie(opt.port);

    struct SessionMessage session_msg = {0, 0};
    if (Util::ReadFixedBytes(sockfd, &session_msg, sizeof(session_msg)) != sizeof(session_msg)) {
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
        if (Util::ReadFixedBytes(sockfd, &payload->length, sizeof(payload->length)) != sizeof(payload->length)) {
            perror("read error");
            exit(1);
        }
        payload->length = ntohl(payload->length);
        assert(payload->length == session_msg.length);
        
        // 读取 payload 
        if (Util::ReadFixedBytes(sockfd, payload->data, payload->length) != payload->length) {
            perror("read payload error");
            exit(1);
        }

        int32_t ack = htonl(payload->length);
        if (Util::WriteFixedBytes(sockfd, &ack, sizeof(ack)) != sizeof(ack)) {
            perror("read payload error");
            exit(1);
        }
    }

    close(sockfd);
}