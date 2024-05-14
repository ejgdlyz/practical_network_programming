#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__

#include <memory>
#include <set>
#include <functional>
#include <vector>
#include "thread.h"

namespace sylar {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
friend TimerManager;
public:
    typedef std::shared_ptr<Timer> ptr;

    bool cancel();
    bool refresh();
    // 重置时间
    bool reset(uint64_t ms, bool from_now);

private:
    // 通过 TimerManager 创建
    Timer(uint64_t ms, std::function<void()> cb,
            bool recurring, TimerManager* manager);
    Timer(uint64_t next);
   
private:
    bool m_recurrring = false;              // 是否是循环定时器
    uint64_t m_ms = 0;                      // 执行周期
    uint64_t m_next = 0;                    // 精确的执行时间 （循环定时器：当前时间 + 定时时间）
    std::function<void()> m_cb;             // 定时器执行的任务回调
    TimerManager* m_manager = nullptr;      // 当前 timer 属于哪个 TimerManager
    
private:
    struct Comparator {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};

class TimerManager {
friend class Timer;
public:
    typedef RWMutex RWMutexType;

    TimerManager();
    virtual ~TimerManager();

    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
    
    // 条件定时器： 传一个条件作为触发条件
    // 以一个智能指针作为条件，使用其引用计数功能
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);

    // 下一个定时器的执行时间
    uint64_t getNextTimer();

    // 已经超时需要执行的回调函数
    void listExpiredCb(std::vector<std::function<void()>>& cbs);

protected:
    // 该方法就提供了一个机会 通知 IOManager，自己唤醒自己，重设时间。
    virtual void onTimerInsertedAtFront() = 0;

    void addTimer(Timer::ptr timer, RWMutexType::WriteLock& lock);

    bool hasTimer();
private:
    // 检查服务器时间是否被修改
    bool detectedClockRollover(uint64_t now_ms);
private:
    RWMutexType m_mutex;
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    
    bool m_tickled = false;

    uint64_t m_previousTime = 0;  // 上一次执行时间
};

}

#endif  // __SYLAR_TIMER_H__