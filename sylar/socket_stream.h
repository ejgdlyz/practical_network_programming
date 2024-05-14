#ifndef __SYLAR_SOCK_STREAM_H__
#define __SYLAR_SOCK_STREAM_H__

#include "stream.h"
#include "socket.h"

namespace sylar {

class SocketStream : public Stream {
public:
    typedef std::shared_ptr<SocketStream> ptr;
    // owner: 表示是否对该 sock 全权管理  
    SocketStream(Socket::ptr sock, bool owner = true);
    ~SocketStream();

    virtual int read(void* buffer, size_t length) override;
    virtual int read(ByteArray::ptr ba, size_t length) override;         // 期望从 Socket 读 length (不确定) 个字节放入 ByteArray
    virtual int write(const void* buffer, size_t length) override;
    virtual int write(ByteArray::ptr ba, size_t length) override;        // 从 ByteArray 向 Socket 写入 length 个字节
    virtual void close() override;

    Socket::ptr getSocket() const { return m_socket;}
    bool isConnected() const;
protected:
    Socket::ptr m_socket;
    bool m_owner;
};

}

#endif  // __SYLAR_SOCK_STREAM_H__