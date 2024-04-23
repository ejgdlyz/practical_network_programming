#pragma once

#include <memory>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <string>

class IPAddress;

class Address {
public:
    typedef std::shared_ptr<Address> ptr;
    virtual ~Address() {}

    static Address::ptr Create(const sockaddr* addr, socklen_t addr_len);
    
    static bool Lookup(std::vector<Address::ptr>& result, const std::string& host, 
            int family = AF_INET, int type = 0, int protocol = 0);

    static Address::ptr LookupAny(const std::string& host, 
            int family = AF_INET, int type = 0, int protocol = 0);

    static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host, 
            int family = AF_INET, int type = 0, int protocol = 0);

    int getFamily() const;

    virtual const sockaddr* getAddr() const = 0;
    virtual sockaddr* getAddr() = 0;
    virtual socklen_t getAddrLen() const = 0;

    virtual std::ostream& toString(std::ostream& os) const = 0;   // 序列化 
    std::string toString() const;
};

class IPAddress : public Address {
public:
    typedef std::shared_ptr<IPAddress> ptr;

    static IPAddress::ptr Create(const char* address, uint16_t port);

    virtual uint16_t getPort() const = 0;
    virtual void setPort(uint16_t port) = 0;
};

class IPv4Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv4Address> ptr;

    static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

    IPv4Address(const sockaddr_in& address);
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;

    uint16_t getPort() const override;
    void setPort(uint16_t port) override;
    
    std::ostream& toString(std::ostream& os) const override;
private:
    sockaddr_in m_addr;
};

std::ostream& operator<<(std::ostream& os, const Address& addr);
