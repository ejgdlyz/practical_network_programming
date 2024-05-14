#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstdarg>
#include <map>
#include "util.h"
#include "singleton.h"
#include "thread.h"

// 流式宏
#define SYLAR_LOG_LEVEL(logger, level) \
    if (logger->getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, __FILE__, __LINE__, 0,\
        sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL) 

// format 格式宏： LogEventWrap(...): temporary object
#define SYLAR_LOG_FTM_LEVEL(logger, level, fmt, ...) \
    if (logger->getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
                __FILE__, __LINE__, 0, sylar::GetThreadId(), sylar::GetFiberId(),\
                time(0), sylar::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FTM_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FTM_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FTM_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FTM_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FTM_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)  // 查找 name 对应的 logger

namespace sylar {

class Logger;
class LogFormatter;
class LoggerManager;

// 日志级别
class LogLevel {
public:
    enum Level {  // 日志级别
        UNKNOWN = 0, 
        DEBUG = 1, 
        INFO = 2, 
        WARN = 3, 
        ERROR = 4, 
        FATAL = 5
    };
    static const char* ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string& str);
};

// 日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t line, 
        uint32_t elapse, uint32_t thread_id, uint32_t fiber_id, uint64_t time, const std::string& thread_name);

    const char* getFile() const { return m_file;}
    int32_t getLine() const { return m_line;}
    uint32_t getElapse() const { return m_elapse;}
    uint32_t getThreadId() const { return m_threadId;}
    const std::string& getThreadName() const { return m_threadName;}
    uint32_t getFiberId() const { return m_fiberId;}
    uint64_t getTime() const { return m_time;}
    std::string getContent() const { return m_ss.str();}
    
    std::stringstream& getSS() {return m_ss;}
    
    std::shared_ptr<Logger> getLogger() const { return m_logger;}
    LogLevel::Level getLevel() const { return m_level;}
    
    void format(const char* fmt, ...);  // format 方式输出日志
    void format(const char* fmt, va_list al);
private:
    const char* m_file = nullptr;       // 文件名
    int32_t m_line = 0;                 // 行号
    uint32_t m_elapse = 0;              // 程序运行的毫秒数
    uint32_t m_threadId = 0;            // 线程 id
    std::string m_threadName;           // 线程名
    uint32_t m_fiberId = 0;             // 协程 id
    uint64_t m_time;                    // 时间戳
    std::stringstream m_ss;             // 消息内容

    std::shared_ptr<Logger> m_logger;   // 析构时输出自身
    LogLevel::Level m_level;
};

// LogEventWrap 封装 LogEvent, 析构时将 event 写到 Logger 中
class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr event);
    ~LogEventWrap();
    
    std::stringstream& getSS();     // 消息内容
    LogEvent::ptr getEvent() const { return m_event;}

private:
    LogEvent::ptr m_event;
};

// 日志格式器
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern);  // 根据 pattern 来解析 FormatterItem

    // %t   %thread_id %m%n ...
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);   // 将 LogEvent format 为一个 string 提供给 LogAppender 输出
    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
public:
    class FormatterItem {
    public:
        typedef std::shared_ptr<FormatterItem> ptr;
        virtual ~FormatterItem () {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

    void init();  // pattern 解析

    bool isError() const { return m_error;}

    const std::string getPattern() const { return m_pattern;} 
private:
    std::string m_pattern;
    std::vector<FormatterItem::ptr> m_items;
    bool m_error = false;                       // 解析是否有错
};

// 日志输出地
class LogAppender {
    friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef Spinlock MutexType;

    virtual ~LogAppender() {}

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0; 

    virtual std::string toYamlString() = 0;

    void setFormatter(LogFormatter::ptr formatter);
    LogFormatter::ptr getFormatter() const;

    LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level level) {m_level = level;}
protected:
    LogLevel::Level m_level = LogLevel::DEBUG;
    LogFormatter::ptr m_formatter;
    bool m_hasFormatter = false;               // 默认没有 formatter
    mutable MutexType m_mutex;
};


// 日志输出器
class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Spinlock MutexType;

    Logger(const std::string& name = "root", LogLevel::Level level = LogLevel::DEBUG);

    void log(LogLevel::Level level, LogEvent::ptr event); 

    void debug(LogEvent::ptr event);   // 输出 debug 级别日志
    void infor(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();

    void setLevel(LogLevel::Level level) {m_level = level;}
    LogLevel::Level getLevel() const {return m_level;}

    const std::string& getName() const { return m_name;}

    void setFormatter(LogFormatter::ptr formatter);
    void setFormatter(const std::string& pattern);
    LogFormatter::ptr getFormatter();

    std::string toYamlString();
private:
    std::string m_name;                         // 日志名称
    LogLevel::Level m_level;                    // 日志级别
    std::list<LogAppender::ptr> m_appenders;    // Appender 集合
    LogFormatter::ptr m_formatter;              // 直接使用 formatter
    Logger::ptr m_root;                         // root Log

    MutexType m_mutex;
};

// 输出到控制台的 Appender
class StdoutLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
private:
};

// 输出到文件的 Appender
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override; 
    bool reopen();                      // 重新打开文件，成功返回 true
    std::string toYamlString() override;
private:
    std::string m_filename;
    std::ofstream m_filestream;
    uint64_t m_lastTime;
};

// 日志管理器：管理所有的 Logger
class LoggerManager {
public:
    typedef Spinlock MutexType;

    LoggerManager();
    Logger::ptr getLogger(const std::string& name);

    void init();                                    // 可以从配置读 logger 配置
    Logger::ptr getRoot() const { return m_root;} 
    std::string toYamlString();
private:
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;                             // 默认 logger
    MutexType m_mutex;
};

typedef sylar::Singleton<LoggerManager> LoggerMgr;
}
#endif