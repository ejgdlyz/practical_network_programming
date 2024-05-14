#include "thread.h"
#include "log.h"
#include "util.h"

namespace sylar {

static thread_local Thread* t_thread = nullptr;  // 指向当前线程
static thread_local std::string t_thread_name = "UNKNOW";

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");  // system 统一为系统日志

Thread* Thread::GetThis() {  // 获得当前线程引用
    return t_thread;
}

const std::string& Thread::GetName() {  // for logger   
    return t_thread_name;
}

void Thread::SetName(const std::string& name) {
    if (name.empty()) {
        return;
    }
    if (t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string& name)
        :m_cb(cb) 
        ,m_name(name) {
    if (name.empty()) {
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if (rt) {
        SYLAR_LOG_ERROR(g_logger) << "pthread_create fail, rt = " 
                << " name = " << name;
        throw std::logic_error("pthread_create error");
    }

    m_semaphore.wait();  // 确保出构造前线程已经启动
}

Thread::~Thread() {
    if (m_thread) {
        pthread_detach(m_thread);
    }
}

void Thread::join() {
    if (m_thread) {
        int rt = pthread_join(m_thread, nullptr);
        if (rt) {
            SYLAR_LOG_ERROR(g_logger) << "pthread_join fail, rt = " 
                << " name = " << m_name;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}

void* Thread::run(void* arg) {
    Thread* thread = (Thread*) arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = sylar::GetThreadId();  // 获得线程 id
    
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());  // 线程命名

    std::function<void()> cb;
    cb.swap(thread->m_cb);    // 减少一个引用，防止智能指针的引用出现在日志不会被释放

    thread->m_semaphore.notify();   // 唤醒线程

    cb();
    return 0;
 }   

}