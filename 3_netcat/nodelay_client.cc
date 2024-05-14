#include <iostream>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <iomanip>

#include "sylar/socket.h"
#include "util/util.h"

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " [-b] [-N] hostname message_length" << std::endl;
        std::cout << "\t -b Buffering before sending." << std::endl;
        std::cout << "\t -N Start Nagle." << std::endl;
        exit(1);

    }
    
    int opt = 0;
    bool buffering = false;
    bool nagle = false;
    while ( (opt = getopt(argc, argv, "bN")) != -1) {
        switch (opt)
        {
        case 'b':
            buffering = true;
            break;
        case 'N':
            nagle = true;
            break;
        default:
            std::cout << "Unknown option " << static_cast<char>(opt) << std::endl;
            break;
        }
    }

    if (optind > argc - 2) {
        std::cout << "Please specify hostname and message_length." << std::endl;
        exit(1);
    }

    std::string hostname = argv[optind];
    int len = atoi(argv[optind  + 1]);

    auto addr = sylar::Address::LookupAny(hostname + ":3210");
    if (!addr) {
        std::cout << "Unable to resolve " << argv[1] << std::endl;
        exit(1);
    }

    std::cout << "connecting to " << *addr << std::endl;
    sylar::Socket::ptr sock = sylar::Socket::CreateTCPSocket();
    bool rt = sock->connect(addr);
    if (!rt) {
        std::cout << "Unable to resolve " << *sock->getRemoteAddress() << std::endl;
        exit(1);
    }

    if (nagle) {
        // sock->setTcpNoDelay(false);  // open Nagle
        int val = 0;
        sock->setOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
        std::cout << "connected, set TCP_NODELAY" << std::endl;
    } else {
        int val = 1;
        // sock->setTcpNoDelay(true); // close Nagle
        sock->setOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
        std::cout << "connected" << std::endl;
    }

    std::cout << "sending " << len << " bytes" << std::endl;

    double start = Util::GetNowSeconds();
    if (buffering) {
        std::vector<char> message(len + sizeof(len), 'S');     //  4 bytes header + payload 
        memcpy(message.data(), &len, sizeof(len));
        int nw = Util::WriteFixedBytes(sock, message.data(), message.size());  // sending this msg once
        std::cout << std::fixed << std::setprecision(6) 
            << Util::GetNowSeconds() << " sent " << nw << " bytes" << std::endl;
    } else {
        Util::WriteFixedBytes(sock, &len, sizeof(len));         // 1. sending 4 bytes header
        usleep(1000);       // waiting 1ms to prevent kernel merging TCP segments
        std::string message(len, 'S');
        int nw = Util::WriteFixedBytes(sock, message.data(), message.size()); // 2. sending paylaod
        std::cout << std::fixed << std::setprecision(6) 
            << Util::GetNowSeconds() << " sent " << nw << " bytes" << std::endl;
    }

    int ack = 0;
    int nr = Util::ReadFixedBytes(sock, &ack, sizeof(ack));
    std::cout << "received " << nr << " bytes, ack = " << ack << std::endl;
    std::cout << std::fixed << std::setprecision(6)
        << Util::GetNowSeconds() - start << " seconds" << std::endl;  // test delay with/without Nagle's Algorithm
    return 0;
}
