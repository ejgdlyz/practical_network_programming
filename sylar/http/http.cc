#include "http.h"

namespace sylar {
namespace http {

HttpMethod StringToHttpMethod(const std::string& m) {
#define XX(num, name, string) \
    if (strcmp(#string, m.c_str()) == 0) { \
        return HttpMethod::name; \
    }
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVAILD_METHOD;
}

HttpMethod CharsToHttpMethod(const char* m) {
#define XX(num, name, string) \
    if (strncmp(#string, m, strlen(#string)) == 0) { \
        return HttpMethod::name; \
    }
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVAILD_METHOD;  
}

static const char* s_method_string[] = {
#define XX(num, name, string) #string, 
    HTTP_METHOD_MAP(XX)
#undef XX
};

const char* HttpMethodToString(const HttpMethod& m) {
    uint32_t idx = (int)m;
    if (idx >= (sizeof(s_method_string) / sizeof(s_method_string[0]))) {
        return "<Unknown>";
    }
    return s_method_string[idx];
}

const char* HttpStatusToString(const HttpStatus& s) {
    switch(s) {
#define XX(code, name, msg) \
        case HttpStatus::name: \
            return #msg;
        HTTP_STATUS_MAP(XX);
#undef XX       
        default:
            return "<Unknown>";
    }
}

bool CaseInsensitiveLess::operator() (const std::string& lhs, const std::string& rhs) const {
    return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;  // 不区分大小写比较
}

HttpRequest::HttpRequest(uint8_t version, bool close) 
    :m_method(HttpMethod::GET)
    ,m_version(version)
    ,m_close(close)
    ,m_websocket(false)
    ,m_path("/"){
}

std::shared_ptr<HttpResponse> HttpRequest::createResponse() {
    HttpResponse::ptr rsp(new HttpResponse(getVersion()
                            ,isClose()));
    return rsp;
}

// 从 Headers 取数据
std::string HttpRequest::getHeader(const std::string& key, const std::string& def) const {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
}

std::string HttpRequest::getParam(const std::string& key, const std::string& def) const {
    auto it = m_params.find(key);
    return it == m_headers.end() ? def : it->second;
}

std::string HttpRequest::getCookie(const std::string& key, const std::string& def) const {
    auto it = m_cookies.find(key);
    return it == m_headers.end() ? def : it->second;
}

// 从 Headers 取数据
void HttpRequest::setHeader(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}

void HttpRequest::setParam(const std::string& key, const std::string& val) {
    m_params[key] = val;

}

void HttpRequest::setCookie(const std::string& key, const std::string& val) {
    m_cookies[key] = val;

}

void HttpRequest::delHeader(const std::string& key) {
    m_headers.erase(key);
}

void HttpRequest::delParam(const std::string& key) {
    m_params.erase(key);
}

void HttpRequest::delCookie(const std::string& key) {
    m_cookies.erase(key);
}

bool HttpRequest::hasHeader(const std::string& key, std::string* val) const {
    auto it = m_headers.find(key);
    if (it == m_headers.end()) {
        return false;
    }
    if (val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasParam(const std::string& key, std::string* val) const {
    auto it = m_params.find(key);
    if (it == m_params.end()) {
        return false;
    }
    if (val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasCookie(const std::string& key, std::string* val) const {
    auto it = m_cookies.find(key);
    if (it == m_cookies.end()) {
        return false;
    }
    if (val) {
        *val = it->second;
    }
    return true;
}

// 序列化
std::ostream& HttpRequest::dump(std::ostream& os) const{
    // GET /uri HTTP/1.1
    // host: www.sylar.top
    //
    //

    // 按照上述格式进行拼接
    os << HttpMethodToString(m_method) << " " << m_path << (m_query.empty() ? "" : "?") << m_query
            <<(m_fragment.empty() ? "" : "#") << m_fragment
            << " HTTP/" << ((uint32_t)(m_version >> 4)) << "." << ((uint32_t)(m_version & 0x0F))
            << "\r\n";

    if (!m_websocket) {
        os << "connect: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    for (auto& header : m_headers) {
        if (!m_websocket && strcasecmp(header.first.c_str(), "connection") == 0) {
            continue;
        }
        os << header.first << ": " << header.second << "\r\n";
    }

    if (!m_body.empty()) {
        os << "content-length: " << m_body.size() << "\r\n\r\n" << m_body;
    } else {
        os << "\r\n";
    }

    return os;
}

std::string HttpRequest::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

void HttpRequest::init() {
    std::string conn = getHeader("connection");
    if(!conn.empty()) {
        if(strcasecmp(conn.c_str(), "keep-alive") == 0) {
            m_close = false;
        } else {
            m_close = true;
        }
    }
}

HttpResponse::HttpResponse(uint8_t version, bool close)
    :m_status(HttpStatus::OK)
    ,m_version(version)
    ,m_close(close)
    ,m_websocket(false) {
}

// 从 Headers 取数据
std::string HttpResponse::getHeader(const std::string& key, const std::string& def) const {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
} 

void HttpResponse::setHeader(const std::string& key, const std::string& v) {
    m_headers[key] = v;
}

void HttpResponse::delHeader(const std::string& key) {
    m_headers.erase(key);
}

std::ostream& HttpResponse::dump(std::ostream& os) const {
    os << "HTTP/" << ((uint32_t)m_version >> 4) << "." << ((uint32_t)m_version & 0x0F) << " " 
        << (uint32_t)m_status << " " <<  (m_reason.empty() ? HttpStatusToString(m_status) : m_reason) << "\r\n";

    for (auto& header : m_headers) {
        if (!m_websocket && strcasecmp(header.first.c_str(), "connection") == 0) {
            continue;
        }
        os << header.first << ": " << header.second << "\r\n";
    }

    if (!m_websocket) {
        os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    if (!m_body.empty()) {
        os << "content-length: " << m_body.size() << "\r\n\r\n" 
            << m_body;
    } else {
        os << "\r\n";
    }
    return os;
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const HttpRequest& req) {
    return req.dump(os);
}

std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp) {
    return rsp.dump(os);
}

}
}