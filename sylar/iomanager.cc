#include "iomanager.h"
#include "macro.h"
#include "log.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace sylar {

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

// 根据事件类型获取对应的事件类
IOManager::FdContext::EventContext& IOManager::FdContext::getContext(IOManager::Event event) {
    switch (event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            SYLAR_ASSERT2(false, "getContext");
    }
    throw std::invalid_argument("getContext invalid event");
}

// 取消事件 
void IOManager::FdContext::resetContext(EventContext& evt_ctx) {
    evt_ctx.scheduler = nullptr;
    evt_ctx.fiber.reset();
    evt_ctx.cb = nullptr;
}   

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
    SYLAR_ASSERT(events & event);
    events = (Event)(events & ~event);  // 删除 event
    EventContext& ctx = getContext(event);
    if (ctx.cb) {
        ctx.scheduler->schedule(&ctx.cb);  // 使用 & 方式会 swap 掉
    } else {
        ctx.scheduler->schedule(&ctx.fiber);
    }

    ctx.scheduler = nullptr;
}

IOManager::IOManager(size_t thread_size, bool use_caller, const std::string& name) 
        : Scheduler(thread_size, use_caller, name) {
    
    m_epollfd = epoll_create(5000);
    SYLAR_ASSERT(m_epollfd > 0);

    int rt = pipe(m_tickleFds);
    SYLAR_ASSERT(!rt);

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;  // 边缘触发
    event.data.fd = m_tickleFds[0];
    
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);  // 设置 m_tickleFds[0] 为非阻塞
    SYLAR_ASSERT(!rt);

    rt = epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    SYLAR_ASSERT(!rt);

    // m_fdContexts.resize(64);
    contextResize(32);

    start();  // 默认启动 scheduer->start()
}

IOManager::~IOManager() {
    stop();                 // 父类 stop, scheduler::run()
    close(m_epollfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (m_fdContexts[i]) {
            delete m_fdContexts[i];
            m_fdContexts[i] = nullptr;
        }
    }
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);
    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    // 获取 fd 对应的 FdContext, 不存在则分配
    FdContext* fd_context = nullptr;
    RWMutexType::ReadLock rd_lock(m_mutex);
    if ((int)m_fdContexts.size() > fd) {
        fd_context = m_fdContexts[fd];  // sock <-> fd_context
        rd_lock.unlock();
    } else {
        rd_lock.unlock();
        RWMutexType::WriteLock wr_lock(m_mutex);
        contextResize(fd * 1.5);   // 1.5 倍扩容
        fd_context = m_fdContexts[fd];
    }

    // 设置 Fd 上下文的状态
    FdContext::MutexType::Lock lock(fd_context->mutex);
    if (SYLAR_UNLIKELY(fd_context->events & event)) {      // 同一类型事件加过了
        SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd = " << fd
                << " evnet = " << event
                << " fd_context.event = " << fd_context->events;
        SYLAR_ASSERT(!(fd_context->events & event));
    }

    int op = fd_context->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_context->events | event;  // 新的 event
    epevent.data.ptr = fd_context;                          // fd_context 作为 event.data.ptr 的内容

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);    // 将 listened/connected fd(sock) 添加到 m_epollfd 中
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl (" << m_epollfd << ", "
                << op << ", " << fd << ", " << epevent.events << ")"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return -1;  // -1 fail
    }

    ++m_pendingEventCount;  // 待执行的 IO 事件数 + 1

    // 获取 fd 的 event 事件对应的 EventContext, 对其属性 scheduler, cb, fiber 进行赋值
    fd_context->events = (Event)(fd_context->events | event);
    FdContext::EventContext& event_ctx = fd_context->getContext(event);
    SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.fiber
            && !event_ctx.cb);

    event_ctx.scheduler = Scheduler::GetThis();
    if (cb) {
        event_ctx.cb.swap(cb);
    } else {
        event_ctx.fiber = Fiber::GetThis();  // 回调函数为空，则将当前协程作为回调执行体
        SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }
    
    return 0;  // 0 sucess, -1 fail
}

bool IOManager::delEvent(int fd, Event event) {
    // 获取 fd 对应的 FdContext
    RWMutexType::ReadLock rd_lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    rd_lock.unlock();

    FdContext::MutexType::Lock lock(fd_ctx->mutex);
    if (SYLAR_UNLIKELY(!(fd_ctx->events & event))) { // 没有此事件
        return false;
    }
    
    Event new_events = (Event)(fd_ctx->events & ~event);  // 将 event 从 fd_ctx->events 中去掉
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;  // 数据指针给 fd_ctx

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);  // 修改事件
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl (" << m_epollfd << ", "
                << op << ", " << fd << ", " << epevent.events << ")"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);  // 清理里面的协程对象、回调事件、scheduer等
    return true;

}

