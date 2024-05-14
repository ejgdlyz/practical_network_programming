#ifndef __SYLAR_MODULE_H__
#define __SYLAR_MODULE_H__

#include <map>
#include "sylar/stream.h"
#include "sylar/singleton.h"
#include "sylar/mutex.h"

namespace sylar {
/**
 * extern "C" {
 * Module* CreateModule() {
 *  return XX;
 * }
 * void DestoryModule(Module* ptr) {
 *  delete ptr;
 * }
 * }
 */

class Module {
public:
    typedef std::shared_ptr<Module> ptr;
    Module(const std::string& name, const std::string& version
            , const std::string& filename);
    virtual ~Module() {}

    virtual void onBeforeArgsParse(int argc, char** argv);
    virtual void onAfterArgsParse(int argc, char** argv);

    virtual bool onLoad();
    virtual bool onUnload();

    virtual bool onConnect(sylar::Stream::ptr stream);
    virtual bool onDisconnect(sylar::Stream::ptr stream);

    virtual bool onServerReady();
    virtual bool onServerUp();

    virtual std::string statusString();

    const std::string& getName() const { return m_name;}
    const std::string& getVersion() const { return m_version;}
    const std::string& getFilename() const { return m_filename;}
    const std::string& getId() const { return m_id;}

    void setFilename(const std::string& v) { m_filename = v;}
protected:
    std::string m_name;
    std::string m_version;
    std::string m_filename;
    std::string m_id;
    
};

class ModuleManagr {
public:
    typedef RWMutex RWMutexType;

    ModuleManagr();

    void add(Module::ptr m);
    void del(const std::string& name);
    void delAll();

    void init();

    Module::ptr get(const std::string& name);

    void onConnect(Stream::ptr stream);
    void onDisconnect(Stream::ptr stream);

    void listAll(std::vector<Module::ptr>& ms);

private:
    void initModule(const std::string& path);
private:
    RWMutexType m_mutex;
    std::map<std::string, Module::ptr> m_modules;
};

typedef sylar::Singleton<ModuleManagr> ModuleMgr;

}

#endif