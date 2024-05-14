
#ifndef __SYLAR_HTTP_PARSER_H__
#define __SYLAR_HTTP_PARSER_H__

#include "http.h"
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace sylar {
namespace http {

class HttpRequestParser {
public:
    typedef std::shared_ptr<HttpRequestParser> ptr;
    HttpRequestParser();

    // http_parser_execute()
    size_t execute(char* data, size_t len);   

    // 是否结束，与 http_parser_finish() 有关
    int isFinished(); 
    
    // 是否有误 
    int hasError();   

    HttpRequest::ptr getData() const { return m_data;}
    void setError(int v) { m_error = v;}

    uint64_t getContentLength();
    const http_parser& getParser() const { return m_parser;}
public:
    static uint64_t GetHttpRequestBufferSize();
    static uint64_t GetHttpRequestMaxBodySize();
private:    
    http_parser m_parser;
    HttpRequest::ptr m_data;            // 作为结果
    // 1000: invalid method, 1001: invalid version, 1002: invalid filed
    int m_error;                        // 是否有误, 

};

class HttpResponseParser {
public:
    typedef std::shared_ptr<HttpResponseParser> ptr;
    HttpResponseParser();
    
    // http_parser_execute(), chunck 表示响应是否分块传输
    size_t execute(char* data, size_t len, bool chunck);   

    // 是否结束，与 http_parser_finish() 有关
    int isFinished(); 
    
    // 是否有误 
    int hasError();   

    HttpResponse::ptr getData() const { return m_data;}
    void setError(int v) { m_error = v;}

    uint64_t getContentLength();
    const httpclient_parser& getParser() const { return m_parser;}
public:
    static uint64_t GetHttpResponseBufferSize();
    static uint64_t GetHttpResponseMaxBodySize();
private:    
    httpclient_parser m_parser;
    HttpResponse::ptr m_data;           // 作为结果
    // 1001: invalid version, 1002: invalid field
    int m_error;                        // 是否有误  
};

}
}


#endif  // __SYLAR_HTTP_PARSER_H__