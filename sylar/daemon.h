#ifndef __SYLAR_DAEMON_H__
#define __SYLAR_DAEMON_H__

#include <unistd.h>
#include <functional>
#include "sylar/singleton.h"

namespace sylar {

// 进程相关信息
struct ProcessInfo {
    pid_t parent_id = 0;                // 父进程 id
    pid_t main_id = 0;                  // 主进程 id
    uint64_t parent_start_time = 0;     // 父进程启动时间
    uint64_t main_start_time = 0;       // 主进程启动时间
    uint64_t restart_count = 0;         // 主进程重启次数

    // 输出进程的信息
    std::string toString() const;
};

typedef sylar::Singleton<ProcessInfo> ProcessInfoMgr;

/**
 * @description: 是否以守护进程的方式启动程序
 *      main_cb 启动函数
 * @return {}
 */
int start_daemon(int argc, char **argv, std::function<int(int argc, char **argv)> main_cb, bool is_daemon);


}

#endif  // __SYLAR_DAEMON_H__