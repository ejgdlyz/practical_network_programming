#ifndef __SYLAR_ENDIAN_H__
#define __SYLAR_ENDIAN_H__

#define SYLAR_LITTLE_ENDIAN 1   // 小端序
#define SYLAR_BIG_ENDIAN 2      // 大端序

#include <byteswap.h>
#include <stdint.h>
#include <type_traits>

namespace sylar {

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T val) {
    return (T)bswap_64((uint64_t) val);  // 字节翻转
}

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T val) {
    return (T)bswap_32((uint32_t) val);
}

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T val) {
    return (T)bswap_16((uint16_t) val);
}

#if BYTE_ORDER == BIG_ENDIAN
#define SYLAR_BYTE_ORDER SYLAR_BIG_ENDIAN
#else
#define SYLAR_BYTE_ORDER SYLAR_LITTLE_ENDIAN
#endif

#if SYLAR_BYTE_ORDER == SYLAR_BIG_ENDIAN

// 只在小端机器上执行 byteswap, 在大端机器上什么都不做
template <class T>
T byteswapOnLittleEndian(T t) {
    return  t;
}

// 只在大端机器上执行 byteswap, 在小端机器上什么都不做
template <class T>
T byteswapOnBigEndian(T t) {
    return byteswap(t);
}

#else

template <class T>
T byteswapOnLittleEndian(T t) {
    return byteswap(t);
}

template <class T>
T byteswapOnBigEndian(T t) {
    return  t;

}
#endif

}

#endif  // __SYLAR_ENDIAN_H__