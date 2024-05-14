#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__

#include <ucontext.h>
#include <memory>
#include <functional>

namespace sylar {

class Scheduler;

class Fiber : public std::enable_shared_from_this<Fiber> {
friend class Scheduler;
public:
    typedef std::shared_ptr<Fiber> ptr;
    
    enum State {                        // 协程状态
        INIT, 
        HOLD,
        EXEC,
        TERM,
        READY, 
        EXCEPT
    };

private:
    /**
     * @brief 无参构造函数
     * @attention 每个线程第一个协程的构造
     */
    Fiber();

public:
    Fiber(std::function<void()> cb, size_t stack_size = 0, bool use_caller = false);
    ~Fiber();

    void reset(std::function<void()> cb);   // 重置协程（INIT、TERM）函数并重置状态
    void swapIn();                          // 切换到当前协程执行
    void swapOut();                         // 切换到后台
    
    // 强行把当前协程置换为目标协程
    void call();    
    void back();

    uint64_t getId() const { return m_id;}
    State getState() const { return m_state;}

public:
    static void SetThis(Fiber* fiber);      // 设置当前协程
    static Fiber::ptr GetThis();            // 获取当前协程
    
    // 协程切换到后台并设置为 Ready 状态或 Hold 状态
    static void YieldToReady();
    static void YieldToHold();

    static uint64_t TotalFibers();          // 总协程数

    static void MainFunc();
    static void CallerMainFunc();


    static uint64_t GetFiberId();

private:
    uint64_t m_id = 0;                  // 协程 id
    uint32_t m_statckSize = 0;          // 协程栈大小
    State m_state = INIT;               // 协程状态

    ucontext_t m_ctx;                   // 协程上下文
    void* m_stack = nullptr;            // 协程栈地址

    std::function<void()> m_cb;         // 协程入口函数
};
}

#endif // __SYLAR_FIBER_H__