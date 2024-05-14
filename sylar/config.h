#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <memory>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>  // 类型转化
#include <yaml-cpp/yaml.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "sylar/log.h"
#include "sylar/thread.h"

namespace sylar {

class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    ConfigVarBase(const std::string& name, const std::string& description = "") 
        : m_name(name)
        , m_description(description) {
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);  // key 转为小写
    }
    virtual ~ConfigVarBase() {}

    const std::string& getName() const { return m_name;}
    const std::string& getDescription() const { return m_description;}

    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& val) = 0;  // 解析，初始化成员 
    virtual std::string getTypeName() const = 0;
protected:
    std::string m_name;
    std::string m_description;

};

// F from_type, T target_type， 将类型 F 转为类型 T
template<class F, class T>
class LexicalCast {
public: 
    T operator() (const F& v) {
        return boost::lexical_cast<T>(v);  // 适合基本数据类型
    }
};

// 偏特化 std::string -> std::vector<T>
template<class T>
class LexicalCast<std::string, std::vector<T>> {
public:
    std::vector<T> operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);  // string -> YAML node
        typename std::vector<T> vec; 
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {  // 为数组则解析，否则抛异常并捕获
            ss.str("");
            ss << node[i];      // YAML node -> string
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));  // 每个元素: string -> T, T 为基本数据类型
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator() (const std::vector<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);  
        for (auto& i : v) {  
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// std::string -> std::list<T>
template<class T>
class LexicalCast<std::string, std::list<T>> {
public:
    std::list<T> operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);  // string -> YAML node
        typename std::list<T> vec; 
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {  // 为数组则解析，否则抛异常并捕获
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator() (const std::list<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);  
        for (auto& i : v) {  
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// std::string -> std::set<T>
template<class T>
class LexicalCast<std::string, std::set<T>> {
public:
    std::set<T> operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);  // string -> YAML node
        typename std::set<T> vec; 
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {  // 为数组则解析，否则抛异常并捕获
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::set<T>, std::string> {
public:
    std::string operator() (const std::set<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);  
        for (auto& i : v) {  
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// std::string -> std::unordered_set<T>
template<class T>
class LexicalCast<std::string, std::unordered_set<T>> {
public:
    std::unordered_set<T> operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);  // string -> YAML node
        typename std::unordered_set<T> vec; 
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {  // 为数组则解析，否则抛异常并捕获
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator() (const std::unordered_set<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);  
        for (auto& i : v) {  
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// std::string -> std::map<std::string, T>
template<class T>
class LexicalCast<std::string, std::map<std::string, T>> {
public:
    std::map<std::string, T> operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);  // string -> YAML node
        typename std::map<std::string, T> vec; 
        std::stringstream ss;
        for (auto it = node.begin(); it != node.end(); ++it) {  
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
    std::string operator() (const std::map<std::string, T>& v) {
        YAML::Node node(YAML::NodeType::Map);  
        for (auto& i : v) {  
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// std::string -> std::unordered_map<std::string, T>
template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
public:
    std::unordered_map<std::string, T> operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);  // string -> YAML node
        typename std::unordered_map<std::string, T> vec; 
        std::stringstream ss;
        for (auto it = node.begin(); it != node.end(); ++it) {  
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));  
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
public:
    std::string operator() (const std::unordered_map<std::string, T>& v) {
        YAML::Node node(YAML::NodeType::Map);  
        for (auto& i : v) {  
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// FromStr: T operator() (const std::string&) // 将 string 转为自定义类型
// ToStr  : std::string operator(const T&)    // 将自定义类型转回 string 
template <class T, class FromStr = LexicalCast<std::string, T>
                 ,class ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    typedef RWMutex RWMutexType;
    typedef std::shared_ptr<ConfigVar> ptr; 
    typedef std::function<void(const T& old_value, const T& new_value)> on_change_cb;  // 配置变更回调函数

    ConfigVar(const std::string& name, const T& default_value, const std::string& description = "") 
        :ConfigVarBase(name, description)
        ,m_val(default_value) {

    }

    std::string toString() override {
        try {
            RWMutexType::ReadLock lock(m_mutex);
            // return boost::lexical_cast<std::string>(m_val);    // member-type is converted to string
            return ToStr()(m_val);
        } catch (std::exception& e) {
            static sylar::Logger::ptr s_logger = SYLAR_LOG_NAME("system");
            SYLAR_LOG_ERROR(s_logger) << "ConfigVar::toString exception: " 
                << e.what() << ", failed to convert: " << typeid(m_val).name() << " to string"
                << ", name=" << m_name;
        }
        return "";
    }
    
    bool fromString(const std::string& val) override {
        try {
            // m_val = boost::lexical_cast<T>(val);               // string is converted to member-type
            setValue(FromStr()(val));
            return true;
        } catch (std::exception& e) {
            static sylar::Logger::ptr s_logger = SYLAR_LOG_NAME("system");
            SYLAR_LOG_ERROR(s_logger) << "ConfigVar::fromString exception: " 
                    << e.what() << ", failed to convert: string to " << typeid(m_val).name()
                    << " name=" << m_name
                    << " - " << val;
        }
        return false;
    }

    const T getValue() const { 
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }

    void setValue(const T& v) { 
        {
            RWMutexType::ReadLock lock(m_mutex);  // 读即可
            if (v == m_val)
                return;
            
            for (auto& cb_pair: m_cbs) {
                cb_pair.second(m_val, v);  // operator()(old_val, new_value) 即调用配置变更回调函数
            }
        }
        RWMutexType::WriteLock lock(m_mutex);
        m_val = v;
    }

    std::string getTypeName() const override{ return typeid(T).name();}
    
    uint64_t addListener(on_change_cb cb) {
        static uint64_t s_fun_id = 0;
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs[++s_fun_id] = cb;
        return s_fun_id;
    }

    void delListener(uint64_t key) {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.erase(key);       
    }

    on_change_cb getListener(uint64_t key) {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    void clearListener() {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }
private:
    T m_val;
    std::map<uint64_t, on_change_cb> m_cbs;  // 变更回调函数组， uint64_t 为 key, 唯一
    mutable RWMutexType m_mutex;
};

// 管理类
class Config {
public:
    typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
    typedef RWMutex RWMutexType;

    template <class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name,     // 定义时赋值,没有则创建
        const T& default_value, const std::string& description = "") {
        
        RWMutexType::WriteLock lock(GetMutex());
        // auto tmp = Lookup<T>(name);
        auto it = GetData().find(name);
        
        if (it != GetData().end()) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>> (it->second);
            if (tmp) {
                static sylar::Logger::ptr s_logger = SYLAR_LOG_NAME("system");
                SYLAR_LOG_INFO(s_logger) << "Lookup name = " << name << " exists.";
                return tmp;
            } else {
                static sylar::Logger::ptr s_logger = SYLAR_LOG_NAME("system");
                SYLAR_LOG_ERROR(s_logger) << "Lookup name = " << name << " exists, but type is not "
                        << typeid(T).name() << ". Real type = " << it->second->getTypeName()
                        << " " << it->second->toString();
                return nullptr;
            }
        }
        
        if (name.find_first_not_of("abcdefghigklmnopqrstuvwxyz.012345678_")
                != std::string::npos) {
            static sylar::Logger::ptr s_logger = SYLAR_LOG_NAME("system");
            SYLAR_LOG_ERROR(s_logger) << "Lookuped name invalid: " << name;

            throw std::invalid_argument(name);
        }

        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetData()[name] = v;
        return v;
    }

    template <class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name ) {            // 直接查找
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetData().find(name);
        if (it == GetData().end()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);                 // ConfigVarBase -> ConfigVar<T>
    } 

    // 从文件中获取配置
    static void loadFromYaml(const YAML::Node& root);
    
    // 获取目录 path 下所有的配置文件
    static void loadFromConfDir(const std::string& path, bool force = false);


    static ConfigVarBase::ptr LookupBase(const std::string& name);

    static void Visit(std::function<void(ConfigVarBase::ptr)> cb); 
private:
    static ConfigVarMap& GetData() {

        static ConfigVarMap s_data;
        return s_data;
    }

    static RWMutexType& GetMutex () {
        static RWMutexType s_mutex;
        return s_mutex; 
    }
};
} // namespace sylar

#endif // __SYLAR_CONFIG_H__