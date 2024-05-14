#include "ws_connection.h"
#include "sylar/util/hash_util.h"

namespace sylar {
namespace http {

WSConnection::WSConnection(Socket::ptr sock, bool owner)
    :HttpConnection(sock, owner) {
}

std::pair<HttpResult::ptr, WSConnection::ptr> WSConnection::Create(const std::string& url
        ,uint64_t timeout_ms, const std::map<std::string, std::string>& headers) {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::INVALID_URL, 
                nullptr, "invalid url: " + url), nullptr);
    }
    return Create(uri, timeout_ms, headers);
}

std::pair<HttpResult::ptr, WSConnection::ptr> WSConnection::Create(Uri::ptr uri
        ,uint64_t timeout_ms, const std::map<std::string, std::string>& headers) {
    Address::ptr addr = uri->createAddress();
    if (!addr) {
        return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::INVALID_HOST, 
                nullptr, "invalid host: " + uri->getHost()), nullptr);
    }
    Socket::ptr sock = Socket::CreateTCP(addr);
    if (!sock) {
        return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::CREATE_SOCKET_ERROR
                , nullptr, "Create socket failure: " + addr->toString() 
                + " errno=" + std::to_string(errno) + " errstr=" + std::string(strerror(errno))), nullptr);
    }
    if (!sock->connect(addr)) {
        return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::CONNECT_FAILURE
                , nullptr, "connect failure: " + addr->toString()), nullptr);
    }
    sock->setRecvTimeout(timeout_ms);
    WSConnection::ptr conn = std::make_shared<WSConnection>(sock);

    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    req->setPath(uri->getPath());
    req->setQuery(uri->getQuery());
    req->setFragment(uri->getFragment());
    req->setMethod(HttpMethod::GET);
    bool has_host = false;
    bool has_conn = false;
    for (auto& header : headers) {
        if (strcasecmp(header.first.c_str(), "connection") == 0) {
            has_conn = true;
        } else if (!has_conn && strcasecmp(header.first.c_str(), "host") == 0) {
            has_host = !header.second.empty();
        }
        req->setHeader(header.first, header.second);
    }
    req->setWebsocket(true);
    if (!has_conn) {
        req->setHeader("connection", "Upgrade");
    }
    req->setHeader("Upgrade", "websocket");
    req->setHeader("Sec-websocket-Version", "13");
    req->setHeader("Sec-websocket-Key", sylar::base64encode(random_string(16)));
    if (!has_host) {
        req->setHeader("Host", uri->getHost());
    }

    int rt = conn->sendRequest(req);
    if(rt == 0) {
        return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::SEND_CLOSE_BY_PEER
                , nullptr, "send request closed by peer: " + addr->toString()), nullptr);
    }
    if(rt < 0) {
        return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::SEND_SOCKET_ERROR
                , nullptr, "send request socket error errno=" + std::to_string(errno)
                + " errstr=" + std::string(strerror(errno))), nullptr);
    }
    auto rsp = conn->recvResponse();
    if (!rsp) {
        return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::TIMEOUT
                , nullptr, "recv response timeout: " + addr->toString()
                + " timeout_ms:" + std::to_string(timeout_ms)), nullptr); 
    }

    if (rsp->getStatus() != HttpStatus::SWITCHING_PROTOCOLS) {
        return std::make_pair(std::make_shared<HttpResult>(50
                    , rsp, "not websocket server " + addr->toString()), nullptr);
    }
    return std::make_pair(std::make_shared<HttpResult>((int)HttpResult::Status::OK
                , rsp, "ok"), conn);
}

WSFrameMessage::ptr WSConnection::recvMessage() {
    return WSRecvMessage(this, true);
}

int32_t WSConnection::sendMessage(WSFrameMessage::ptr msg, bool fin) {
    return WSSendMessage(this, msg, true, fin);
}

int32_t WSConnection::sendMessage(const std::string& msg, int32_t opcode, bool fin) {
    return WSSendMessage(this, std::make_shared<WSFrameMessage>(opcode, msg), true, fin);
}

int32_t WSConnection::ping() {
    return WSPing(this);
}

int32_t WSConnection::pong() {
    return WSPong(this);
}

}    
}