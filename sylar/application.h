#ifndef __SYLAR_APPLICATION_H__
#define __SYLAR_APPLICATION_H__

#include "sylar/http/http_server.h"

namespace sylar {

class Application {
public:
    Application();

    static Application* GetInstance() { return s_instance;}
    bool init(int argc, char **argv);
    bool run();

    bool getServer(const std::string& type, std::vector<TcpServer::ptr>& servers);
private:
    int main(int argc, char** argv);
    int run_fiber();
private:
    int m_argc  = 0;
    char **m_argv = nullptr;

    // std::vector<sylar::http::HttpServer::ptr> m_httpServers;         // 保存启动的 server
    std::map<std::string, std::vector<TcpServer::ptr>> m_servers;       // 保存启动的 server
    IOManager::ptr m_mainIOManager;
    static Application* s_instance;

};

} // namespace sylar 

#endif  // __SYLAR_APPLICATION_H__