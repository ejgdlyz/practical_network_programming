#include <atomic>
#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"

namespace sylar {

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber* t_fiber = nullptr;               // 当前执行的协程
static thread_local Fiber::ptr t_threadFiber = nullptr;     // caller 线程中的主协程

static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
        Config::Lookup("fiber.stack_size", (uint32_t)1024 * 1024, "Fiber stack size");      // 协程栈大小， 默认 1M

class MallocStackAllocate {
public:
    static void* Alloc(size_t size) {
        return malloc(size);
    }    

    static void Dealloc(void* vp, size_t size) {
        return free(vp);
    }
};

using StackAlloc = MallocStackAllocate;

Fiber::Fiber() {  
    m_state = EXEC;
    SetThis(this); 

    if (getcontext(&m_ctx)) {       // 当前 thread 的上下文赋给 main fiber
        SYLAR_ASSERT2(false, "Fiber::Fiber(): getcontext");
    }

    ++s_fiber_count;

    SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber(main), id = " << m_id << " " << this;
}

Fiber::Fiber(std::function<void()> cb, size_t stack_size, bool use_caller) 
        :m_id(++s_fiber_id)
        ,m_cb(cb) {
    ++s_fiber_count;
    m_statckSize = stack_size ? stack_size : g_fiber_stack_size->getValue();

    // 协程上下文环境初始化
    m_stack = StackAlloc::Alloc(m_statckSize);
    if (getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false, "Fiber::Fiber(cb): getcontext");
    }
    m_ctx.uc_link = nullptr;                // 当前协程结束后的返回点
    m_ctx.uc_stack.ss_sp = m_stack;         // 当前协程的栈空间起始地址
    m_ctx.uc_stack.ss_size = m_statckSize;  // 当前协程的栈空间大小

    if (!use_caller) {
        makecontext(&m_ctx, &Fiber::MainFunc, 0);   // 入口函数与当前协程对象的上下文环境 m_ctx 绑定
    }
    else {
        makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);   // 有 caller 版本
    }
    SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber (sub), id = " << m_id << " " << this;

}

Fiber::~Fiber() {
    --s_fiber_count;
    if (m_stack) {
        SYLAR_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT );
        
        // 回收栈
        StackAlloc::Dealloc(m_stack, m_statckSize);
    } else {                            // 主协程
        SYLAR_ASSERT(!m_cb);            // main_fiber's cb is null
        SYLAR_ASSERT(m_state == EXEC);  // main_fiber's state is exec
        
        Fiber* cur = t_fiber;
        if (cur == this) {
            SetThis(nullptr);  
        }
    }

    SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber, id = " << m_id;

}

void Fiber::reset(std::function<void()> cb) {
    SYLAR_ASSERT(m_stack);
    SYLAR_ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);

    m_cb = cb;
    if (getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false, "Fiber::reset: getcontext");
    }
    m_ctx.uc_link = nullptr;                
    m_ctx.uc_stack.ss_sp = m_stack;         
    m_ctx.uc_stack.ss_size = m_statckSize;  

    makecontext(&m_ctx, &Fiber::MainFunc, 0);   

    m_state = INIT;
}

void Fiber::swapIn() {
    // 目标协程唤醒到当前执行状态
    SetThis(this);
    SYLAR_ASSERT(m_state != EXEC);
    m_state = EXEC;

    if (swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {       // 调度协程切换到当前协程对象的 m_ctx 执行
        SYLAR_ASSERT2(false, "Fiber::swapIn: swapcontext");
    }
}

// 当前协程 Yield 到后台，唤醒 main 协程
void Fiber::swapOut() {
    SetThis(Scheduler::GetMainFiber());
    if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {       // 当前协程对象切回调度协程 m_ctx 执行
        SYLAR_ASSERT2(false, "Fiber::swapOut: swapcontext");
    }
}

// 强行把当前协程置换为目标协程
void Fiber::call() {
    SetThis(this);
    m_state = EXEC; 
    // SYLAR_LOG_INFO(g_logger) << getId();
    if (swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
        SYLAR_ASSERT2(false, "swapcontext");
    }
}

void Fiber::back() {
    // 不加判断的 swapOut()
    SetThis(t_threadFiber.get());
    
    if (swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {       // 与caller线程的调度协程切换
        SYLAR_ASSERT2(false, "Fiber::swapOut: swapcontext");
    }
}

// 设置当前协程
void Fiber::SetThis(Fiber* fiber) {
    t_fiber = fiber;
}

// 获得当前执行协程的引用 或者 新建一个主协程
Fiber::ptr Fiber::GetThis() {
    if (t_fiber) {
        return t_fiber->shared_from_this();  // 当前协程
    }

    Fiber::ptr main_fiber(new Fiber);
    SYLAR_ASSERT(t_fiber == main_fiber.get());  // 当前协程应为 主协程

    t_threadFiber = main_fiber;
    
    return t_fiber->shared_from_this();  
}

// 当前协程切换到后台并设置为 Ready 状态
void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur->m_state == EXEC);
    cur->m_state = READY;
    cur->swapOut();
}

// 当前协程切换到后台并设置为 Hold 状态
void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();
    SYLAR_ASSERT(cur->m_state == EXEC);
    // cur->m_state = HOLD;
    cur->swapOut();
}

uint64_t Fiber::TotalFibers() {
    return s_fiber_count;
}

// 协程入口函数封装，以在协程结束时自动 yield 回主协程。
void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis();                         // 当前子协程引用计数 + 1
    SYLAR_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }  catch (std::exception& e) {
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << e.what()
                << " fiber_id = " << cur->getId()
                << std::endl
                << sylar::BacktraceToString();
    } catch (...) {
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except."
                << " fiber_id = " << cur->getId()
                << std::endl
                << sylar::BacktraceToString();
    }

    // SYLAR_LOG_DEBUG(g_logger) << "subfiber ref_count = " << cur.use_count();

    // 需要减少智能指针 GetThis 的引用计数
    auto raw_ptr = cur.get();
    cur.reset();

    // 需要切回主协程
    raw_ptr->swapOut();

    SYLAR_ASSERT2(false, 
        "never reach fiber_id = " + std::to_string(raw_ptr->getId()));
}

void Fiber::CallerMainFunc() {
    Fiber::ptr cur = GetThis();                         // 当前子协程引用计数 + 1
    SYLAR_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }  catch (std::exception& e) {
        cur->m_state = EXCEPT;
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << e.what()
                << "fiber_id = " << cur->getId()
                << std::endl
                << sylar::BacktraceToString();
    } catch (...) {
        SYLAR_LOG_ERROR(g_logger) << "Fiber Except."
                << "fiber_id = " << cur->getId()
                << std::endl
                << sylar::BacktraceToString();
    }

    // SYLAR_LOG_DEBUG(g_logger) << "subfiber ref_count = " << cur.use_count();

    // 需要减少智能指针 GetThis 的引用计数
    auto raw_ptr = cur.get();
    cur.reset();

    // caller 线程中调度协程切回主协程(main函数)
    raw_ptr->back();

    SYLAR_ASSERT2(false, 
        "never reach fiber_id = " + std::to_string(raw_ptr->getId()));
}

uint64_t Fiber::GetFiberId() {
    if (t_fiber) {
        return t_fiber->getId();
    }
    return 0;
}

}