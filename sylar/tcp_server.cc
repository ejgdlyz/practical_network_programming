#include "tcp_server.h"
#include "config.h"
#include "log.h"

namespace sylar {

sylar::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout = 
    sylar::Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");  // 2 mins

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

TcpServer::TcpServer(sylar::IOManager* work, sylar::IOManager* accept_work) 
    :m_worker(work)
    ,m_acceptWork(accept_work)
    ,m_recvTimeout(g_tcp_server_read_timeout->getValue())
    ,m_name("sylar/1.0.0")  // name + version
    ,m_isStop(true){
}

TcpServer::~TcpServer() {
    for (auto& sock : m_socks) {
        sock->close();
    }    
    m_socks.clear();
}

void TcpServer::setConf(const TcpServerConf& v) {
    m_conf.reset(new TcpServerConf(v));
}

bool TcpServer::bind(sylar::Address::ptr addr, bool ssl) {
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> falied_addrs;
    addrs.push_back(addr);
    return bind(addrs, falied_addrs, ssl);
}  

bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& failed_addrs, bool ssl) {
    m_ssl = ssl;
    for (auto& addr : addrs) {
        Socket::ptr sock = ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
        if (!sock->bind(addr)) {
            SYLAR_LOG_ERROR(g_logger) << "fail to bind, errno=" << errno << ", errstr=" << strerror(errno)
                << ", addr=[" << addr->toString() << "]";
            failed_addrs.push_back(addr);
            continue;
        }
        if (!sock->listen()) {
            SYLAR_LOG_ERROR(g_logger) << "fail to listen, errno=" << errno << ", errstr=" << strerror(errno)
                << ", addr=[" << addr->toString() << "]";
            failed_addrs.push_back(addr);
            continue;
        }
        m_socks.push_back(sock);
        
    }
    
    if (!failed_addrs.empty()) {
        m_socks.clear();
        return false;
    }

    for (auto& sock: m_socks) {
        SYLAR_LOG_INFO(g_logger) << "type=" << m_type
            << " name=" << m_name
            << " ssl=" << m_ssl
            << " server bind success: " << *sock;
    }
    return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
    while (!m_isStop) {
        Socket::ptr client = sock->accept();
        if (client) {
            client->setRecvTimeout(m_recvTimeout); // 设置超时时间
            m_worker->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));  // 
        } else {
            SYLAR_LOG_ERROR(g_logger) << "accept errno = " << errno
                << " errstr=" << strerror(errno);
        }
    }
}

bool TcpServer::start() {
    if (!m_isStop) {
        return true;
    }
    m_isStop = false;
    for (auto& sock : m_socks) {
        m_acceptWork->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));  //
    }
    return true;
}

void TcpServer::stop() {
    m_isStop = true;
    auto self = shared_from_this();
    m_acceptWork->schedule([this, self]() {
        for (auto& sock : m_socks) {
            sock->cancelAll();
            sock->close();
        }
        m_socks.clear();
    });
}

void TcpServer::handleClient(Socket::ptr client) {
    SYLAR_LOG_INFO(g_logger) << "handleClient: " << *client;
}

// 加载认证
bool TcpServer::loadCertificates(const std::string& cert_file, const std::string& key_file) {
    for(auto& sock : m_socks) {
        auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(sock);
        if(ssl_socket) {
            if(!ssl_socket->loadCertificates(cert_file, key_file)) {
                return false;
            }
        }
    }
    return true;
}

}