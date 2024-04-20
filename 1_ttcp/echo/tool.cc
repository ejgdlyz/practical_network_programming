#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include "tool.h"

int Tool::WriteFixedBytes(int sockfd, const void* buf, int length) {
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

int Tool::ReadFixedBytes(int sockfd, void* buf, int length) {
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