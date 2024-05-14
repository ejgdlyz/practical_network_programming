#include "util.h"

int Util::WriteFixedBytes(int sockfd, const void* buf, int length) {
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

int Util::ReadFixedBytes(int sockfd, void* buf, int length) {
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

int Util::ReadFixedBytes(sylar::Socket::ptr sock, void* buf, int length) {
    int nread = 0;
    while (nread < length) {
        int nr = sock->recv(static_cast<char*>(buf) + nread, length - nread);
        if (nr > 0) {
            nread += nr;
        } else if (nr == 0) {
            break;
        } else if (errno != EINTR) {
            perror("read");
            break;
        }
    }
    return nread;
}

int Util::ReceiveAll(sylar::Socket::ptr sock, void* buf, int length) {
    return sock->recv(static_cast<char*>(buf), length, MSG_WAITALL);
}

int Util::WriteFixedBytes(sylar::Socket::ptr sock, const void* buf, int length) {
   int written = 0;
    while (written < length) {
        int nw = sock->send(static_cast<const char*>(buf) + written, length - written);
        if (nw > 0) {
            written += nw;
        } else if (nw == 0) {
            break;
        } else if (errno != EINTR) {
            perror("write");
            break;
        }
    }
    return written; 
}

int Util::GetListenfd(uint16_t port) {
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

    return listenfd;
}

int Util::AcceptOrDie(uint16_t port) {
    int listenfd = GetListenfd(port);

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

int64_t Util::GetNow() {
    struct timeval tv = {0, 0};
    gettimeofday(&tv, NULL);
    return tv.tv_sec * (int64_t)(1000000) + tv.tv_usec;
}

double Util::GetNowSeconds() {
    struct timeval tv = {0, 0};
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}