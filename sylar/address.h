#ifndef __SYLAR_ADDRESS_H__
#define __SYLAR_ADDRESS_H__

#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <arpa/inet.h>
#include <sys/un.h>
#include <map>
#include <vector>

namespace sylar {

class IPAddress;

class Address {
public:
    typedef std::shared_ptr<Address> ptr;
    ~Address() {}
    
    static Address::ptr Create(const sockaddr* addr, socklen_t);
    static bool Lookup(std::vector<Address::ptr>& result, const std::string& host, 
            int family = AF_INET, int type = 0, int protocol = 0);
   
    static Address::ptr LookupAny(const std::string& host,
                        int family = AF_INET, int type = 0, int protocol = 0);

    static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                        int family = AF_INET, int type = 0, int protocol = 0);

    // 通过网卡得到 IPAddress
    static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& res, 
                    int family = AF_INET);
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& res, 
                    const std::string& iface, int family = AF_INET);

    int getFamily() const;                                      // socket 类型

    virtual const sockaddr* getAddr() const = 0;
    virtual sockaddr* getAddr() = 0;
    virtual socklen_t getAddrLen() const = 0;

    virtual std::ostream& insert(std::ostream& os) const = 0;   // 序列化 
    std::string toString() const;

    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;

};

class IPAddress : public Address {
public:
    typedef std::shared_ptr<IPAddress> ptr;

    static IPAddress::ptr Create(const char* address, uint16_t port = 0);   // IPv4/IPv6 域名转 IP地址

    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;       // 通过 IP 得到广播地址
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;         // 网络地址
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;             // 子网掩码

    virtual uint16_t getPort() const = 0;
    virtual void setPort(uint16_t port) = 0;
};

class IPv4Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv4Address> ptr;

    static IPv4Address::ptr Create(const char* address, uint16_t port = 0); // str -> sockaddr_in

    IPv4Address(const sockaddr_in& address);
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);
    
    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;           // 序列化 
    
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;   // 通过 IP 得到广播地址
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;     // 网络地址
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;         // 子网掩码

    uint16_t getPort() const override;
    void setPort(uint16_t port) override;

private:
    sockaddr_in m_addr;
};

class IPv6Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv6Address> ptr;
    IPv6Address();
    IPv6Address(const sockaddr_in6 & address);
    IPv6Address(const uint8_t address[16], uint16_t port = 0);

    static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;           // 序列化 
    
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;   // 通过 IP 得到广播地址
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;     // 网络地址
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;         // 子网掩码

    uint16_t getPort() const override;
    void setPort(uint16_t port) override;

private:
    sockaddr_in6 m_addr;
};

class UnixAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnixAddress();
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    void setAddrLen(uint32_t v);
    std::string getPath() const;
    std::ostream& insert(std::ostream& os) const override;   // 序列化 

private:
    struct sockaddr_un m_addr;
    socklen_t m_length;
};

class UnknownAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnknownAddress(int family);
    UnknownAddress(const sockaddr& addr);
    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;   // 序列化 
private:
    sockaddr m_addr;
};

std::ostream& operator<< (std::ostream& os, const Address& addr);

}

#endif  // __SYLAR_ADDRESS_H__