bool IOManager::cancelEvent(int fd, Event event) { // 取消事件, 找到事件，强制触发执行
    RWMutexType::ReadLock rd_lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    rd_lock.unlock();

    FdContext::MutexType::Lock lock(fd_ctx->mutex);
    if (SYLAR_UNLIKELY(!(fd_ctx->events & event))) { // 没有此事件
        return false;
    }
    
    Event new_events = (Event)(fd_ctx->events & ~event);  // 将 event 从 fd_ctx->events 中去掉
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;  // 数据指针给 fd_ctx

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);  // 修改 m_epollfd 中的 fd 描述符
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl (" << m_epollfd << ", "
                << op << ", " << fd << ", " << epevent.events << ")"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    fd_ctx->triggerEvent(event); // 将找到的事件 tigger 一下
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd) {                // 取消 fd 上所有事件
    RWMutexType::ReadLock rd_lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    rd_lock.unlock();

    FdContext::MutexType::Lock lock(fd_ctx->mutex);
    if (!fd_ctx->events) { // 没有此事件
        return false;
    }
    
    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;  // 数据指针给 fd_ctx

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);  // 修改事件
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl (" << m_epollfd << ", "
                << op << ", " << fd << ", " << epevent.events << ")"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    if (fd_ctx->events & READ) {  // 读事件
        fd_ctx->triggerEvent(READ); // 将找到的事件 tigger 一下
        --m_pendingEventCount;
    }
    if (fd_ctx->events & WRITE) {  // 写事件
        fd_ctx->triggerEvent(WRITE); // 将找到的事件 tigger 一下
        --m_pendingEventCount;

    }
    SYLAR_ASSERT(fd_ctx->events == 0);
    return true;
}

// 获取当前的 IOManager
IOManager* IOManager::GetThis() {        
    return dynamic_cast<IOManager*> (Scheduler::GetThis());
}

// @override
void IOManager::tickle() {
    if (!hasIdleThreads()) {
        return;   
    }
    // SYLAR_LOG_INFO(g_logger) << "IOManager::tick()";

    int rt = write(m_tickleFds[1], "T", 1);
    SYLAR_ASSERT(rt == 1);
}

// for idle()
bool IOManager::stopping(uint64_t& timeout) {
    timeout = getNextTimer();
    return timeout == ~0ull 
            && m_pendingEventCount == 0
            && Scheduler::stopping();
}

// @override, for schedule
bool IOManager::stopping() {
    uint64_t timeout;
    return stopping(timeout);
            
}

// @override, 线程空闲时，陷入 idle, 即 epoll_wait
void IOManager::idle() {
    SYLAR_LOG_DEBUG(g_logger) << "idle";
    const uint64_t MAX_EVNETS = 256;
    epoll_event* events = new epoll_event[MAX_EVNETS]();  // 保存发生事件的 fd 结构体集合
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){ delete[] ptr;});

    while (true) {
        uint64_t next_timeout = 0;      // 堆顶定时器过期剩余时间
        if (SYLAR_UNLIKELY(stopping(next_timeout))) {
            SYLAR_LOG_INFO(g_logger) << "name = " << getName() 
                    << " idle stopping exit";
            break;  
            // idle_fiber: TREM
        }

        int rt = 0;
        do {
            static const int MAX_TIMEOUT = 3000; // 3s
            if (next_timeout != ~0ull) {
                next_timeout = std::min((int)next_timeout, MAX_TIMEOUT);
            } else {
                next_timeout = MAX_TIMEOUT;
            }
            rt = epoll_wait(m_epollfd, events, MAX_EVNETS, (int)next_timeout);

            if (rt < 0 && errno == EINTR) {
                continue;
            } else { 
                // SYLAR_LOG_INFO(g_logger) << "IOManager::wait()";
                break;
            }

        } while(true);

        // 检查定时器, 满足条件的回调
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if (!cbs.empty()) {
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        // 处理所有的事件
        for (int i = 0; i < rt; ++i) {
            epoll_event& event = events[i];
            if (event.data.fd == m_tickleFds[0]) {  // tick() 的消息无意义，跳过
                uint8_t dummy[256];
                while (read(m_tickleFds[0], dummy, sizeof(dummy)) > 0);
                continue;
            }

            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            if (event.events & (EPOLLERR | EPOLLHUP))  {  // 该事件为 epoll 错误或者中断
                event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }
            int real_events = NONE;
            if (event.events & EPOLLIN) {  // 读事件
                real_events |= READ;
            } 
            if (event.events & EPOLLOUT) {  // 写事件
                real_events |= WRITE;
            } 

            if ((fd_ctx->events & real_events) == NONE) {  // 无事件
                continue;
            }

            int left_events = (fd_ctx->events & ~real_events);  // 剩余事件
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | left_events;  // 修改为剩余事件

            int rt2 = epoll_ctl(m_epollfd, op, fd_ctx->fd, &event);  // 将剩余事件放入 epoll
            if (rt2) {
                SYLAR_LOG_ERROR(g_logger) << "epoll_ctl (" << m_epollfd << ", "
                        << op << ", " << fd_ctx->fd << ", " << event.events << ")"
                        << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }            

            // 触发事件
            
            if (fd_ctx->events & READ) {  // 读事件 
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }
            
            if (fd_ctx->events & WRITE) {  // 写事件 
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            } 
        }

        // 让出执行权，return back to run()::idle_fiber->swapIn() next;
        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        raw_ptr->swapOut(); 
    }
    
} 

void IOManager::onTimerInsertedAtFront() {
    tickle();

}

}