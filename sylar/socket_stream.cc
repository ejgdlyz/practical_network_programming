#include "socket_stream.h"
#include <vector>

namespace sylar {

// owner: 表示是否对该 sock 全权管理  
SocketStream::SocketStream(Socket::ptr sock, bool owner)
    :m_socket(sock),
    m_owner(owner) {
}

SocketStream::~SocketStream() {
    if (m_owner && m_socket) {
        m_socket->close();
    }
}

bool SocketStream::isConnected() const {
    return m_socket && m_socket->isConnected();
}

int SocketStream::read(void* buffer, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    return m_socket->recv(buffer, length);
}

int SocketStream::read(ByteArray::ptr ba, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getWriteBuffers(iovs, length);  // iovs 指向 ByteArray 中的内存块，通过 iovec 向 ByteArray 写数据
    int rt = m_socket->recv(&iovs[0], iovs.size());
    if (rt > 0) {
        ba->setPosition(ba->getPosition() + rt);
    }
    return rt;
}         

// 期望从 Socket 读 length (不确定) 个字节放入 ByteArray
int SocketStream::write(const void* buffer, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    return m_socket->send(buffer, length);
}

int SocketStream::write(ByteArray::ptr ba, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getReadBuffers(iovs, length);               // 从 ByteArray 读出需要的数据
    int rt =  m_socket->send(&iovs[0], iovs.size());
    if (rt > 0) {
        ba->setPosition(ba->getPosition() + rt);    // 需要修改位置
    }
    return rt;
}        

// 从 ByteArray 向 Socket 写入 length 个字节
void SocketStream::close() {
    if (m_socket) {
        m_socket->close();
    }
}

} // namespace sylar 
