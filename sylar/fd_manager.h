#ifndef __SYLAR_FD_MANAGER_H__
#define __SYLAR_FD_MANAGER_H__

#include <memory>
#include <vector>
#include "thread.h"
#include "iomanager.h"
#include "singleton.h"

namespace sylar {

// 文件句柄上下文
class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    typedef std::shared_ptr<FdCtx> ptr;
    // typedef MutexType;

    FdCtx(int fd);
    ~FdCtx();

    bool init();
    bool isInit() const { return m_isInit;}
    bool isSocket() const { return m_isSocket;}
    bool isClose() const {return m_isClosed;}
    bool close();

    void setUserNonblock(bool v) { m_userNonblock = v;}
    bool getUserNonblock() {return m_userNonblock;}

    void setSysNonblock(bool v) { m_sysNonblock = v;}
    bool getSysNonblock() {return m_sysNonblock;}

    void setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);


private:
    bool m_isInit: 1;           // 是否初始化
    bool m_isSocket: 1;         // 是否是 socket fd
    bool m_sysNonblock: 1;      // 是否系统非阻塞(hook)
    bool m_userNonblock: 1;     // 是否用户非阻塞
    bool m_isClosed: 1;         // 是否关闭
    int m_fd;                   // 文件句柄
    uint64_t m_recvTimeout;     // 接收超时时间
    uint64_t m_sendTimeout;     // 发送超时时间

};

class FdManager {
public:
    typedef RWMutex RWMuteType;
    typedef std::shared_ptr<FdManager> ptr;

    FdManager();
    FdCtx::ptr get(int fd, bool auto_create = false);
    void del(int fd);

private:
    RWMuteType m_mutex;                 // 读写锁
    std::vector<FdCtx::ptr> m_data;     // 文件句柄集合
};

typedef Singleton<FdManager> FdMgr;

}
#endif // __SYLAR_FD_MANAGER_H__