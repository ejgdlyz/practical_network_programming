#include <string.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include "bytearray.h"
#include "endian.h"
#include "log.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

ByteArray::Node::Node(size_t s)
    :ptr(new char[s])
    ,next(nullptr)
    ,size(s) {

}

ByteArray::Node::Node() 
    :ptr(nullptr)
    ,next(nullptr)
    ,size(0) {
}

ByteArray::Node::~Node() {
    if (ptr) {
        delete[] ptr;
    }
}

// 链表每个节点默认 4K
ByteArray::ByteArray(size_t base_size)
    :m_baseSize(base_size)
    ,m_position(0)
    ,m_capacity(base_size)
    ,m_size(0)
    ,m_endian(SYLAR_BIG_ENDIAN)
    ,m_root(new Node(base_size))
    ,m_cur(m_root)
     {

}  

ByteArray::~ByteArray() {
    Node* tmp = m_root;
    while (tmp) {
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
}

bool ByteArray::isLittleEndian() const {
    return m_endian == SYLAR_LITTLE_ENDIAN;
}

void ByteArray::setIsLittleEndian(bool val) {
    if (val) {
        m_endian = SYLAR_LITTLE_ENDIAN;
    } else {
        m_endian = SYLAR_BIG_ENDIAN;
    }   
}

// write
void ByteArray::writeFint8 (int8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFuint8(uint8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFint16(int16_t value) {
    if (m_endian != SYLAR_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));

}

void ByteArray::writeFuint16(uint16_t value) {
    if (m_endian != SYLAR_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFint32(int32_t value) {
    if (m_endian != SYLAR_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFuint32(uint32_t value) {
    if (m_endian != SYLAR_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFint64(int64_t value) {
    if (m_endian != SYLAR_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFuint64(uint64_t value) {
    if (m_endian != SYLAR_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

static uint32_t EncodeZigzag32(const int32_t & v) {
    if (v < 0) {
        return ((uint32_t)(-v) << 1) - 1;  // 负数编码
    } else {
        return v << 1;      // 正数编码
    }
}

static uint64_t EncodeZigzag64(const int64_t & v) {
    if (v < 0) {
        return ((uint64_t)(-v) << 1) - 1;
    } else {
        return v << 1;
    }
}

static int32_t DecodeZigzag32(const uint32_t & v) {
    return (v >> 1) ^ -(v & 1);  // 按位取反
}

static int64_t DecodeZigzag64(const uint64_t & v) {
    return (v >> 1) ^ -(v & 1);
}

// 可变长度-压缩类型
void ByteArray::writeInt32(int32_t value) {
    writeUint32(EncodeZigzag32(value));
}

// 可变长编码
void ByteArray::writeUint32(uint32_t value) {
    uint8_t tmp[5];
    uint8_t i = 0;
    while (value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);      // i 是 value 被压缩后的字节数
}

void ByteArray::writeInt64(int64_t value) {
    writeUint64(EncodeZigzag64(value));
}

void ByteArray::writeUint64(uint64_t value) {
    uint8_t tmp[10];
    uint8_t i = 0;
    while (value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::writeFloat(float value) {
    uint32_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint32(v);
}

void ByteArray::writeDouble(double value) {
    uint64_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint64(v);
}

// length:int 16, data 
void ByteArray::writeStringF16(const std::string& value) {
    writeFuint16(value.size());
    write(value.c_str(), value.size());
} 

// length:int 32, data 
void ByteArray::writeStringF32(const std::string& value) {
    writeFuint32(value.size());
    write(value.c_str(), value.size());
}  

// length:int 64, data 
void ByteArray::writeStringF64(const std::string& value) {
    writeFuint64(value.size());
    write(value.c_str(), value.size());
}  

// length:varint, data 
void ByteArray::writeStringVint(const std::string& value) {  // 压缩长度表示
    writeUint64(value.size());
    write(value.c_str(), value.size());
}  

void ByteArray::writeStringWithoutLength(const std::string& value) {  // 压缩长度表示
    write(value.c_str(), value.size());
}


// read  
int8_t ByteArray::readFint8() {
    int8_t v;
    read(&v, sizeof(v));
    return v;
}

uint8_t ByteArray::readFuint8() {
    uint8_t v;
    read(&v, sizeof(v));
    return v;
}       

#define XX(type) \
    type v; \
    read(&v, sizeof(v)); \
    if (m_endian != SYLAR_BYTE_ORDER) { \
        return byteswap(v);  \
    }  \
    return v; \

int16_t ByteArray::readFint16() {
    XX(int16_t);
}

uint16_t ByteArray::readFuint16() {
    XX(uint16_t);
}   

int32_t ByteArray::readFint32() {
    XX(int32_t);
}

uint32_t ByteArray::readFuint32() {
    XX(uint32_t);
}

int64_t ByteArray::readFint64() {
    XX(int64_t);
}

uint64_t ByteArray::readFuint64() {
    XX(uint64_t);
}

# undef XX

int32_t  ByteArray::readInt32() {
    return DecodeZigzag32(readUint32());
}

uint32_t ByteArray::readUint32() {
    uint32_t result = 0;
    for (int i = 0; i < 32; i += 7) {
        uint8_t b = readFuint8();
        if (b < 0x80) {
            result |= ((uint32_t)b) << i;
            break;
        } else {
            result |= (((uint32_t)(b & 0x7f)) << i);  // (b & 0x7f) 取得低 7 位
        }
    }
    return result;
}

int64_t  ByteArray::readInt64() {
    return DecodeZigzag64(readUint64());
}

uint64_t ByteArray::readUint64() {
    uint64_t result = 0;
    for (int i = 0; i < 32; i += 7) {
        uint8_t b = readFuint8();
        if (b < 0x80) {
            result |= ((uint64_t)b) << i;       // b 先左移，再与 result 或运算
            break;
        } else {
            result |= (((uint64_t)(b & 0x7f)) << i);
        }
    }
    return result;
}

float ByteArray::readFloat() {
    uint32_t v = readFuint32();
    float value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

double ByteArray::readDouble() {
    uint64_t v = readFuint64();
    double value;
    memcpy(&value, &v, sizeof(v));    
    return value;
}

// length: int16, data
std::string ByteArray::readStringF16() {
    uint16_t len = readFuint16();  // 先读长度
    std::string buf;               // 然后读字符数据
    buf.resize(len);
    read(&buf[0], len);
    return buf;
}

// length: int32, data
std::string ByteArray::readStringF32() {
    uint32_t len = readFuint32();
    std::string buf;
    buf.resize(len);
    read(&buf[0], len);
    return buf;   
}

// length: int64, data
std::string ByteArray::readStringF64() {
    uint64_t len = readFuint64();
    std::string buf;
    buf.resize(len);
    read(&buf[0], len);
    return buf;
}

// length: varint, data
std::string ByteArray::readStringVint() {
    uint64_t len = readUint64();
    std::string buf;
    buf.resize(len);
    read(&buf[0], len);
    return buf;
}

// 内部操作
void ByteArray::clear() {
    m_position = 0;
    m_capacity = m_baseSize;
    Node* tmp = m_root->next;
    while (tmp) {
        m_cur = tmp;
        tmp = tmp->next;
        delete m_cur;
    }
    m_cur = m_root;
    m_root->next = NULL;
}   

void ByteArray::write(const void* buf, size_t size) {
    if (size == 0) {
        return;
    }

    addCapaticy(size);                      // 如有新增 Node, 改变 m_cur
    size_t npos = m_position % m_baseSize;  // 当前 Node::ptr 块内偏移
    size_t ncap = m_cur->size - npos;       // 当前节点的剩余容量 = 当前节点大小 - 块内偏移
    size_t bpos = 0;                        // buf 中已写入的位置

    while (size > 0) {
        if (ncap >= size) {
            memcpy(m_cur->ptr + npos, (const char*)buf + bpos, size);
            if (m_cur->size == (npos + size)) {
                m_cur = m_cur->next;
            }
            m_position += size;  // 后移
            size = 0;
            
        } else {
            memcpy(m_cur->ptr + npos, (const char*)buf + bpos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;

            m_cur = m_cur->next;
            ncap = m_cur->size;
            npos = 0;
        }
    }

    if (m_position > m_size) {
        m_size = m_position;
    }
}

void ByteArray::read(void* buf, size_t size) {
    if (size > getReadSize()) {
        throw std::out_of_range("not enough length");
    }

    size_t npos = m_position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    size_t bpos = 0;
    while (size > 0) {
        if (ncap >= size) {
            memcpy((char*)buf + bpos, m_cur->ptr + npos, size);
            if (m_cur->size == (npos + size)) {
                m_cur = m_cur->next;
            }
            m_position += size;
            bpos += size;
            size = 0;
        } else {
            memcpy((char*)buf + bpos, m_cur->ptr + npos, ncap);
            m_position += ncap;
            bpos += ncap;
            size -= ncap;

            m_cur = m_cur->next;
            ncap = m_cur->size;
            npos = 0;
        }
    }
}

// 从当前位置读指定长度的数据， 不会改成员属性, toString() 调用
void ByteArray::read(void* buf, size_t size, size_t position) const {
    if (size > getReadSize()) {
        throw std::out_of_range("not enough length");
    }

    size_t npos = position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    size_t bpos = 0;
    Node* cur = m_cur;
    while (size > 0) {
        if (ncap >= size) {
            memcpy((char*)buf + bpos, cur->ptr + npos, size);
            if (cur->size == (npos + size)) {
                cur = cur->next;
            }
            position += size;
            bpos += size;
            size = 0;
        } else {
            memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
            position += ncap;
            bpos += ncap;
            size -= ncap;

            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
    }
}

void ByteArray::setPosition(size_t v) {
    if (v > m_capacity) {
        throw std::out_of_range("set_position out of range");
    }
    m_position = v;
    if (m_position > m_size) {
        m_size = m_position;
    }
    m_cur = m_root;
    while (v > m_cur->size) {
        v -= m_cur->size;
        m_cur = m_cur->next;
    }

    if (v == m_cur->size) {
        m_cur = m_cur->next;
    }
}

// 当前数据写到文件, 调用前先调用 setPosition()
bool ByteArray::writeToFile(const std::string& name) const {
    std::ofstream ofs;
    ofs.open(name, std::ios::trunc | std::ios::binary);
    if (!ofs) {
        SYLAR_LOG_ERROR(g_logger) << "writeToFile name=" << name
            << " error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    int64_t read_size = getReadSize();
    int64_t pos = m_position;  // 当前位置
    Node* cur = m_cur;
    while (read_size > 0) {
        int diff = pos % m_baseSize;
        int64_t len = (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;  // 一次最多写入一个块

        ofs.write(cur->ptr + diff, len);
        cur = cur->next;
        pos += len;
        read_size -= len;
    }
    return true;
}

// 从文件读出数据
bool ByteArray::readFromFile(const std::string& name) {
    std::ifstream ifs;
    ifs.open(name, std::ios::binary);
    if (!ifs) {
        SYLAR_LOG_ERROR(g_logger) << "readFromFile name=" << name
            << " error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    std::shared_ptr<char> buf(new char[m_baseSize], [](char* ptr){ delete[] ptr;});

    while (!ifs.eof()) {
        ifs.read(buf.get(), m_baseSize);    // 读 <= 一块大小数据
        write(buf.get(), ifs.gcount());     // 以真正读入的大小写入
    }
    return true;
}   

void ByteArray::addCapaticy(size_t size) {
    if (size == 0) {
        return;
    }
    size_t old_cap = getCapacity();
    if (old_cap >= size) {
        return;
    }

    size = size - old_cap;  // 需要增加的额外容量
    size_t count = ceil(1.0 * size / m_baseSize);  // 需要添加多少个节点
    Node* tmp = m_root;
    // 尾节点
    while (tmp->next) {
        tmp = tmp->next;
    }

    Node* first = NULL;
    for (size_t i = 0; i < count; ++i) {
        tmp->next = new Node(m_baseSize);
        if (!first) {
            first = tmp->next;
        }
        tmp = tmp->next;
        m_capacity += m_baseSize;
    }

    // 加完之后，如果当前节点的容量为0，还需后移一个节点
    if (old_cap == 0) {
        m_cur = first;  // 移动 m_cur 到新增的头节点
    }
}

std::string ByteArray::toString() const{
    std::string str;
    str.resize(getReadSize());
    if (str.empty()) {
        return str;
    }
    read(&str[0], str.size(), m_position);
    return str;
}

std::string ByteArray::toHexString() const{
    std::string str = toString();
    std::stringstream ss;

    for (size_t i = 0; i < str.size(); ++i) {
        if (i > 0 && i % 32 == 0) {
            ss << std::endl;
        }
        ss << std::setw(2) << std::setfill('0') << std::hex 
            << (int)(uint8_t)str[i] << " ";
    }
    return ss.str();
}

// 从当前位置
uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
    len = len > getReadSize() ? getReadSize() : len;
    if (len == 0) {
        return 0;
    }
    uint64_t size = len;

    size_t npos = m_position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    struct iovec iov;
    Node* cur = m_cur;

    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;

            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;  // 实际读到的长度
}

// 从指定位置返回指定长度的数据
uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const {
    len = len > getReadSize() ? getReadSize() : len;
    if (len == 0) {
        return 0;
    }
    uint64_t size = len;

    size_t npos = position % m_baseSize;
    size_t count = position / m_baseSize;
    Node* cur = m_root;
    while (count > 0 && cur) {
        cur = cur->next;
        --count;
    }

    size_t ncap = cur->size - npos;
    struct iovec iov;
    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;

            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;  // 实际读到的长度
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
    if (len == 0) {
        return 0;
    }

    addCapaticy(len);
    uint64_t size = len;
    
    size_t npos = m_position % m_baseSize;
    size_t ncap = m_cur->size - npos;
    struct iovec iov;
    Node* cur = m_cur;
    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len -= ncap;

            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);     // 拷贝 iovec 到 buffer
    }
    return size;
}

}