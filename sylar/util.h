#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <iomanip>
#include <boost/lexical_cast.hpp>
#include "sylar/util/hash_util.h"
#include "sylar/util/json_util.h"


namespace sylar {

pid_t GetThreadId();

uint32_t GetFiberId();

void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1); 

std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");

// 时间 ms
uint64_t GetCurretMS();
// 时间 us
uint64_t GetCurretUS();

std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");

class FSUtil {
public:
    // 返回 path 下所有的后缀名为 subfix 的 所有文件名
    static void ListAllFile(std::vector<std::string>& files, const std::string& path, const std::string& subfix);
    static bool Mkdir(const std::string& dirname);
    static bool IsRunningPidfile(const std::string& pidfile);
    static bool Rm(const std::string& path);
    static bool Mv(const std::string& from, const std::string& to);
    static bool Realpath(const std::string& path, std::string& rpath);
    static bool Symlink(const std::string& from, const std::string& to);
    static bool Unlink(const std::string& filename, bool exist = false);
    static std::string Dirname(const std::string& filename);
    static std::string Basename(const std::string& filename);
    static bool OpenForRead(std::ifstream& ifs, const std::string& filenam, std::ios_base::openmode mode);
    static bool OpenForWrite(std::ofstream& ofs, const std::string& filenam, std::ios_base::openmode mode);

};

template <class Map, class K, class V> 
V GetParamValue(const Map& m, const K& k, const V& def = V()) {
    auto it = m.find(k);
    if (it == m.end()) {
        return def;
    }
    try {
        return boost::lexical_cast<V>(it->second);
    } catch(...) {
    }
    return def;
}

template <class Map, class K, class V>
bool CheckGetParamValue(const Map& m, const K& k, V& v) {
    auto it = m.find(k);
    if (it == m.end()) {
        return false;
    }
    try {
        v = boost::lexical_cast<V>(it->second);
        return true;
    } catch (...) {
    }
    return false;
}

class TypeUtil {
public:
    static int8_t ToChar(const std::string& str);
    static int64_t Atoi(const std::string& str);
    static double Atof(const std::string& str);
    static int8_t ToChar(const char* str);
    static int64_t Atoi(const char* str);
    static double Atof(const char* str);
};

}
#endif