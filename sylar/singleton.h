#ifndef __SYLAR_SINGLE_H__
#define __SYLAR_SINGLE_H__

#include <memory>

namespace sylar {

namespace {

template <class T, class X, int N>
T& GetInstanceX() {
    static T v;
    return v;
}

template <class T, class X, int N> 
std::shared_ptr<T> GetInstancePtr() {
    static std::shared_ptr<T> v(new T);
    return v;
}
}

 /**
 * @description: 单例模式封装
 * @details T 类型
 *          X 为了创建多个实例对应的 Tag
 *          N 为同一个 Tag 创造多个实例索引
 */
template <class T, class X = void, int N = 0>
class Singleton {
public:
    /**
     * @return {返回裸指针}
     */
    static T* GetInstance() {
        static T v;
        return &v;
        //return &GetInstanceX<T, X, N>();
    }
};

template<class T, class X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> v(new T);
        return v;
        //return GetInstancePtr<T, X, N>();
    }
};
}

#endif // __SYLAR_SINGLE_H__