#include "timer.h"
#include "util.h"

namespace sylar {

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    if (!lhs && !rhs) {
        return false;
    }
    if (!lhs) {
        return true;
    }
    if (!rhs) {
        return false;
    }
    if (lhs->m_next < rhs->m_next) { // 左边的执行时间 < 右边执行时间
        return true;
    }
    if (rhs->m_next < lhs->m_next) {
        return false;
    }
    return lhs.get() < rhs.get();
}


// 通过 TimerManager 创建
Timer::Timer(uint64_t ms, std::function<void()> cb,
            bool recurring, TimerManager* manager) 
    :m_recurrring(recurring)
    ,m_ms(ms)
    ,m_cb(cb)
    ,m_manager(manager) {
    
    m_next = sylar::GetCurretMS() + m_ms;  // 绝对时间点

}

Timer::Timer(uint64_t next) : m_next(next) {
}


bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (m_cb) {
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

// 重设一个时间，以当前时间开始计算
bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (!m_cb) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    // 先删除再添加
    m_manager->m_timers.erase(it);
    m_next = sylar::GetCurretMS() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

// 重置时间
bool Timer::reset(uint64_t ms, bool from_now) {
    if (ms == m_ms && !from_now) {
        return true;
    }
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (!m_cb) {
        return false;
    }

    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    // 先删除再添加
    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    if (from_now) {
        start = sylar::GetCurretMS();
    } else {
        start = m_next - m_ms;  // 旧定时器开始执行的时刻
    }
    m_ms = ms;
    m_next = start + m_ms;   // 旧时刻 + 新间隔 = 新的绝对时间点
    m_manager->addTimer(shared_from_this(), lock);

    return true;
}

TimerManager::TimerManager() {
    m_previousTime = sylar::GetCurretMS();
}

TimerManager::~TimerManager() {

}

Timer::ptr TimerManager::addTimer(uint64_t ms, 
        std::function<void()> cb, bool recurring) {

    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutexType::WriteLock lock(m_mutex);
    // auto it = m_timers.insert(timer).first;
    // bool at_front = (it == m_timers.begin());       // 定时器最小
    // lock.unlock();

    // if (at_front) {
    //     onTimerInsertedAtFront();
    // }
    addTimer(timer, lock);
    return timer;
}

static void onTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    std::shared_ptr<void> tmp = weak_cond.lock();
    if (tmp) {
        cb();
    }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, 
        std::weak_ptr<void> weak_cond, bool recurring) {
    return addTimer(ms, std::bind(onTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::getNextTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    m_tickled = false; // 说明需要重新调用epoll_wait，清除标志
    if (m_timers.empty()) {
        return ~0ull;  // 最大值
    }

    const Timer::ptr& next = *m_timers.begin();
    uint64_t now_ms = sylar::GetCurretMS();

    if (now_ms >= next->m_next) {
        return 0;   // 晚了，立即执行
    } else {
        return next->m_next - now_ms;  // 还需等待的时间
    }

}

// 返回出来，放到 schedule 中去执行
void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
    uint64_t now_ms =sylar::GetCurretMS();
    std::vector<Timer::ptr> expired;  // 已经超时的定时器

    {
        RWMutexType::ReadLock lock(m_mutex);
        if (m_timers.empty()) {
            return;
        }
    }

    RWMutexType::WriteLock lock(m_mutex);
    if (m_timers.empty()) {
        return;
    }
    
    bool rollover = detectedClockRollover(now_ms);
    if (!rollover && (*m_timers.begin())->m_next > now_ms) {
        return;
    }

    Timer::ptr now_timer(new Timer(now_ms));
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
    while (it != m_timers.end() && (*it)->m_next == now_ms) {
        ++it;
    }

    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    cbs.reserve(expired.size());

    for (auto& timer: expired) {
        cbs.push_back(timer->m_cb);
        if (timer->m_recurrring) {
            // 循环定时器，重置时间
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        } else {
            timer->m_cb = nullptr;
        }
    }
}

void TimerManager::addTimer(Timer::ptr timer, RWMutexType::WriteLock& lock) {
    auto it = m_timers.insert(timer).first;
    bool at_front = (it == m_timers.begin()) && !m_tickled;
    if (at_front) {
        m_tickled = true;  // 防止频繁修改 onTimerInsertedAtFront
    }
    lock.unlock();

    if (at_front) {
        onTimerInsertedAtFront();
    }
}

bool TimerManager::detectedClockRollover(uint64_t now_ms) {
    bool rollover = false;
    if (now_ms < m_previousTime && now_ms < (m_previousTime - 60 * 60 * 1000)) {
        rollover = true;
    }

    m_previousTime = now_ms;
    return rollover;
}

bool TimerManager::hasTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}


} // namespace sylar



