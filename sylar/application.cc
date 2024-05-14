#include "application.h"

#include <unistd.h>

#include "sylar/daemon.h"
#include "sylar/config.h"
#include "sylar/env.h"
#include "sylar/log.h"
#include "sylar/module.h"
#include "sylar/worker.h"
#include "sylar/http/ws_server.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static sylar::ConfigVar<std::string>::ptr g_server_work_path = 
        sylar::Config::Lookup("server.work_path", std::string("/home/lambda/apps/work/sylar"), "server work path");

static sylar::ConfigVar<std::string>::ptr g_server_pid_file = 
        sylar::Config::Lookup("server.pid_file", std::string("sylar.pid"), "server pid file");

static sylar::ConfigVar<std::vector<TcpServerConf>>::ptr g_servers_conf = 
        sylar::Config::Lookup("servers", std::vector<TcpServerConf>(), "http servers config");

Application* Application::s_instance = nullptr;

Application::Application() {
    s_instance = this;
}

bool Application::init(int argc, char **argv) {
    m_argc = argc;
    m_argv = argv;

    sylar::EnvMgr::GetInstance()->addHelper("s", "start with the terminal");
    sylar::EnvMgr::GetInstance()->addHelper("d", "run as daemon");
    sylar::EnvMgr::GetInstance()->addHelper("c", "default conf path: ./conf");
    sylar::EnvMgr::GetInstance()->addHelper("p", "print help");

    bool is_print_help = false;
    if (!sylar::EnvMgr::GetInstance()->init(argc, argv)) {
        is_print_help = true;
    }

    if (sylar::EnvMgr::GetInstance()->has("p")) {
        is_print_help = true;
    }

    std::string conf_path = sylar::EnvMgr::GetInstance()->getConfigPath();  // 获取配置路径，以当前可执行文件为相对路径，默认取 conf
    SYLAR_LOG_INFO(g_logger) << "load conf path: " << conf_path;
    sylar::Config::loadFromConfDir(conf_path);                              // 加载配置

    ModuleMgr::GetInstance()->init();
    std::vector<Module::ptr> modules;
    ModuleMgr::GetInstance()->listAll(modules);
    
    for (auto& module : modules) {
        module->onBeforeArgsParse(argc, argv);
    }

    if (is_print_help) {
        sylar::EnvMgr::GetInstance()->printHelper();
        return false;
    }
    
    for (auto& module : modules) {
        module->onAfterArgsParse(argc, argv);
    }
    modules.clear();

    int run_type = 0;
    if (sylar::EnvMgr::GetInstance()->has("s")) {  // 命令行方式启动
        run_type = 1;
    } 
    if (sylar::EnvMgr::GetInstance()->has("d")) {  // 守护进程方式启动
        run_type = 2;
    }

    if (run_type == 0) {
        sylar::EnvMgr::GetInstance()->printHelper();
        return false;
    }

    std::string pidfile = g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
    
    if (sylar::FSUtil::IsRunningPidfile(pidfile)) {
        SYLAR_LOG_ERROR(g_logger) << "server is running: " << pidfile;
        return false;
    }

    // 创建进程的 工作路径，可以存放系统的信息
    if (!sylar::FSUtil::Mkdir(g_server_work_path->getValue())) {
        SYLAR_LOG_FATAL(g_logger) << "create work path [" << g_server_work_path->getValue()
                << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    return true;
}

bool Application::run() {
    bool is_daemon = sylar::EnvMgr::GetInstance()->has("d");
    return start_daemon(m_argc, m_argv
            , std::bind(&Application::main, this, std::placeholders::_1, std::placeholders::_2)
            , is_daemon);
}

int Application::main(int argc, char** argv) {
    SYLAR_LOG_INFO(g_logger) << "main";
    std::string conf_path = sylar::EnvMgr::GetInstance()->getConfigPath();
    sylar::Config::loadFromConfDir(conf_path, true);
    {
        std::string pidfile = g_server_work_path->getValue() + "/" 
                + g_server_pid_file->getValue();
    
        // 写入进程 id (子进程id if daemon)
        std::ofstream ofs(pidfile);
        if (!ofs) {
            SYLAR_LOG_ERROR(g_logger) << "open pidfile: " << pidfile << " failed.";
            return false;
        }
        ofs << getpid();
    }

    m_mainIOManager.reset(new sylar::IOManager(1, true, "main"));
    m_mainIOManager->schedule(std::bind(&Application::run_fiber, this));
    m_mainIOManager->addTimer(2000, [](){
        // SYLAR_LOG_INFO(g_logger) << "hello";
    }, true);
    m_mainIOManager->stop();
    return 0;
}

int Application::run_fiber() {
    std::vector<Module::ptr> modules;
    ModuleMgr::GetInstance()->listAll(modules);
    bool has_error = false;
    for (auto& module : modules) {
        if (!module->onLoad()) {
            SYLAR_LOG_ERROR(g_logger) << "module name=" << module->getName() 
                    << " version=" << module->getVersion()
                    << " filename=" << module->getFilename();
            has_error = true;
        }
    }
    if (has_error) {
        _exit(0);
    }
    sylar::WorkerMgr::GetInstance()->init();
    // server 放在协程里初始化
    auto http_confs = g_servers_conf->getValue();
    for (auto& conf : http_confs) {
        // SYLAR_LOG_INFO(g_logger) << std::endl << LexicalCast<TcpServerConf, std::string>()(conf);  // HttpServerConf -> string
        
        std::vector<Address::ptr> p_addrs;
        for(auto& addr : conf.address) {
            size_t pos = addr.find(":");
            if (pos == std::string::npos) {
                // SYLAR_LOG_ERROR(g_logger) << "invalid address: " << addr;
                p_addrs.push_back(UnixAddress::ptr(new UnixAddress(addr)));
                continue;
            }
            int32_t port = atoi(addr.substr(pos + 1).c_str());
            // 解析地址, 127.0.0.1
            auto p_addr = sylar::IPAddress::Create(addr.substr(0, pos).c_str(), port);
            if (p_addr) {
                p_addrs.push_back(p_addr);
                continue;
            } 
            // 地址解析不成功，解析网卡 
            std::vector<std::pair<Address::ptr, uint32_t>> results;
            if (sylar::Address::GetInterfaceAddresses(results, addr.substr(0, pos))) {
                for (auto& res : results) {
                    auto ip_addr = std::dynamic_pointer_cast<IPAddress>(res.first);
                    if (ip_addr) {
                        ip_addr->setPort(atoi(addr.substr(pos + 1).c_str()));
                    }
                    p_addrs.push_back(ip_addr);
                }
                continue;
            }
            auto aaddr = sylar::Address::LookupAny(addr);
            if (aaddr) {
                p_addrs.push_back(aaddr);
                continue;
            }
            SYLAR_LOG_ERROR(g_logger) << "invalid address: " << addr;
            _exit(0);
        }

        IOManager* accept_worker = sylar::IOManager::GetThis();
        IOManager* process_worker = sylar::IOManager::GetThis();
        if (!conf.accept_worker.empty()) {
            accept_worker = sylar::WorkerMgr::GetInstance()->getAsIOManager(conf.accept_worker).get();
            if (!accept_worker) {
                SYLAR_LOG_ERROR(g_logger) << "accept_worker: " << conf.accept_worker
                        << " not exists.";
                _exit(0);
            }
        }
        if (!conf.process_worker.empty()) {
            process_worker = sylar::WorkerMgr::GetInstance()->getAsIOManager(conf.process_worker).get();
            if (!process_worker) {
                SYLAR_LOG_ERROR(g_logger) << "process_worker: " << conf.process_worker
                        << " not exists.";
                _exit(0);
            }
        }
        // 创建 TcpServer
        TcpServer::ptr server;
        if (conf.type == "http") {
            // httpserver
            server.reset(new sylar::http::HttpServer(conf.keepalive, process_worker, accept_worker));
        } else if (conf.type == "ws") {
            // wsserver
            server.reset(new sylar::http::WSServer(process_worker, accept_worker));
        } else {
            SYLAR_LOG_ERROR(g_logger) << "invalid server type=" << conf.type
                << LexicalCast<TcpServerConf, std::string>()(conf);
            _exit(0);
        }
        std::vector<Address::ptr> failed_addrs;
        if (!server->bind(p_addrs, failed_addrs, conf.ssl)) {
            for (auto& f_addr : failed_addrs) {
                SYLAR_LOG_ERROR(g_logger) << "failed to bind address: " << *f_addr;
            }
            _exit(0); // 绑定失败，直接退出
        }
        
        if (conf.ssl) {
            if (!server->loadCertificates(conf.cert_file, conf.key_file)) {
                SYLAR_LOG_ERROR(g_logger) << "loadCertificates failure, cert_file" << conf.cert_file
                        << ", key_file=" << conf.key_file;
            }
        }
        
        if (!conf.name.empty()) {
            server->setName(conf.name);
        }
        server->setConf(conf);
        server->start();
        m_servers[conf.type].push_back(server);
    }

    for (auto& module: modules) {
        module->onServerReady();
    }
    return 0;
}

bool Application::getServer(const std::string& type, std::vector<TcpServer::ptr>& servers) {
    auto it = m_servers.find(type);
    if (it == m_servers.end()) {
        return false;
    }
    servers = it->second;
    return true;
}

}