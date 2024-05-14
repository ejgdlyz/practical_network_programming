#include <iostream>
#include <functional>
#include <time.h>
#include <string.h>
#include "log.h"
#include "config.h"
#include "util.h"
#include "macro.h"
#include "env.h"

namespace sylar
{

const char* LogLevel::ToString(LogLevel::Level level) {
    switch (level) {
#define XX(name) \
    case LogLevel::name: \
        return #name;  \
        break;
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, string)  \
    if (str == #string) {  \
        return LogLevel::level;  \
    }
    XX(DEBUG, debug);  // 小写
    XX(INFO, info);
    XX(WARN, warn);
    XX(ERROR, error);
    XX(FATAL, fatal);

    XX(DEBUG, DEBUG);  // 大写
    XX(INFO, INFO);
    XX(WARN, WARN);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);
    return LogLevel::UNKNOWN;
#undef XX
}

LogEventWrap::LogEventWrap(LogEvent::ptr event) 
    : m_event(event) {

}

LogEventWrap::~LogEventWrap() {
    m_event->getLogger()->log(m_event->getLevel(), m_event);  // 将自己写进 log 
}

std::stringstream& LogEventWrap::getSS() {
    return m_event->getSS();
}

void LogEvent::format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

void LogEvent::format(const char* fmt, va_list va) {
    char* buf = nullptr;
    int len = vasprintf(&buf, fmt, va);  // 根据 fmt，al 分配内存, 然后 序列化
    if (len != -1) {
        m_ss << std::string(buf, len);  // 写到 stringstream
    }
    free(buf);
}

class MessageFormatterItem : public LogFormatter::FormatterItem {
public:
    MessageFormatterItem(const std::string& str = "") {} 
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getContent();
    }
};

class LevelFormatterItem : public LogFormatter::FormatterItem {
public:
    LevelFormatterItem(const std::string& str = "") {} 
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << LogLevel::ToString(level);
    }
};

class ElapseFormatterItem : public LogFormatter::FormatterItem {
public:
    ElapseFormatterItem(const std::string& str = "") {} 
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

class NameFormatterItem : public LogFormatter::FormatterItem {
public:
    NameFormatterItem(const std::string& str = "") {} 
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLogger()->getName();  // 原始 logger
    }
};

class ThreadIdFormatterItem : public LogFormatter::FormatterItem {
public:
    ThreadIdFormatterItem(const std::string& str = "") {} 
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

class ThreadNameFormatterItem : public LogFormatter::FormatterItem {
public:
    ThreadNameFormatterItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadName();
    }
};

class FiberIdFormatterItem : public LogFormatter::FormatterItem {
public:
    FiberIdFormatterItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFiberId();
    }
};

class DateTimeFormatterItem : public LogFormatter::FormatterItem {
public:
    DateTimeFormatterItem (const std::string& format = "%Y-%m-%d %H:%M:%S")
        :m_formatter(format) 
    {
        if (m_formatter.empty()) {
            m_formatter = "%Y-%m-%d %H:%M:%S";
        }
    }
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_formatter.c_str(), &tm);
        os << buf;
    }

private:
    std::string m_formatter;
};

class FilenameFormatterItem : public LogFormatter::FormatterItem {
public:
    FilenameFormatterItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFile();
    }
};

class LineFormatterItem : public LogFormatter::FormatterItem {
public:
    LineFormatterItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLine();
    }
};

class NewLineFormatterItem : public LogFormatter::FormatterItem {
public:
    NewLineFormatterItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << std::endl;
    }
};

class TabFormatterItem : public LogFormatter::FormatterItem {
public:
    TabFormatterItem(const std::string& str = "") {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << "\t";
    }
};

class StringFormatterItem : public LogFormatter::FormatterItem {
public:
    StringFormatterItem (const std::string& str = "") 
        : m_string(str)
    {}
    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << m_string;
    }

private:
    std::string m_string;
};


LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t line, uint32_t elapse, 
                        uint32_t thread_id, uint32_t fiber_id, uint64_t time, const std::string& thread_name)
    : m_file(file)
    ,m_line(line)
    ,m_elapse(elapse)
    ,m_threadId(thread_id)
    ,m_threadName(thread_name) 
    ,m_fiberId(fiber_id)
    ,m_time(time)
    ,m_logger(logger)
    ,m_level(level) {

 }

Logger::Logger(const std::string& name, LogLevel::Level level) 
        :m_name(name), m_level(level) {
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));    // default 格式, LogFormatter::init() 解析该字符串
}

void Logger::setFormatter(LogFormatter::ptr formatter) {
    MutexType::Lock lock(m_mutex);
    m_formatter = formatter;

    for (auto& appender : m_appenders) {
        MutexType::Lock alock(appender->m_mutex);
        if (!appender->m_hasFormatter) {
            appender->m_formatter = m_formatter;
        }
    }
}

void Logger::setFormatter(const std::string& pattern) {
    LogFormatter::ptr formatter(new LogFormatter(pattern));
    if (m_formatter->isError()) {  // 有误
        std::cout << "Logger setFormatter  name = " << m_name
                    << " val = " << pattern << " invalid formatter."
                    << std::endl;
        return;
    }
    // m_formatter = formatter;
    setFormatter(formatter);
    
}   

std::string Logger::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    if (m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }

    if (m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }

    for (auto& appender : m_appenders) {
        node["appenders"].push_back(YAML::Load(appender->toYamlString()));
    }

    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFormatter::ptr Logger::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    if (!appender->getFormatter()) {
        MutexType::Lock alock(appender->m_mutex);
        appender->m_formatter = m_formatter;  // 保证每个都有 formatter(default)
        // 不改 LogAppender 标志位 m_hasFormatter
    }
    m_appenders.push_back(appender);
}
void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it)
    {
        if (*it == appender) {
            m_appenders.erase(it); 
            break;
        }
    }
}

void Logger::clearAppenders() {
    MutexType::Lock lock(m_mutex);
    m_appenders.clear();
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level)
    {
        auto self = shared_from_this();
        MutexType::Lock lock(m_mutex);
        if (!m_appenders.empty()) {
            for (auto& appender :  m_appenders) {
                appender->log(self, level, event);  // 遍历 LogAppender::log, like StdoutApppender->log()...
            }
        } else if (m_root) {
            m_root->log(level, event);
        }
        
    }
}

