#ifndef FAULT_INJECT_DWARF_SMALL_VECTOR_H
#define FAULT_INJECT_DWARF_SMALL_VECTOR_H

#include <cstdint>
#include <string>

namespace dwarf
{
/**
 * @brief 表示一个小型动态数组
 * 
 * @tparam T 
 * @tparam Min 
 */
template <class T, unsigned Min>
class small_vector
{
  public:
    typedef T value_type; // 类型别名，表示数组中的元素类型
    typedef value_type &reference; // 类型别名，表示元素的引用类型
    typedef const value_type &const_reference; // 类型别名，表示元素的常量引用类型
    typedef size_t size_type; // 类型别名，表示数组的大小类型

    small_vector()
        : base((T *)buf), end(base), cap((T *)&buf[sizeof(T[Min])])
    {
    }

    small_vector(const small_vector<T, Min> &o)
        : base((T *)buf), end(base), cap((T *)&buf[sizeof(T[Min])])
    {
        *this = o;
    }

    /**
     * @brief 移动构造函数，创建一个小型动态数组的移动副本
     * 
     * @param o 
     */
    small_vector(small_vector<T, Min> &&o)
        : base((T *)buf), end(base), cap((T *)&buf[sizeof(T[Min])])
    {
        if ((char *)o.base == o.buf) {
            // 判断元素是否存储在内部。如果是，则需要将元素复制到新的对象
            base = (T *)buf;
            end = base;
            cap = (T *)&buf[sizeof(T[Min])];
            *this = o;
            o.clear();
        } else {
            // 如果元素存储在外部，则需要交换指针
            base = o.base;
            end = o.end;
            cap = o.cap;
            o.base = (T *)o.buf;
            o.end = o.base;
            o.cap = (T *)&o.buf[sizeof(T[Min])];
        }
    }

    ~small_vector()
    {
        clear();
        if ((char *)base != buf)
            delete[] (char *)base;
    }

    small_vector &operator=(const small_vector<T, Min> &o)
    {
        size_type osize = o.size();
        clear();
        reserve(osize);
        for (size_type i = 0; i < osize; i++)
            new (&base[i]) T(o[i]);
        end = base + osize;
        return *this;
    }

    size_type size() const
    {
        return end - base;
    }

    bool empty() const
    {
        return base == end;
    }

    /**
     * @brief 为数组分配足够的内存以容纳至少n个元素
     * 
     * @param n 
     */
    void reserve(size_type n)
    {
        if (n <= (size_type)(cap - base))
            return;

        size_type target = cap - base;
        if (target == 0)
            target = 1;
        while (target < n)
            target <<= 1;

        char *newbuf = new char[sizeof(T[target])];
        T *src = base, *dest = (T *)newbuf;
        for (; src < end; src++, dest++) {
            new (dest) T(*src);
            dest->~T();
        }
        if ((char *)base != buf)
            delete[] (char *)base;
        base = (T *)newbuf;
        end = dest;
        cap = base + target;
    }

    reference operator[](size_type n)
    {
        return base[n];
    }

    const_reference operator[](size_type n) const
    {
        return base[n];
    }

    reference at(size_type n)
    {
        return base[n];
    }

    const_reference at(size_type n) const
    {
        return base[n];
    }

    /**
     * @brief 返回数组中倒数第n个元素的引用
     * 
     * @param n 
     * @return reference 
     */
    reference revat(size_type n)
    {
        return *(end - 1 - n);
    }

    const_reference revat(size_type n) const
    {
        return *(end - 1 - n);
    }

    reference front()
    {
        return base[0];
    }

    const_reference front() const
    {
        return base[0];
    }

    reference back()
    {
        return *(end - 1);
    }

    const_reference back() const
    {
        return *(end - 1);
    }

    void push_back(const T &x)
    {
        reserve(size() + 1);
        new (end) T(x);
        end++;
    }

    void push_back(T &&x)
    {
        reserve(size() + 1);
        new (end) T(std::move(x));
        end++;
    }

    void pop_back()
    {
        end--;
        end->~T();
    }

    void clear()
    {
        for (T *p = base; p < end; ++p)
            p->~T();
        end = base;
    }

  private:
    char buf[sizeof(T[Min])];
    T *base, *end, *cap;
};

} // namespace dwarf

#endif