#pragma once

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iomanip>
#include <memory>

#include "socket.h"

struct Util {
    static int ReadFixedBytes(int sockfd, void* buf, int length);
    static int WriteFixedBytes(int sockfd, const void* buf, int length);

    static int ReceiveAll(Socket::ptr sockfd, void* buf, int length);
    static int ReadFixedBytes(Socket::ptr sockfd, void* buf, int length);
    static int WriteFixedBytes(Socket::ptr sockfd, const void* buf, int length);
    
    static int GetListenfd(uint16_t port);
    static int AcceptOrDie(uint16_t port);

    static int64_t GetNow();
    static double GetNowSeconds();
};


