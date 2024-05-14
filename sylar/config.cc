#include <sys/stat.h>
#include "config.h"
#include "env.h"
#include "util.h"
#include "log.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

// Config::ConfigVarMap Config::s_data;    

ConfigVarBase::ptr Config::LookupBase(const std::string& name) {  // 查找当前命名的项
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetData().find(name);
    return it == GetData().end() ? nullptr : it->second;
}

/*
"A.B", 10,    即 key = "A.B", value = 100, 对应的 yaml
A:
    B: 10
    C: str
*/
static void ListAllMember(const std::string& prefix,
        const YAML::Node& node, std::list<std::pair<std::string, const YAML::Node>>& output) {
    
    if (prefix.find_first_not_of("abcdefghigklmnopqrstuvwxyz.012345678_") 
            != std::string::npos) {
        SYLAR_LOG_ERROR(g_logger) << "config invalid name: " << prefix << " : " << node;
        return;
    }
    output.push_back(std::make_pair(prefix, node));  
    if (node.IsMap()) {  // map，继续遍历
        for (auto it = node.begin(); it != node.end(); ++it) {
            ListAllMember(prefix.empty() ? it->first.Scalar()
                    : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }

}

void Config::loadFromYaml(const YAML::Node& root) {
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    ListAllMember("", root, all_nodes);  // 层级结构打平

    for (auto& p_node: all_nodes) {     // p_node: pair_node 
        std::string key = p_node.first;
        if (key.empty()) {
            continue;
        }

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);

        if (var) {
            if (p_node.second.IsScalar()) {    // base type
                var->fromString(p_node.second.Scalar());
            } else {
                std::stringstream ss;
                ss << p_node.second;        // YAML node -> string
                var->fromString(ss.str());  // ConfigVar->fromString()
            }
        }

    }
}

// 记录文件的最后修改时间
static std::map<std::string, uint64_t> s_file2modifytime;
static sylar::Mutex s_mutex;

void Config::loadFromConfDir(const std::string& path, bool force) {
    // 获取 path 绝对路径
    std::string absolute_path = sylar::EnvMgr::GetInstance()->getAbsolutePath(path);
    std::vector<std::string> files;
    FSUtil::ListAllFile(files, absolute_path, ".yml");

    for (auto& file : files) {
        struct stat st;
        if (lstat(file.c_str(), &st) == 0) {
            sylar::Mutex::Lock lock(s_mutex);
            if (!force && s_file2modifytime[file] == (uint64_t)st.st_mtime) {
                continue;
            } else {
                s_file2modifytime[file] = (uint64_t)st.st_mtime;
            }
        }

        try {
            YAML::Node root = YAML::LoadFile(file);
            loadFromYaml(root);
            SYLAR_LOG_INFO(g_logger) << "LoadConfFile file=" << file << " ok.";
        }
        catch(...) {
            SYLAR_LOG_ERROR(g_logger) << "LoadConfFile file=" << file << " falied.";
        }
    }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {  // 将 s_data 以 toString 方式返回
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap& m = GetData();
    for (auto it = m.begin(); it != m.end(); ++it) {
        cb(it->second);         // cb(ConfigVar<set<LogDefine>, LecicalCast<set<LogDefine>, stirng>>)
    } 
}

}
