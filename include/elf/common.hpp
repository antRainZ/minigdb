#ifndef FAULT_INJECT_ELF_COMMON_H
#define FAULT_INJECT_ELF_COMMON_H
#include <cstdint>
namespace elf
{
/**
 * 字节序 enum
 */
enum class byte_order {
    native,
    lsb,
    msb
};

/**
 * @brief 获取字节序
 * 
 * @param o 
 * @return byte_order 
 */
static inline byte_order
resolve_order(byte_order o)
{
    static const union {
        int i;
        char c[sizeof(int)];
    } test = {1};

    if (o == byte_order::native)
        return test.c[0] == 1 ? byte_order::lsb : byte_order::msb;
    return o;
}

/**
 * @brief 用于将一个值从一种字节序转换为另一种字节序
 * 
 * @tparam T 
 * @param v 
 * @param from 表示原始字节序
 * @param to 表示目标字节序
 * @return T 
 */
template <typename T>
T swizzle(T v, byte_order from, byte_order to)
{
    static_assert(sizeof(T) == 1 ||
                      sizeof(T) == 2 ||
                      sizeof(T) == 4 ||
                      sizeof(T) == 8,
                  "cannot swizzle type");

    from = resolve_order(from);
    to = resolve_order(to);

    if (from == to)
        return v;

    switch (sizeof(T)) {
    case 1:
        return v;
    case 2: {
        std::uint16_t x = (std::uint16_t)v;
        return (T)(((x & 0xFF) << 8) | (x >> 8));
    }
    case 4:
        return (T)__builtin_bswap32((std::uint32_t)v);
    case 8:
        return (T)__builtin_bswap64((std::uint64_t)v);
    }
}

namespace internal
{
/**
 * @brief 用于根据指定的字节序选择要使用的类型
 * 
 * @tparam ord 表示要选择的字节序
 * @tparam Native 表示原生字节序下的类型
 * @tparam LSB 表示小端字节序下的类型
 * @tparam MSB 表示大端字节序下的类型
 */
template <byte_order ord, typename Native, typename LSB, typename MSB>
struct OrderPick;

template <typename Native, typename LSB, typename MSB>
struct OrderPick<byte_order::native, Native, LSB, MSB> {
    typedef Native T;
};

template <typename Native, typename LSB, typename MSB>
struct OrderPick<byte_order::lsb, Native, LSB, MSB> {
    typedef LSB T;
};

template <typename Native, typename LSB, typename MSB>
struct OrderPick<byte_order::msb, Native, LSB, MSB> {
    typedef MSB T;
};
} // namespace internal
} // namespace elf

#endif