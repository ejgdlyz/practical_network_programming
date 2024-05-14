#include "servlet.h"
#include <fnmatch.h>

namespace sylar {
namespace http {

FunctionServlet::FunctionServlet(callback cb)
    :Servlet("FunctionServlet")
    ,m_cb(cb) {
}

int32_t FunctionServlet::handle(sylar::http::HttpRequest::ptr request
        ,sylar::http::HttpResponse::ptr response
        ,sylar::http::HttpSession::ptr session) {
    return m_cb(request, response, session);
}


ServletDispatch::ServletDispatch()
    :Servlet("ServletDispatch") {
    m_default.reset(new NotFoundServlet("sylar/1.0"));
}

int32_t ServletDispatch::handle(sylar::http::HttpRequest::ptr request
        ,sylar::http::HttpResponse::ptr response
        ,sylar::http::HttpSession::ptr session) {
    
    auto slt = getMatchedServlet(request->getPath());
    if (slt) {
        slt->handle(request, response,session);
    }
    return 0;
}

// 添加到 精准匹配
void  ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt) {
    RWMutexType::WriteLock lock(m_mutex);
    m_data[uri] = slt;
}   

void  ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback cb) {
    RWMutexType::WriteLock lock(m_mutex);
    m_data[uri].reset(new FunctionServlet(cb));
}

// 添加到 模糊匹配
void  ServletDispatch::addGlobServlet(const std::string& uri, Servlet::ptr slt) {
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
    m_globs.push_back(std::make_pair(uri, slt));
}              

void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback cb) {
    addGlobServlet(uri, FunctionServlet::ptr(new FunctionServlet(cb)));
}

void ServletDispatch::delServlet(const std::string& uri) {
    RWMutexType::WriteLock lock(m_mutex);
    m_data.erase(uri);
}

void ServletDispatch::delGlobServlet(const std::string& uri) {
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            m_globs.erase(it);
            break;
        }
    }
}

// 得到 精准匹配结果
Servlet::ptr ServletDispatch::getServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_data.find(uri);
    return it == m_data.end() ? nullptr : it->second;
}

// 得到 模糊匹配结果                            
Servlet::ptr ServletDispatch::getGlobServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (it->first == uri) {
            return it->second;
        }
    }
    return nullptr;
}                        

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(m_mutex);
    auto mit = m_data.find(uri);
    if (mit != m_data.end()) {
        return mit->second;
    }   

    for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
        if (!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
            return it->second;
        }
    }
    return m_default;
}

NotFoundServlet::NotFoundServlet(const std::string& name)
    : Servlet("NotFoundServlet")
    , m_name(name) {
    m_content = "<html><head><title>404 Not Found"
            "</title></head><body><center><h1>404 Not Found</h1></center>"
            "<hr><center>" + name + "</center></body></html>";
}

int32_t NotFoundServlet::handle(sylar::http::HttpRequest::ptr request
        ,sylar::http::HttpResponse::ptr response
        ,sylar::http::HttpSession::ptr session) {
    
    response->setStatus(sylar::http::HttpStatus::NOT_FOUND);
    // response->setHeader("Server:", "sylar/1.0.0");
    response->setHeader("Content-Type:", "text/html");
    response->setBody(m_content);
    return 0;  // 正常
}

}
}