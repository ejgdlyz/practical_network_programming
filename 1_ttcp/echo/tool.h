#pragma once

struct Tool {
    static int ReadFixedBytes(int sockfd, void* buf, int length);
    static int WriteFixedBytes(int sockfd, const void* buf, int length);
};


