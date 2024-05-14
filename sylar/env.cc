#include <string.h>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <stdlib.h>
#include "env.h"
#include "log.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

bool Env::init(int argc, char **argv) {
    char link[1024] = {0}; // 链接路径
    char path[1024] = {0}; // 真正路径
    sprintf(link, "/proc/%d/exe", getpid());
    // 得到链接指向的真正路径 (绝对路径)
    ssize_t rt = readlink(link, path, sizeof(path));
    if (rt == -1) {
        return false;
    }
    m_exe= path;  // "/proc/xxx/exe"
    auto pos = m_exe.find_last_of("/");  // 从尾部开始找 '/'
    m_cwd = m_exe.substr(0, pos) + "/"; // "/proc/xxx/"


    m_program = argv[0]; // 程序名

    // -config /path/to/config -file xxxx -d
    const char* now_key = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (strlen(argv[i]) > 1) {
                if (now_key) {
                    add(now_key, "");  // 不带参数的 key, 如 -d 启动守护线程，其后不加参数
                }
                now_key = argv[i] + 1;
            } else {
                SYLAR_LOG_ERROR(g_logger) << "invalid arg idx=" << i << " val=" << argv[i];
                return false;
            }
        } else {
            if (now_key) {
                add(now_key, argv[i]);
                now_key = nullptr;
            } else {
                SYLAR_LOG_ERROR(g_logger) << "invalid arg idx=" << i << " val=" << argv[i];
                return false;
            }
        }
    }
    if (now_key) {
        add(now_key, "");  // 最后的参数只有一个 key, 无 val
    }
    return true;
}

void Env::add(const std::string &key, const std::string& val) {
    RWMutexType::WriteLock lock(m_mutex);
    m_args[key] = val;
}

bool Env::has(const std::string& key) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return it != m_args.end();
}

void Env::del(const std::string& key) {
    RWMutexType::WriteLock lock(m_mutex);
    m_args.erase(key);
}

std::string Env::get(const std::string& key, const std::string& default_val) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return it != m_args.end() ? it->second : default_val;
}

void Env::addHelper(const std::string& key, const std::string& desc) {
    removeHelper(key);
    RWMutexType::WriteLock lock(m_mutex);
    m_helpers.push_back(std::make_pair(key, desc)); 
}

void Env::removeHelper(const std::string& key) {
    RWMutexType::WriteLock lock(m_mutex);
    
    auto it = m_helpers.begin(); 
    while (it != m_helpers.end()) {
        if (it->first == key) {
            it = m_helpers.erase(it);
        } else {
            ++it;
        }
    }
}

// 输出使用的正确方式
void Env::printHelper() {
    RWMutexType::ReadLock lock(m_mutex);
    std::cout << "Usage: " << m_program << " [options]" << std::endl;
    for (auto& helper: m_helpers) {
        std::cout << std::setw(5) << "-" << helper.first << " : " << helper.second << std::endl;
    }
}

bool Env::setEnv(const std::string& key, const std::string& val) {
    return !setenv(key.c_str(), val.c_str(), 1);
}

std::string Env::getEnv(const std::string& key, const std::string& default_val) const {
    const char* v = getenv(key.c_str());
    if (v == NULL) {
        return default_val;
    }
    return v;
}

std::string Env::getAbsolutePath(const std::string& path) const {
    if (path.empty()) {
        return "/";
    }
    if (path[0] == '/') {
        return path;
    }
    return m_cwd + path;
}

std::string Env::getConfigPath() {
    return getAbsolutePath(get("c", "conf"));
}

}