#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include "daemon.h"
#include "log.h"
#include "config.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static sylar::ConfigVar<uint32_t>::ptr g_daemon_restart_interval = 
        sylar::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");  // 重启时间间隔

std::string ProcessInfo::toString() const {
    std::stringstream ss;
    ss << "[ProcessInfo parent_id=" <<parent_id << " main_id=" << main_id
            << " parent_start_time=" << Time2Str(parent_start_time) 
            << " main_start_time=" << Time2Str(main_start_time)
            << " restart_count=" << restart_count << "]";
    return ss.str();
}

static int real_start(int argc, char **argv
        , std::function<int(int argc, char **argv)> main_cb) {
    return main_cb(argc, argv);
}

static int real_daemeon(int argc, char **argv
        , std::function<int(int argc, char **argv)> main_cb) {
    int rt = daemon(1, 0);
    if (rt == -1) {
        return -1;
    }

    ProcessInfoMgr::GetInstance()->parent_id = getpid();        // 父进程当前进程 id
    ProcessInfoMgr::GetInstance()->parent_start_time = time(0);  // 父进程当前时间

    while (true) {
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程
            ProcessInfoMgr::GetInstance()->main_id = getpid();        // 子进程当前进程 id
            ProcessInfoMgr::GetInstance()->main_start_time = time(0);  // 子进程当前时间
            SYLAR_LOG_INFO(g_logger) << "process start: pid=" << getpid();
            return real_start(argc, argv, main_cb);
        } else if (pid < 0) {
            SYLAR_LOG_ERROR(g_logger) << "fork failure pid=" << pid << 
                    " errno=" << errno << " errstr=" << strerror(errno);
            return -1;
        } else {
            // 父进程
            int status = 0;
            waitpid(pid, &status, 0);
            if (status) {  
                // 子进程异常
                SYLAR_LOG_ERROR(g_logger) << "child crash pid=" << pid 
                        << " status=" << status;
            } else {    
                // 子进程正常结束
                SYLAR_LOG_ERROR(g_logger) << "child finished pid=" << pid; 
                break;
            }
            ProcessInfoMgr::GetInstance()->restart_count += 1;
            sleep(g_daemon_restart_interval->getValue());  // 等待资源释放
        }
    }
    return 0;
}

int start_daemon(int argc, char **argv
        , std::function<int(int argc, char **argv)> main_cb, bool is_daemon) {
    if (!is_daemon) {
        return real_start(argc, argv, main_cb);
    }

    return real_daemeon(argc, argv, main_cb);
}

}