// 输出 debug 级别日志
void Logger::debug(LogEvent::ptr event) {
    log(LogLevel::DEBUG, event);
}
void Logger::infor(LogEvent::ptr event) {
    log(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
    log(LogLevel::WARN, event);

}
void Logger::error(LogEvent::ptr event) {
    log(LogLevel::ERROR, event);

}
void Logger::fatal(LogEvent::ptr event) {
    log(LogLevel::FATAL, event);

}

void LogAppender::setFormatter(LogFormatter::ptr formatter) {
    MutexType::Lock lock(m_mutex);
    m_formatter = formatter;
    if (m_formatter) {
        m_hasFormatter = true;
    } else {
        m_hasFormatter = false;
    }
}

LogFormatter::ptr LogAppender::getFormatter() const {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

FileLogAppender::FileLogAppender(const std::string& filename)
        : m_filename(filename) {
    reopen();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level){
        uint64_t now = event->getTime();
        if (now >= (m_lastTime + 3)) {
            reopen(); 
            m_lastTime = now; 
        }
        MutexType::Lock lock(m_mutex);
        // m_filestream << m_formatter->format(logger, level, event);
        if (!m_formatter->format(m_filestream, logger, level, event)) {
            std::cout << "FileLogAppender::log error" << std::endl;
        }
    }
}

std::string FileLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_filename;
    if (m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if (m_hasFormatter && m_formatter) {    // m_formatter 不是默认值 && 非空
        node["formatter"] = m_formatter->getPattern(); 
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

bool FileLogAppender::reopen() {
    MutexType::Lock lock(m_mutex);
    if (m_filestream) {
        m_filestream.close();
    }
    // return !!m_filestream;
    return FSUtil::OpenForWrite(m_filestream, m_filename, std::ios::app);
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        MutexType::Lock lock(m_mutex);
        // std::cout << m_formatter->format(logger, level, event);  // 得到基类 LogFormatter::format() 返回的 string
        m_formatter->format(std::cout, logger, level, event);
    }
}

std::string StdoutLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if (m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if (m_hasFormatter && m_formatter) {                // m_formatter 不是默认值 && 非空
        node["formatter"] = m_formatter->getPattern(); 
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFormatter::LogFormatter(const std::string& pattern) 
    :m_pattern(pattern) {
    init();  // 解析 pattern
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    std::stringstream ss;
    for(auto& formatter_item : m_items) {
        formatter_item->format(ss, logger, level, event);  // 解析后的各个字符对应的特定类内容，输出到字符流中, like DateTimeFormatterItem、StringFormatterItem 
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    for (auto& formatter_item : m_items) {
        formatter_item->format(ofs, logger, level, event);
    }
    return ofs;
}

// %xxx、%xxx{xxx}、 %%。 其余认为是非法的 
void LogFormatter::init() {
    // 解析 pattern: (str, format, type) 三元组格式
    std::vector<std::tuple<std::string, std::string, int> > vec;
    std::string nstr;
    for (size_t i = 0; i < m_pattern.size(); ++i) {
        if (m_pattern[i] != '%') {       // != %
            nstr.append(1, m_pattern[i]);
            continue;
        }

        if ((i + 1) < m_pattern.size() && m_pattern[i + 1] == '%') {  // %%
            nstr.append(1, '%');
            continue;
        }

        size_t n = i + 1;   // %xxx, %<>, %{}
        int fmt_status = 0;
        size_t fmt_begin = 0;

        std::string str;
        std::string fmt;

        while (n < m_pattern.size()) {  
            if (!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')) {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if (fmt_status == 0) {
                if (m_pattern[n] == '{') {  // %{...}
                    str = m_pattern.substr(i + 1, n - i - 1);
                    // std::cout << "*" << str << std::endl; 
                    fmt_status = 1;   // 解析格式
                    fmt_begin = n;
                    ++n;
                    continue;
                }
            } else if (fmt_status == 1) {   // %{...}
                if (m_pattern[n] == '}') {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    // std::cout << "#" << fmt << std::endl; 
                    fmt_status = 0;
                    ++n;
                    break;
                }
            }

            ++n;
            if (n == m_pattern.size()) {
                if (str.empty()) {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if (fmt_status == 0) {
            if (!nstr.empty()) {
                vec.push_back(std::make_tuple(nstr, std::string(), 0));
                nstr.clear();
            }
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        } else if (fmt_status == 1) {
            // std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
        }

    }
    
    if (!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, "", 0));
    }

   static std::map<std::string, std::function<FormatterItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C) \
        {#str, [](const std::string& fmt) { return FormatterItem::ptr( new C(fmt));}}

        XX(m, MessageFormatterItem),            // %m -- 消息体
        XX(p, LevelFormatterItem),              // %p -- 日志级别
        XX(r, ElapseFormatterItem),             // %r -- 启动后的时间(ms)
        XX(c, NameFormatterItem),               // %c -- 日志名称
        XX(t, ThreadIdFormatterItem),           // %t -- 线程 id
        XX(N, ThreadNameFormatterItem),         // %t -- 线程 name
        XX(n, NewLineFormatterItem),            // %n -- 回车换行
        XX(d, DateTimeFormatterItem),           // %d -- 时间
        XX(f, FilenameFormatterItem),           // %f -- 文件名
        XX(l, LineFormatterItem),               // %l -- 行号
        XX(T, TabFormatterItem),                // %T -- tab
        XX(F, FiberIdFormatterItem),            // %F -- 协程 id
#undef XX
    };

    for (auto& tuple_v : vec) {
        if (std::get<2>(tuple_v) == 0) {            // 非定义字符 like "] <"，用一个正常字符串类接收
            m_items.push_back(FormatterItem::ptr(new StringFormatterItem(std::get<0>(tuple_v))));  
        } else {                              // 自定义字符 like %m, %p, ....,                             
            auto it = s_format_items.find(std::get<0>(tuple_v));
            if (it == s_format_items.end()) {
                m_items.push_back(FormatterItem::ptr(new StringFormatterItem("<<error_format %" + std::get<0>(tuple_v) + ">>")));
                m_error = true;   // 解析失败
            
            } else {
                m_items.push_back(it->second(std::get<1>(tuple_v)));  // like,  "m" 对应的 it->second(str) 函数对象返回值 MessageFormatterItem::ptr 放入 m_items 
            }
        }

        // std::cout << "{" << std::get<0>(tuple_v) << "} - {" << std::get<1>(tuple_v) << "} - {" << std::get<2>(tuple_v) << "}" << std::endl;
    }
    
}

LoggerManager::LoggerManager() {
    m_root.reset(new Logger);  // 默认 logger
    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));  // 默认 appender
    
    m_loggers[m_root->m_name] = m_root;

    init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
    MutexType::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        return it->second;
    }

    Logger::ptr logger(new Logger(name));
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}

std::string LoggerManager::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    for (auto& p_logger : m_loggers) {      // p_logger -> pair_logger
        node.push_back(YAML::Load(p_logger.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

// 配置中设置 logger
struct LogAppenderDefine {
    int type = 0;       // 1 -> File, 2 -> Stdout
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::string file;

    bool operator== (const LogAppenderDefine& rhs) const {
        return type == rhs.type
                && level == rhs.level
                && formatter == rhs.formatter
                && file == rhs.file;
    }
};

// logger 配置自定义类型
struct LogDefine {
    std::string name;
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator== (const LogDefine& rhs) const {
        return name == rhs.name
                && level == rhs.level
                && formatter == rhs.formatter
                && appenders == rhs.appenders;
    }

    bool operator< (const LogDefine& rhs) const {
        return name < rhs.name;
    }

    bool isValid() const {
        return !name.empty();
    }
};

// string -> LogDefine
template<>
class LexicalCast<std::string, LogDefine> {
public:
    LogDefine operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);  // string -> std::set<LogDefine>
        LogDefine ld; 
       
        if (!node["name"].IsDefined())  {  // 没有 name 属性
            std::cout << "log config error: name is null, " << node << std::endl;
            throw std::logic_error("log config name is null");
        }
            
        ld.name = node["name"].as<std::string>();
        ld.level = LogLevel::FromString( node["level"].IsDefined() ? node["level"].as<std::string>() : "");
        if (node["formatter"].IsDefined()) {
            ld.formatter = node["formatter"].as<std::string>();
        }

        if (node["appenders"].IsDefined()) {                       // 解析 LogDefine 的 appenders
            for (size_t j = 0; j < node["appenders"].size(); ++j) {
                auto a = node["appenders"][j];  // LogDefine::appenders: 
                if (!a["type"].IsDefined()) {
                    std::cout << "log config error: appender:type is null, " << a << std::endl;
                    throw std::logic_error("log config appender:type is null");
                }
                std::string type = a["type"].as<std::string>();
                LogAppenderDefine lad;
                if (type == "FileLogAppender") {
                    lad.type = 1;
                    if (!a["file"].IsDefined()) {
                        std::cout << "log config error: fileappender: file is null, " << a << std::endl;
                        throw std::logic_error("log config fileappender: file is null");
                    }
                    lad.file = a["file"].as<std::string>();
                    
                    if (a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                    
                } else if (type == "StdoutLogAppender") {
                    lad.type = 2;
                    if (a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                } else {
                    std::cout << "log config error: appender:type is invalid, " << a << std::endl;
                    throw std::logic_error("log config appender:type is invalid");
                }
                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};

// LogDefine -> string
template<>
class LexicalCast<LogDefine, std::string> {
public:
    std::string operator() (const LogDefine& ld) {
        YAML::Node n;   
        n["name"] = ld.name;

        if (ld.level != LogLevel::UNKNOWN) {
            n["level"] = LogLevel::ToString(ld.level);
        }
        if (ld.formatter.empty()) {
            n["formatter"] = ld.formatter;
        }

        for (auto& appender_d: ld.appenders) {
            YAML::Node na;
            if (appender_d.type == 1) {
                na["type"] = "FileLogAppender";
                na["file"] = appender_d.file;
            } else if (appender_d.type == 2) {
                na["type"] = "StdoutLogAppender";
            }

            if (appender_d.level != LogLevel::UNKNOWN) {
                na["level"] = LogLevel::ToString(appender_d.level);
            }

            if (!ld.formatter.empty()) {
                na["formatter"] = ld.formatter;
            }

            n["appenders"].push_back(na);
        }
        std::stringstream ss;
        ss << n;
        return ss.str();
    }
};

// 全局变量 g_log_defines, main 之前
sylar::ConfigVar<std::set<LogDefine>>::ptr g_log_defines = 
        sylar::Config::Lookup("logs", std::set<LogDefine>(), "logs config");
        
struct LogIniter {
    LogIniter() {
        g_log_defines->addListener([](const std::set<LogDefine>& old_values  // 配置变化事件触发
                , const std::set<LogDefine>& new_values) {
            
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "on_logger_conf_changed";
            for (auto& new_val: new_values) {
                auto it = old_values.find(new_val);
                Logger::ptr logger;
                if (it == old_values.end()) {
                    // 新增 logger
                    // logger.reset(new sylar::Logger(new_val.name));
                    logger = SYLAR_LOG_NAME(new_val.name);  // 找到 logger

                } else {
                    if (!(new_val == *it)) {
                        // 修改 logger
                        logger = SYLAR_LOG_NAME(new_val.name);  // 找到 logger
                    } else {
                        // 相同的 logger 配置, 跳过
                        continue;
                    }
                }

                logger->setLevel(new_val.level);
                if (!new_val.formatter.empty()) {  // 不空，则覆盖  
                    logger->setFormatter(new_val.formatter);
                }

                logger->clearAppenders();
                for (auto& appender_d : new_val.appenders) {
                    LogAppender::ptr ap;

                    if (appender_d.type == 1)  {        // File
                        ap.reset(new FileLogAppender(appender_d.file));
                    } else if (appender_d.type == 2) {  // Stdout
                        if (!sylar::EnvMgr::GetInstance()->has("d")) {  // 不为守护进程
                            ap.reset(new StdoutLogAppender); 
                        } else {
                            continue;
                        }
                    }
                    ap->setLevel(appender_d.level);
                    
                    if (!appender_d.formatter.empty()) {  // appender 下的 Formatter
                        LogFormatter::ptr fmt(new LogFormatter(appender_d.formatter));
                        if (!fmt->isError()) {
                            ap->setFormatter(fmt);         // 覆盖旧的 formatter, 设置 m_hasFromatter
                        } else {
                            std::cout << "log.name = " << new_val.name << ", appender type = " << appender_d.type
                                    << ", formatter = " << appender_d.formatter << ", is invalid" << std::endl; 
                        }
                    }
                    
                    logger->addAppender(ap);
                }
            }
            
            for (auto& old_val: old_values) {
                auto it = new_values.find(old_val);
                if (it == new_values.end()) {
                    // 删除 logger
                    auto logger = SYLAR_LOG_NAME(old_val.name);
                    logger->setLevel((LogLevel::Level)100);  // 将待删除的 logger 的 level 设置很高，以模拟删除
                    logger->clearAppenders();
                }
            }
        });
    }
};

static LogIniter __log_init;  // main 之前构造

void LoggerManager::init() {
    
}

}