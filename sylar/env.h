#ifndef __SYLAR_ENV_H__
#define __SYLAR_ENV_H__

#include <map>
#include <vector>
#include <string>
#include "sylar/singleton.h"
#include "sylar/thread.h"

namespace sylar {

class Env {
public:
    typedef RWMutex RWMutexType;
    bool init(int argc, char **argv);
    
    void add(const std::string &key, const std::string& val);
    bool has(const std::string& key);
    void del(const std::string& key);
    std::string get(const std::string& key, const std::string& default_val = "");

    void addHelper(const std::string& key, const std::string& desc);
    void removeHelper(const std::string& key);
    void printHelper();

    const std::string& getExe() const { return m_exe;}
    const std::string& getCwd() const { return m_cwd;}

    bool setEnv(const std::string& key, const std::string& val);
    std::string getEnv(const std::string& key, const std::string& default_val = "") const;

    std::string getAbsolutePath(const std::string& path) const;
    std::string getConfigPath();
private:
    RWMutexType m_mutex;
    std::map<std::string, std::string> m_args;                      // 参数名: 参数值
    std::vector<std::pair<std::string, std::string>> m_helpers;     // 保存参数描述
    
    std::string m_program;
    std::string m_exe;          // 可执行程序的绝对路径, 即进程的 exe 指向的路径
    std::string m_cwd;          // m_exe 所在的目录
};

typedef sylar::Singleton<Env> EnvMgr;
}

#endif