#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__

#include <string>
#include "mutex.h"

namespace sylar {

class Thread : NonCopyable{
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()>cb, const std::string& name);
    ~Thread();

    const std::string& getName() const { return m_name;}
    pid_t  getId() const { return m_id;}

    void join();

    static Thread* GetThis();                       // 获得当前线程引用
    static const std::string& GetName();            // for logger
    static void SetName(const std::string& name);   // 通过线程局部变量，主线程也可以命名
private:
    Thread(const Thread&) = delete;    
    Thread(Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;

    static void* run(void* args);

private:    
    pid_t m_id = -1;                      // 线程 id
    pthread_t m_thread = 0;               // 线程结构
    std::function<void()> m_cb;           // 线程函数
    std::string m_name;                   // 线程名

    Semaphore m_semaphore;                // 信号量
};

}


#endif // __SYLAR_THREAD_H__
