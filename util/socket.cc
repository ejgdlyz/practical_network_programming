#include "socket.h"

#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sstream>

Socket::ptr Socket::CreateTCP(Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
    Socket::ptr sock(new Socket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
    Socket::ptr sock(new Socket(IPv4, UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol)
    : m_sock(-1)
    , m_family(family)
    , m_type(type)
    , m_protocol(protocol)
    , m_connected(false) {
}

Socket::~Socket() {
    close();
}

bool Socket::close() {
    if (!m_connected || m_sock == -1) {
        return true;
    }   

    m_connected = false;
    ::close(m_sock);
    m_sock = -1;
    return false;
}

void Socket::initSocketOpt() {
    int val = 1;
    ::setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (m_type == SOCK_STREAM) {  // set nodelay
        setTcpNoDelay(true);
    }
}

void Socket::setTcpNoDelay(bool tcp_nodelay) {
    int val = tcp_nodelay ? 1 : 0;
    ::setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
}

void Socket::newSocket() {
    m_sock = ::socket(m_family, m_type, m_protocol);
    if (m_sock != -1) {
        initSocketOpt();
    } else {
        perror("newSocket");
    }
}

bool Socket::init(int connfd) {
    m_connected = true;
    m_sock = connfd;
    initSocketOpt();
    getLocalAddress();
    getRemoteAddress();
    return true;
}

bool Socket::bind(const Address::ptr addr) {
    if (!isValid()) {
        newSocket();
        if (!isValid()) {
            return false;
        }
    }
    
    if (addr->getFamily() != m_family) {
        perror("addr.family != m_family");
        return false;
    }

    if (addr->getFamily() == UNIX) {
        // UNIX
        return false;
    }

    if (::bind(m_sock, addr->getAddr(), addr->getAddrLen())) {
        perror("bind");
        return false;
    }
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog) {
    if (!isValid()) {
        perror("m_sock = -1");
        return false;
    }

    if(::listen(m_sock, backlog)) {
        perror("listen");
        return false;
    }
    return true;
}

Socket::ptr Socket::accpet() {
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));  // create client socket
    int connfd = ::accept(m_sock, nullptr, nullptr);
    sock->init(connfd);

    return sock;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    if (!isValid()) {
        newSocket();
    }

    if (addr->getFamily() != m_family) {
        perror("addr.family != m_family");
        return false;
    }

    if (timeout_ms == (uint64_t)-1) {
        if (::connect(m_sock, addr->getAddr(), addr->getAddrLen())) {
            perror("connect");
            close();
            return false;
        }
    } else {
        // TODO: connect_with_timeout()
    }

    m_connected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;

}

int Socket::send(const void* buffer, size_t length, int flags) {
    if (isConnected()) {
        return ::send(m_sock, buffer, length, flags);
    }
    return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags) {
    if (isConnected()) {
        msghdr msg;
        bzero(&msg, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(buffers);
        msg.msg_iovlen = length;
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
    if (isConnected()) {
        return ::sendto(m_sock, buffer, length, flags, to->getAddr(), flags); 
    }
    return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
    if (isConnected()) {
        msghdr msg;
        bzero(&msg, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(buffers);
        msg.msg_iovlen = length;
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::recv(void* buffer, size_t length, int flags) {
    if (isConnected()) {
        return ::recv(m_sock, buffer, length, flags);
    }
    return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags) {
    if (isConnected()) {
        msghdr msg;
        bzero(&msg, sizeof(msg));
        msg.msg_iov = buffers;
        msg.msg_iovlen = length;
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::recvFrom(void* buffer, size_t length, const Address::ptr from, int flags) {
    if (isConnected()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(m_sock, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, const Address::ptr from, int flags) {
    if (isConnected()) {
        msghdr msg;
        bzero(&msg, sizeof(msg));
        msg.msg_iov = buffers;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::getLocalAddress() {
    if (m_localAddress) {
        return m_localAddress;
    }

    Address::ptr addr;
    switch (m_family) {
        case AF_INET:
            addr.reset(new IPv4Address());
            break;
        case AF_INET6:
            break;
        case AF_UNIX:
            break;
        default:
            break;
    }

    socklen_t addr_len = addr->getAddrLen();
    if (::getsockname(m_sock, addr->getAddr(), &addr_len)) {
        perror("getsockname");
        return nullptr;
    }
    if (m_family == AF_UNIX) {
        // 
    }

    m_localAddress = addr;
    return m_localAddress;
}

Address::ptr Socket::getRemoteAddress() {
    if (m_remoteAddress) {
        return m_remoteAddress;
    }

    Address::ptr addr;
    switch (m_family) {
        case AF_INET:
            addr.reset(new IPv4Address());
            break;
        case AF_INET6:
            break;
        case AF_UNIX:
            break;
        default:
            break;
    }
    socklen_t addr_len = addr->getAddrLen();
    if (::getpeername(m_sock, addr->getAddr(), &addr_len)) {
        perror("getpeername");
        return nullptr;
    }
    if (m_family == AF_UNIX) {
        // 
    }

    m_remoteAddress = addr;
    return m_remoteAddress;
}

bool Socket::isValid() const {
    return m_sock != -1;
}

std::ostream& Socket::dump(std::ostream& os) const {
    os << "[Socket sock=" << m_sock << " is_connected=" << m_connected
        << " family=" << m_family << " type=" << m_type
        << " protocol=" << m_protocol;
    
    if (m_localAddress)  {
        os << " local_address=" << *m_localAddress;
    }
    if (m_remoteAddress) {
        os << " remote_address=" << *m_remoteAddress;
    }
    os << "]";
    return os;
}
