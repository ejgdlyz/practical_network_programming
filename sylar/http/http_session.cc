#include "http_session.h"
#include "http_parser.h"

namespace sylar {
namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner) 
    : SocketStream(sock, owner) {
}

// 获取 HTTP Request 结构体
HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser);
    
    // 申请一块缓冲区
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    // uint64_t buff_size = 100;
    std::shared_ptr<char> buffer(new char[buff_size], 
        [](char* ptr){ delete[] ptr;});
    
    char* data = buffer.get();
    int offset = 0;  // execute 解析后 data 的剩余长度，
    do {
        int len = read(data + offset, buff_size - offset);  // 在 offset 后继续读取数据
        if (len <= 0) {
            close();
            return nullptr;
        }
        len += offset;      // 当前 data 长度 = 当前读取的 data 长度 + 上次剩余的数据长度
        size_t nparse = parser->execute(data, len);  // nparse 为成功解析的字节数，且 data 会向前移动 nparse 个字节
        if (parser->hasError()) {
            close();
            return nullptr;
        }
        offset = len - nparse;  // 此时 data 剩余的数据为：当前 data 长度 (len) - execute 移除的数据长度 (nparse)
        if (offset == (int)buff_size) {  // 缓冲区满还未解析完
            close();
            return nullptr;
        }
        if (parser->isFinished()) {  // 解析结束
            break;
        }
    } while (true);

    int64_t body_length = parser->getContentLength();  // 获得 header 中 "content-length" 对应的 body 长度
    if (body_length > 0) {
        std::string body;
        body.resize(body_length);

        int len = 0;
        if (body_length >= offset) {     // body 长度 > 缓冲区剩余大小，将 data 中的数据全部复制到 body
            memcpy(&body[0], data, offset);
            len = offset;
        } else {                    // 否则，直接取 body_length
            memcpy(&body[0], data, body_length);
            len = body_length;
        }
        body_length -= offset;
        if (body_length > 0) {      // 缓冲区中的数据 < body_length，再从缓冲区读取数据直到满足 body_length
            if (readFixSize(&body[len], body_length) <= 0) {
                close();
                return nullptr;
            }
        }
        parser->getData()->setBody(body);   // 设置 body
    }
    parser->getData()->init();
    return parser->getData();  // 返回解析后的 HttpRequest

}   

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

}
}