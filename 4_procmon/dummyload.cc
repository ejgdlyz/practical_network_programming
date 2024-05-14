#include <atomic>
#include <thread>
#include <condition_variable>

#include "sylar/mutex.h"
#include "sylar/timestamp.h"
#include "sylar/macro.h"
#include "sylar/iomanager.h"
#include "sylar/hook.h"

#include <cmath>
#include <stdio.h>

int g_cycles = 0;
int g_percent = 0;
std::atomic<bool> g_done;
std::atomic<bool> g_busy;
sylar::Semaphore g_sem;
// bool g_busy = false;
// std::mutex g_mutex;
// std::condition_variable g_cond;

double Busy(int cycles) {
    double result = 0;
    for (int i = 0; i < cycles; ++i) {
        result += sqrt(i) * sqrt(i + 1);
        result += sqrt(i + 2) * sqrt(i + 3);
    }
    return result;
}

/// 得到调用 Busy() 一次的时间
double GetSeconds(int cycles) {
    sylar::Timestamp start = sylar::Timestamp::now();
    Busy(cycles);
    return sylar::timeDifference(sylar::Timestamp::now(), start);
}

/// 查找合适的 g_cycles，使得执行一次 Busy() 应该为 1ms 左右
void FindCycles() {
    g_cycles = 1000;
    double t = GetSeconds(g_cycles);
    while (t < 0.001) {
        g_cycles = g_cycles + g_cycles / 4; // * 1.25，线性增长
        t = GetSeconds(g_cycles);
    }
    std::cout << "cycles=" << g_cycles << ", secs=" << GetSeconds(g_cycles) << std::endl;    
}

void ThreadFunc() {
    while (g_done.load() == false) {
        {
            // std::unique_lock<std::mutex> lk(g_mutex);
            while (!g_busy) {
                g_sem.wait();
                // g_cond.wait(lk, [](){ return g_busy;});
            }
            Busy(g_cycles);
        }
    }
    std::cout << "thread exit" << std::endl;
}

// this is open-loop control： 
void Load(int percent) {
    // Load 每次调用运行 1s，本身不占 CPU（大部分时间在等待），由子线程耗 CPU 时间
    percent = std::max(0, percent);
    percent = std::min(100, percent);

    // Bresenham's line algorithm
    int err = 2 * percent - 100;
    int count = 0;

    for (int i = 0; i < 100; ++i) {
        bool busy = false;
        if (err > 0) {
            busy = true;
            err += 2 * (percent - 100);
            ++count;
        } else {
            err += 2 * percent;
        }

        {   
            g_busy.exchange(busy);
            g_sem.notify();
            // std::unique_lock<std::mutex> lk(g_mutex);
            // g_busy = busy;
            // g_cond.notify_all();
        }

        // TODO 添加一个定时器来实现 如果使用非阻塞的 IOManager
        // CurrentThread::sleepUsec(10 * 1000);  // 10ms
        ::usleep_f(10 * 1000);
    }
    SYLAR_ASSERT(count == percent);
    // std::cout << "count = " << count << std::endl;
}

void Fixed() {
    while (true) {
        // 固定 CPU 使用率，一直调用 Load 即可
        Load(g_percent);
    }
}

void Cosine() {
    while (true) {
        // 200s 为周期
        for (int i = 0; i < 200; ++i) { 
            // 计算本周期的 CPU 使用率, 0-100 之间
            int percent = static_cast<int>((1.0 + cos(i * 3.14159 / 100)) / 2 * g_percent + 0.5);
            Load(percent);
        }
    }
}

/// 锯齿波
void Sawtooth() {
    while (true) {
        // 100s 为周期
        for (int i = 0; i <= 100; ++i) {
            // 100s 时间内均匀增加到预设值 g_percent
            int percent = static_cast<int>(i / 100.0 * g_percent);
            Load(percent);
        }
    }
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [fctsz] [percent] [num_threads]" << std::endl;
        return 0;
    }

    std::cout << "pid " << getpid() << std::endl;  // for procmon
    FindCycles();

    g_percent = argc > 2 ? atoi(argv[2]) : 43;
    int num_threads = argc > 3 ? atoi(argv[3]) : 1;
    // std::vector<sylar::Thread::ptr> threads;
    std::vector<std::thread> threads;
    threads.resize(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        // threads[i].reset(new sylar::Thread(ThreadFunc, "dummy_Load_" + std::to_string(i)));
        threads.push_back(std::thread(ThreadFunc));
    }
    
    // sylar::IOManager iom(num_threads, false);
    // iom.schedule(&ThreadFunc);

    switch (argv[1][0]) {
        case 'f': {
            Fixed();    // 固定的 CPU 使用率曲线，如 30%
        }
        break;

        case 'c': {
            Cosine();   // cos 曲线
        }
        break;
        
        case 'z': {
            Sawtooth(); // 锯齿波
        }
        break;

        // TODO: square and triangle waves

        default:     
        break;
    }

    g_done.exchange(true);
    {
        g_busy.exchange(true);
        g_sem.notify();
        // std::unique_lock<std::mutex> lk(g_mutex);
        // g_busy = true;
        // g_cond.notify_all();
    }

    for(int i = 0; i < num_threads; ++i) {
        // threads[i]->join();
        threads[i].join();
    }

    std::cout << "dummyload exited!" << std::endl;
}