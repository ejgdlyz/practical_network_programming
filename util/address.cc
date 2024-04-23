#include "address.h"

#include <sstream>
#include <netdb.h>
#include <string.h>

bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host, 
            int family, int type, int protocol) {
    
    addrinfo hints, *results, *next;
    bzero(&hints, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;

    std::string node;
    const char* service = NULL;

    // ipv6 [ip]:port
    if (!host.empty() && host[0] == '[') {
        const char* endipv6 = reinterpret_cast<const char*>(memchr(host.c_str() + 1, ']', host.size() - 1));
        if (endipv6) {
            if ( *(endipv6 + 1) == ':') {
                service = endipv6 + 2;  // service is part after removing the port
            }
            node = host.substr(1, endipv6 - host.c_str() - 1 );  // ipv6's ip part
        }
    }

    // ipv4
    if (node.empty()) {
        service = reinterpret_cast<const char*>(memchr(host.c_str(), ':', host.size()));
        if (service) {
            if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    if (node.empty()) {
        node = host;
    }

    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if (error) {
        perror("getaddrinfo");
        return false;
    }

    next = results;
    while (next) {
        result.push_back(Create(next->ai_addr, next->ai_addrlen));
        next = next->ai_next;
    }

    freeaddrinfo(results);
    return true;

}

Address::ptr Address::Create(const sockaddr* addr, socklen_t addr_len) {
    if (!addr) {
        return nullptr;
    }

    Address::ptr result = nullptr;
    switch (addr->sa_family)
    {
    case AF_INET:
        result.reset(new IPv4Address(*reinterpret_cast<const sockaddr_in*>(addr)));
        break;
    case AF_INET6:
        break;
    case AF_UNIX:
        break;
    default:
        break;
    }
    return result;
}
    
Address::ptr Address::LookupAny(const std::string& host, 
        int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    if (Lookup(result, host, family, type, protocol)) {
        return result[0];
    }
    return nullptr;
}

std::shared_ptr<IPAddress> Address::LookupAnyIPAddress(const std::string& host, 
        int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    if (Lookup(result, host, family, type, protocol)) {
        for (auto& res : result) {
            IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(res);
            if (v) {
                return v;
            }
        }
    }
    return nullptr;
}

int Address::getFamily() const {
    return getAddr()->sa_family;
}

std::string Address::toString() const {
    std::stringstream ss;
    toString(ss);
    return ss.str();
}

// IPAddress
IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
    addrinfo hints, *results;
    bzero(&hints, sizeof(hints));

    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(address, NULL, &hints, &results);
    if (error) {
        perror("getaddrinfo");
        return nullptr;
    }

    IPAddress::ptr result = nullptr;
    try {
        result = std::dynamic_pointer_cast<IPAddress>(
            Address::Create(results->ai_addr, results->ai_addrlen));
        if (result) {
            result->setPort(port);
        }
    } catch (...) {
        perror("IPAddress::Create() error");
    }

    freeaddrinfo(results);
    return result;
}

// IPv4
IPv4Address::ptr IPv4Address::Create(const char* ip, uint16_t port) {
    IPv4Address::ptr ipv4(new IPv4Address);
    ipv4->m_addr.sin_port = htons(port);
    int rt = inet_pton(AF_INET, ip, &ipv4->m_addr.sin_addr);
    if (rt <= 0) {
        perror("inet_pton");
        return nullptr;
    }
    return ipv4;
}

IPv4Address::IPv4Address(const sockaddr_in& address)
    : m_addr(address) {
}

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
    bzero(&m_addr, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(port);
    m_addr.sin_addr.s_addr = htonl(address);
}

const sockaddr* IPv4Address::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

sockaddr* IPv4Address::getAddr() {
    return reinterpret_cast<sockaddr*>(&m_addr);
}

socklen_t IPv4Address::getAddrLen() const {
    return sizeof(m_addr);
}

uint16_t IPv4Address::getPort() const {
    return ntohs(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t port) {
    m_addr.sin_port = htons(port);
}

std::ostream& IPv4Address::toString(std::ostream& os) const {
    uint32_t addr = ntohl(m_addr.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "."
        << ((addr >> 16) & 0xff) << "."
        << ((addr >> 8) & 0xff) << "."
        << (addr & 0xff);
    
    os << ":" << ntohs(m_addr.sin_port);
    return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return addr.toString(os);
}
