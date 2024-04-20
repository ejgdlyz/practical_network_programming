#pragma once

#include <string>
#include <stdint.h>

struct Options {
    uint16_t port;
    int length;
    int number;
    bool transmit, receive, nodelay;
    std::string host;

    Options() : port(0), length(0), number(0), transmit(0), receive(0), nodelay(0) {
    }
};

struct SessionMessage {
    int32_t number;             // 消息数量
    int32_t length;             // 消息长度
} __attribute__ ((__packed__));

struct PayloadMessage {
    int32_t length;
    char data[0];
};


bool ParseCommandLine(int argc, char* argv[], Options& opt);
struct sockaddr_in ResolveOrDie(const char* host, uint16_t port);

void transmit(const Options& opt);
void receive(const Options& opt);

int64_t GetNow();


