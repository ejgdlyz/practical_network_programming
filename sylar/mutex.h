#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>

#include "noncopyable.h"
#include "fiber.h"

namespace sylar {

// 信号量
class Semaphore : NonCopyable{
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();


    void wait();
    void notify();

private:
    sem_t m_semaphore;
};

// 局部锁模板实现
template<class T>
class ScopedLockImpl {
public:
    ScopedLockImpl(T& mutex)
            :m_mutex(mutex) {
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl () {
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

// 局部读锁模板实现
template<class T>
class ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T& mutex)
            :m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }

    ~ReadScopedLockImpl () {
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

// 局部写锁模板实现
template<class T>
class WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T& mutex)
            :m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }

    ~WriteScopedLockImpl () {
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T& m_mutex;
    bool m_locked;
};

// 互斥量
class Mutex : NonCopyable {
public:
    typedef ScopedLockImpl<Mutex> Lock;

    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }
private:
    pthread_mutex_t m_mutex;
};

// 空锁: 用于调试
class NullMutex : NonCopyable {
public:
    typedef ScopedLockImpl<NullMutex> Lock;
    NullMutex() {}
    ~NullMutex() {}
    void lock() {}
    void unlock() {}
};

// 读写互斥量
class RWMutex : NonCopyable {
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;       // 局部读锁
    typedef WriteScopedLockImpl<RWMutex> WriteLock;     // 局部写锁

    RWMutex() {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&m_lock);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }
private:    
    pthread_rwlock_t m_lock;
};

// 空锁: 用于调试
class NullRWMutex : NonCopyable {
public:
    typedef ReadScopedLockImpl<NullMutex> ReadLock;         // 局部读锁
    typedef WriteScopedLockImpl<NullMutex> WriteLock;       // 局部写锁

    NullRWMutex() {}
    ~NullRWMutex() {}

    void rdlock() {}
    void wtlock() {}
    void unlock() {}

};

// 自旋锁
class Spinlock : NonCopyable {
public: 
    typedef ScopedLockImpl<Spinlock> Lock;
     
    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }

    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    void lock() {
        pthread_spin_lock(&m_mutex);
    }

    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }

private:
    pthread_spinlock_t m_mutex;
};

// 原子锁
class CASLock : NonCopyable {
public:
    typedef ScopedLockImpl<CASLock> Lock;
     
    CASLock() {
        m_mutex.clear();
    }

    ~CASLock() {
    }

    void lock() {
        while (std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:
    volatile std::atomic_flag m_mutex;
};

class Scheduler;
class FiberSemaphore : NonCopyable {
public:
    typedef Spinlock MutexType;

    FiberSemaphore(size_t inital_concurrency = 0);
    ~FiberSemaphore();

    bool tryWait();
    void wait();
    void notify();

    size_t getConcurrency() const { return m_concurrency;}
    void reset() { m_concurrency = 0;}

private:
    MutexType m_mutex;
    std::list<std::pair<Scheduler*, Fiber::ptr>> m_waiters;
    size_t m_concurrency;

};

}

#endif 