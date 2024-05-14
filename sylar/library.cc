#include "library.h"

#include <dlfcn.h>
#include "sylar/config.h"
#include "sylar/env.h"
#include "sylar/log.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

typedef Module* (*create_module)();
typedef void (*destroy_module)(Module*);

class ModuleCloser {
public:
    ModuleCloser(void* handle, destroy_module d)
        : m_handle(handle)
        , m_destroy(d) {
    }

    void operator()(Module* module) {
        std::string name = module->getName();
        std::string version = module->getVersion();
        std::string path = module->getFilename();
        m_destroy(module);
        int rt = dlclose(m_handle);
        if (rt) {
            SYLAR_LOG_ERROR(g_logger) << "dlclose handle failure, handle=" 
                    << m_handle << " name=" << name
                    << " version=" << version
                    << " path=" << path
                    << " error=" << dlerror();
        } else {
            SYLAR_LOG_INFO(g_logger) << "destroy module=" << name
                    << " version=" << version
                    << " path=" << path
                    << " handle=" << m_handle
                    << " success.";
        }
    }

private:
    void* m_handle;
    destroy_module m_destroy;
};


Module::ptr Library::GetModule(const std::string& path) {
    void* handle = dlopen(path.c_str(), RTLD_NOW);          // 加载动态链接库
    if (!handle) {
        SYLAR_LOG_ERROR(g_logger) << "cannot load library path="
            << path << " error=" << dlerror();
        return nullptr;
    }

    // 获取动态链接库中指向符号 "CreateModule" 的函数指针, 以通过 create 调用 CreateModule
    create_module create = (create_module)dlsym(handle, "CreateModule");  
    if (!create) {
        SYLAR_LOG_ERROR(g_logger) << "cannot load symbol CreateModule in" 
                << path << " error=" << dlerror();
        dlclose(handle);
        return nullptr;
    }

    destroy_module destroy = (destroy_module)dlsym(handle, "DestroyModule");
    if (!destroy) {
        SYLAR_LOG_ERROR(g_logger) << "cannot load symbol DestroyModule in" 
                << path << " error=" << dlerror();
        dlclose(handle);
        return nullptr;
    }

    Module::ptr module(create(), ModuleCloser(handle, destroy));
    module->setFilename(path);
    SYLAR_LOG_INFO(g_logger) << "load module name=" << module->getName()
            << " version=" << module->getVersion()
            << " path=" << module->getFilename()
            << " success.";
    Config::loadFromConfDir(sylar::EnvMgr::GetInstance()->getConfigPath(), true);
    return module;
}

}