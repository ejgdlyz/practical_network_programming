#include <iostream>
#include <memory>
#include <vector>

#include "util/util.h"
#include "sylar/socket.h"

int main(int argc, char *argv[])
{   
    auto addr = sylar::Address::LookupAnyIPAddress("localhost:3210");
    if (!addr) {
        perror("addr = nullptr");
        exit(0);
    }
    std::cout << *addr << std::endl;
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    sock->bind(addr);
    sock->listen();
    std::cout << "Accepting... Ctrl-C to exit" << std::endl;
    int cnt = 0;
    while (true) {
        sylar::Socket::ptr client_socket = sock->accept();
        std::cout << "accepted no. " << ++cnt << " client" << std::endl;
        
        int len = 0;
        int nr = Util::ReadFixedBytes(client_socket, &len, sizeof(len));  // header 
        std::cout << Util::GetNowSeconds() << " received header payload " << len << " bytes" << std::endl;

        std::vector<char> buf(len);                         
        nr = Util::ReadFixedBytes(client_socket, buf.data(), len);       // payload
        assert(nr == len);

        int nw = Util::WriteFixedBytes(client_socket, &len, sizeof(len));
        assert(nw == sizeof(len));

        std::cout << "no. " << cnt << " client ended." << std::endl;
    }
    return 0;
}
