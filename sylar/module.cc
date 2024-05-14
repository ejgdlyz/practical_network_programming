#include "module.h"
#include "config.h"
#include "env.h"
#include "library.h"
#include "util.h"

namespace sylar {

static sylar::ConfigVar<std::string>::ptr g_module_path = 
        Config::Lookup("module.path", std::string("module"), "module path");

Module::Module(const std::string& name, const std::string& version
        , const std::string& filename)
    : m_name(name)
    , m_version(version)
    , m_filename(filename)
    , m_id(name + "/" + version) {
}

void Module::onBeforeArgsParse(int argc, char** argv) {
}

void  Module::onAfterArgsParse(int argc, char** argv) {
}

bool Module::onLoad() {
    return true;
}

bool Module::onUnload() {
    return true;
}

bool Module::onConnect(sylar::Stream::ptr stream) {
    return true;
}

bool Module::onDisconnect(sylar::Stream::ptr stream) {
    return true;
}

bool Module::onServerReady() {
    return true;
}

bool  Module::onServerUp() {
    return true;
}

std::string Module::statusString() {
    return "";
}


ModuleManagr::ModuleManagr() {
}

Module::ptr ModuleManagr::get(const std::string& name) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_modules.find(name);
    return it == m_modules.end() ? nullptr : it->second;
}

void ModuleManagr::add(Module::ptr m) {
    del(m->getId());
    RWMutexType::WriteLock lock(m_mutex);
    m_modules[m->getId()] = m;
}

void ModuleManagr::del(const std::string& name) {
    Module::ptr module;
    RWMutexType::WriteLock lock(m_mutex);
    auto it = m_modules.find(name);
    if (it == m_modules.end()) {
        return;
    }
    module = it->second;
    m_modules.erase(it);
    lock.unlock();
    module->onUnload();
}

void ModuleManagr::delAll() {
    RWMutexType::WriteLock lock(m_mutex);
    auto tmp = m_modules;
    lock.unlock();

    for (auto& module: tmp) {
        del(module.first);
    }
}

void ModuleManagr::init() {
    auto path = EnvMgr::GetInstance()->getAbsolutePath(g_module_path->getValue());

    std::vector<std::string> files;
    sylar::FSUtil::ListAllFile(files, path, ".so");

    std::sort(files.begin(), files.end());
    for (auto& file : files) {
        initModule(file);
    }
}

void ModuleManagr::onConnect(Stream::ptr stream) {
    std::vector<Module::ptr> modules;
    listAll(modules);

    for (auto& module : modules) {
        module->onConnect(stream);
    }
}

void ModuleManagr::onDisconnect(Stream::ptr stream) {
    std::vector<Module::ptr> modules;
    listAll(modules);

    for (auto& module : modules) {
        module->onDisconnect(stream);
    }
}

void ModuleManagr::listAll(std::vector<Module::ptr>& modules) {
    RWMutexType::ReadLock lock(m_mutex);
    for (auto& module : m_modules) {
        modules.push_back(module.second);
    }
}

void ModuleManagr::initModule(const std::string& path) {
    Module::ptr module = Library::GetModule(path);
    if (module) {
        add(module);
    }
}

}