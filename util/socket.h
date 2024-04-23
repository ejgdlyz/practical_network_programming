#pragma once

#include <memory>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "address.h"

class Socket : public std::enable_shared_from_this<Socket> {
public:
    typedef std::shared_ptr<Socket> ptr;
    typedef std::weak_ptr<Socket> weak_ptr;

    enum TYPE {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };

    enum Family {
        IPv4 = AF_INET, 
        IPv6 = AF_INET6,
        UNIX = AF_UNIX
    };

    static Socket::ptr CreateTCP(Address::ptr address);
    static Socket::ptr CreateUDP(Address::ptr address);

    static Socket::ptr CreateTCPSocket();
    static Socket::ptr CreateUDPSocket();

    Socket(int family, int type, int protocol = 0);
    ~Socket();

    Socket::ptr accpet();

    bool bind(const Address::ptr addr);
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
    bool listen(int backlog = SOMAXCONN);
    bool close();

    int send(const void* buffer, size_t length, int flags = 0);
    int send(const iovec* buffers, size_t length, int flags = 0);
    int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);
    int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);

    int recv(void* buffer, size_t length, int flags = 0);
    int recv(iovec* buffers, size_t length, int flags = 0);
    int recvFrom(void* buffer, size_t length, const Address::ptr from, int flags = 0);
    int recvFrom(iovec* buffers, size_t length, const Address::ptr from, int flags = 0);

    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();

    std::ostream& dump(std::ostream& os) const;
    int getSocket() const { return m_sock;}

    bool isConnected() const { return m_connected;}
    bool isValid() const;
    
    void setTcpNoDelay(bool tcp_nodelay = true);
protected:
    void initSocketOpt();
    void newSocket();
    bool init(int connfd);
    void setSockFd(int sockfd) { m_sock = sockfd;}

private:
    int m_sock;
    int m_family;
    int m_type;
    int m_protocol;
    int m_connected;

    Address::ptr m_localAddress;
    Address::ptr m_remoteAddress;
};