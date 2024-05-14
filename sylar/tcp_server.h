#ifndef __SYLAR_TCP_SERVER__
#define __SYLAR_TCP_SERVER__

#include <memory>
#include <functional>
#include <string>
#include "address.h"
#include "iomanager.h"
#include "socket.h"
#include "noncopyable.h"
#include "config.h"

namespace sylar {

struct TcpServerConf {
    typedef std::shared_ptr<TcpServerConf> ptr;

    std::vector<std::string> address;
    int keepalive = 0;
    int timeout = 1000 * 2 * 60;
    int ssl = 0;
    std::string id;
    /// 服务器类型，http, ws
    std::string type = "http";
    std::string name;
    std::string cert_file;
    std::string key_file;
    std::string accept_worker;
    std::string process_worker;
    std::map<std::string, std::string> args;

    bool isValid() const {
        return !address.empty();
    }

    bool operator==(const TcpServerConf& oth) const {
        return address == oth.address
            && keepalive == oth.keepalive
            && timeout == oth.timeout
            && name == oth.name
            && ssl == oth.ssl
            && cert_file == oth.cert_file
            && key_file == oth.key_file
            && accept_worker == oth.accept_worker
            && process_worker == oth.process_worker
            && args == oth.args
            && id == oth.id
            && type == oth.type;
    }
};

// 偏特化 ： 配置系统与自定义类型的转换
template<>
class LexicalCast<std::string, TcpServerConf> {
public:
    TcpServerConf operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        TcpServerConf conf;
        conf.id = node["id"].as<std::string>(conf.id);
        conf.type = node["type"].as<std::string>(conf.type);
        conf.keepalive = node["keepalive"].as<int>(conf.keepalive);
        conf.timeout = node["timeout"].as<int>(conf.timeout);
        conf.name = node["name"].as<std::string>(conf.name);
        conf.ssl = node["ssl"].as<int>(conf.ssl);
        conf.cert_file = node["cert_file"].as<std::string>(conf.cert_file);
        conf.key_file = node["key_file"].as<std::string>(conf.key_file);
        conf.accept_worker = node["accept_worker"].as<std::string>();
        conf.process_worker = node["process_worker"].as<std::string>();
        conf.args = LexicalCast<std::string
            ,std::map<std::string, std::string> >()(node["args"].as<std::string>(""));
        if(node["address"].IsDefined()) {
            for(size_t i = 0; i < node["address"].size(); ++i) {
                conf.address.push_back(node["address"][i].as<std::string>());
            }
        }
        return conf;
    }
};

// YAML node -> string 
template<>
class LexicalCast<TcpServerConf, std::string> {
public:
    std::string operator()(const TcpServerConf& conf) {
        YAML::Node node;
        node["id"] = conf.id;
        node["type"] = conf.type;
        node["name"] = conf.name;
        node["keepalive"] = conf.keepalive;
        node["timeout"] = conf.timeout;
        node["ssl"] = conf.ssl;
        node["cert_file"] = conf.cert_file;
        node["key_file"] = conf.key_file;
        node["accept_worker"] = conf.accept_worker;
        node["process_worker"] = conf.process_worker;
        node["args"] = YAML::Load(LexicalCast<std::map<std::string, std::string>
            , std::string>()(conf.args));
        for(auto& i : conf.address) {
            node["address"].push_back(i);
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

class TcpServer : public std::enable_shared_from_this<TcpServer>, NonCopyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    TcpServer(sylar::IOManager* work = sylar::IOManager::GetThis()
            , sylar::IOManager* accept_work = sylar::IOManager::GetThis());
    virtual ~TcpServer();

    virtual bool bind(sylar::Address::ptr addr, bool ssl = false);                // 监听地址
    virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& failed_addrs, bool ssl = false);

    bool loadCertificates(const std::string& cert_file, const std::string& key_file);

    virtual bool start();  
    virtual void stop();

    uint64_t getRecvTimeout() const { return m_recvTimeout;}
    std::string getName() const { return m_name;}
    void setRecvTimeout(uint64_t v) { m_recvTimeout = v;}  
    virtual void setName(const std::string& v) { m_name = v;}  
    bool isStop() const { return m_isStop;}

    TcpServerConf::ptr getConf() const { return m_conf;}
    void setConf(TcpServerConf::ptr v) { m_conf = v;}
    void setConf(const TcpServerConf& v);
protected:
    virtual void handleClient(Socket::ptr client);  // 每 accpet 一次，执行一次这个回调函数
    virtual void startAccept(Socket::ptr sock);
protected:
    std::vector<Socket::ptr> m_socks;       // 监听 socket 数组
    IOManager* m_worker;                    // 工作线程池，执行 hanleClient()
    IOManager* m_acceptWork;                // accept() 线程池 
    uint64_t m_recvTimeout;                 // 接收超时时间 ms
    std::string m_name;                     // 服务器名称
    std::string m_type = "tcp";             // 服务器类型
    bool m_isStop;

    bool m_ssl = false;

    TcpServerConf::ptr m_conf;              // 服务器配置
};


}


#endif  // __SYLAR_TCP_SERVER__