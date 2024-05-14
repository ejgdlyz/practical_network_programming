#ifndef __SYLAR_HTTP_WS_SESSION_H__
#define __SYLAR_HTTP_WS_SESSION_H__

#include <stdint.h>
#include "sylar/config.h"
#include "sylar/http/http_session.h"

namespace sylar {
namespace http {

#pragma pack(1)  // 指定 WSFrameHead 内存对齐模式为 1 字节
struct WSFrameHead {
    enum OPCODE {
        CONTINUE = 0,       // 数据分片帧
        TEXT_FRAME = 1,     // 文本帧
        BIN_FRAME = 2,      // 二进制帧
        CLOSE = 8,          // 断开连接 
        PING = 0x9,         // PING
        PONG = 0xA          // PONG
    };

    /// @brief 定义位字段，以及字段宽度
    uint32_t opcode: 4;
    bool rsv3: 1;
    bool rsv2: 1;
    bool rsv1: 1;
    bool fin: 1;
    uint32_t payload: 7;
    bool mask: 1;

    std::string toString() const;

};
#pragma pack()

class WSFrameMessage {
public:    
    typedef std::shared_ptr<WSFrameMessage> ptr;
    WSFrameMessage(int opcode = 0, const std::string& data = "");

    int getOpcode() const {return m_opcode;}
    void setOpcode(int v) { m_opcode = v;}

    const std::string& getData() const { return m_data;}
    std::string& getData() { return m_data;}
    void setData(const std::string& v) { m_data = v;} 
private:
    int m_opcode;
    std::string m_data;
};

class WSSession : public HttpSession {
public:
    typedef std::shared_ptr<WSSession> ptr;
    WSSession(Socket::ptr sock, bool owner = true);

    // server client
    HttpRequest::ptr handleShake();

    WSFrameMessage::ptr recvMessage();
    int32_t sendMessage(WSFrameMessage::ptr msg, bool fin = true);
    int32_t sendMessage(const std::string& msg, int32_t opcode = WSFrameHead::TEXT_FRAME, bool fin = true);
    int32_t ping();
    int32_t pong();
private:
    bool handleServerShake();
    bool handleClientShake();
};

extern sylar::ConfigVar<uint32_t>::ptr g_websocket_message_max_size;
WSFrameMessage::ptr WSRecvMessage(Stream* stream, bool client);
int32_t WSSendMessage(Stream* stream, WSFrameMessage::ptr msg, bool client, bool fin);
int32_t WSPing(Stream* stream);
int32_t WSPong(Stream* stream);

}
}

#endif  // 