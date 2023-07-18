#ifndef FAULT_INJECT_DWARF_INTERNAL_H
#define FAULT_INJECT_DWARF_INTERNAL_H

#include "../hex.hpp"
#include "dwarf.hpp"

#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace dwarf
{
/**
 * @brief 32还是64位格式
 *
 */
enum class format {
    unknown,
    dwarf32,
    dwarf64
};

enum class byte_order {
    lsb,
    msb
};

/**
 * @brief 用于确定当前系统的字节顺序
 *
 * @return byte_order
 */
static inline byte_order native_order()
{
    static const union {
        int i;
        char c[sizeof(int)];
    } test = {1};

    return test.c[0] == 1 ? byte_order::lsb : byte_order::msb;
}

/**
 * @brief 表示一个DWARF节或节的片段。该结构体还跟踪解码该节中的值所需的动态信息
 *
 */
struct section {
    // 表示节的类型的枚举类型
    section_type type;
    // 指向节的起始地址和结束地址的指针
    const char *begin, *end;
    // 表示节的格式的枚举类型
    const format fmt;
    // 表示节的字节顺序的枚举类型
    const byte_order ord;
    // 表示节中地址的大小
    unsigned addr_size;

    section(section_type type, const void *begin,
            section_length length,
            byte_order ord, format fmt = format::unknown,
            unsigned addr_size = 0)
        : type(type), begin((char *)begin), end((char *)begin + length),
          fmt(fmt), ord(ord), addr_size(addr_size) {}

    section(const section &o) = default;

    /**
     * @brief 返回一个新的section结构体的共享指针，表示当前节的一个片段
     * 
     * @param start 
     * @param len 
     * @param fmt 
     * @param addr_size 
     * @return std::shared_ptr<section> 
     */
    std::shared_ptr<section> slice(section_offset start, section_length len,
                                   format fmt = format::unknown,
                                   unsigned addr_size = 0)
    {
        if (fmt == format::unknown)
            fmt = this->fmt;
        if (addr_size == 0)
            addr_size = this->addr_size;

        return std::make_shared<section>(
            type, begin + start,
            std::min(len, (section_length)(end - begin)),
            ord, fmt, addr_size);
    }

    /**
     * @brief 返回当前节的大小
     * 
     * @return size_t 
     */
    size_t size() const
    {
        return end - begin;
    }
};

/**
 * @brief 表示一个指向DWARF节的游标。游标提供了反序列化操作和边界检查
 * 
 */
struct cursor {
    // 指向section结构体的共享指针，表示游标所在的节
    std::shared_ptr<section> sec;
    // 指向游标当前位置的指针
    const char *pos;

    cursor()
        : pos(nullptr) {}
    cursor(const std::shared_ptr<section> sec, section_offset offset = 0)
        : sec(sec), pos(sec->begin + offset) {}

    /**
     * @brief 读取一个子节。游标必须位于一个初始长度处。
     * 读取完成后，游标将指向子节的结束位置之后。
     * 返回一个具有适当的DWARF格式并从游标的当前位置开始的节（通常紧跟着skip_initial_length操作）
     * 
     * @return std::shared_ptr<section> 
     */
    std::shared_ptr<section> subsection();
    /**
     * @brief 读取一个有符号LEB128整数
     * 根据LEB128编码规则，从游标当前位置开始逐个字节读取，直到遇到最后一个字节。
     * 将每个字节的低7位与结果进行按位或运算，并根据字节的第8位判断是否还有下一个字节。
     * 如果有下一个字节，则将结果左移7位，并将字节的低7位与结果进行按位或运算。
     * 如果字节的第7位为1，表示该字节不是最后一个字节，需要继续读取下一个字节；
     * 如果字节的第7位为0，表示该字节是最后一个字节，
     * 根据字节的第6位判断结果是否为负数，如果是负数，则将结果进行补码运算
     * @return std::int64_t 
     */
    std::int64_t sleb128();
    /**
     * @brief 根据当前节的格式，分别读取一个uint32_t或一个uint64_t的值
     * 
     * @return section_offset 
     */
    section_offset offset();
    /**
     * @brief 首先调用cstr函数获取以null结尾的字符串的指针和长度，
     * 然后将字符串复制到out中
     * 
     * @param out 
     */
    void string(std::string &out);
    /**
     * @brief 读取一个以null结尾的字符串，并返回字符串的指针。
     * 
     * @param size_out 可以通过size_out参数获取字符串的长度
     * @return const char* 
     */
    const char *cstr(size_t *size_out = nullptr);

    /**
     * @brief 确保游标后面的字节数至少为bytes
     * 
     * @param bytes 
     */
    void ensure(section_offset bytes)
    {
        if ((section_offset)(sec->end - pos) < bytes || pos >= sec->end)
            underflow();
    }

    /**
     * @brief 读取一个固定大小的值，要求值的大小不能超过8字节
     * 
     * @tparam T 
     * @return T 
     */
    template <typename T>
    T fixed()
    {
        ensure(sizeof(T));
        static_assert(sizeof(T) <= 8, "T too big");
        uint64_t val = 0;
        const unsigned char *p = (const unsigned char *)pos;
        if (sec->ord == byte_order::lsb) {
            for (unsigned i = 0; i < sizeof(T); i++)
                val |= ((uint64_t)p[i]) << (i * 8);
        } else {
            for (unsigned i = 0; i < sizeof(T); i++)
                val = (val << 8) | (uint64_t)p[i];
        }
        pos += sizeof(T);
        return (T)val;
    }

    /**
     * @brief 读取一个无符号LEB128整数
     * 
     * @return std::uint64_t 
     */
    std::uint64_t uleb128()
    {
        std::uint64_t result = 0;
        int shift = 0;
        while (pos < sec->end) {
            uint8_t byte = *(uint8_t *)(pos++);
            result |= (uint64_t)(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0)
                return result;
            shift += 7;
        }
        underflow();
        return 0;
    }

    /**
     * @brief 读取一个地址值，根据节的地址大小进行读取
     * 
     * @return taddr 
     */
    taddr address()
    {
        switch (sec->addr_size) {
        case 1:
            return fixed<uint8_t>();
        case 2:
            return fixed<uint16_t>();
        case 4:
            return fixed<uint32_t>();
        case 8:
            return fixed<uint64_t>();
        default:
            throw std::runtime_error("address size " + std::to_string(sec->addr_size) + " not supported");
        }
    }
    /**
     * @brief 跳过初始长度字段。根据当前节的格式，分别跳过一个uword
     * 或一个uword和一个uint64_t的大小
     * 
     */
    void skip_initial_length();

    /**
     * @brief 跳过单元类型
     * 
     */
    void skip_unit_type();

    /**
     * @brief 根据给定的DW_FORM跳过相应的字段。
     * 根据字段的类型和当前节的格式，分别跳过不同大小的字段
     * 
     * @param form 
     */
    void skip_form(DW_FORM form);

    cursor &operator+=(section_offset offset)
    {
        pos += offset;
        return *this;
    }

    cursor operator+(section_offset offset) const
    {
        return cursor(sec, pos + offset);
    }

    bool operator<(const cursor &o) const
    {
        return pos < o.pos;
    }

    bool end() const
    {
        return pos >= sec->end;
    }

    bool valid() const
    {
        return !!pos;
    }

    /**
     * @brief 获取游标相对于节起始位置的偏移量
     * 
     * @return section_offset 
     */
    section_offset get_section_offset() const
    {
        return pos - sec->begin;
    }

  private:
    cursor(const std::shared_ptr<section> sec, const char *pos)
        : sec(sec), pos(pos) {}
    /**
     * @brief 当游标超出节的范围时，抛出异常
     * 
     */
    void underflow();
};

/**
 * An attribute specification in an abbrev.
 */
struct attribute_spec {
    DW_AT name;
    DW_FORM form;

    // Computed information
    value::type type;

    attribute_spec(DW_AT name, DW_FORM form);
};

typedef std::uint64_t abbrev_code;

/**
 * @brief 用于表示.debug_abbrev中的一个条目
 * 
 */
struct abbrev_entry {
    // 表示条目的编码
    abbrev_code code;
    DW_TAG tag;
    bool children;
    // 表示条目的属性规范列表
    std::vector<attribute_spec> attributes;

    abbrev_entry() : code(0) {}
    /**
     * @brief 用于从给定的游标cur中读取条目的信息。
     * read函数首先清空attributes列表，然后
     * 1）调用游标的uleb128函数读取code 
     * 2）调用游标的uleb128函数读取tag
     * 3）fixed函数读取，判断使用有子节点
     * 4）循环读取属性
     * 5）将其容量调整为与实际元素数量相匹配
     * @param cur 
     * @return true 读取成功
     * @return false 
     */
    bool read(cursor *cur);
};

/**
 * 
 */
struct name_unit {
    // 表示节头的版本号
    uhalf version;
    section_offset debug_info_offset;
    section_length debug_info_length;
    // 指向该节中第一个name_entry的游标。该游标的所在节被限制在该单元内
    cursor entries;

    void read(cursor *cur)
    {
        // Section 7.19
        std::shared_ptr<section> subsec = cur->subsection();
        cursor sub(subsec);
        sub.skip_initial_length();
        version = sub.fixed<uhalf>();
        if (version != 2)
            throw format_error("unknown name unit version " + std::to_string(version));
        debug_info_offset = sub.offset();
        debug_info_length = sub.offset();
        entries = sub;
    }
};

/**
 * 用于表示.debug_pubnames或.debug_pubtypes单元中的一个条目
 */
struct name_entry {
    section_offset offset;
    std::string name;

    /**
     * @brief 用于从给定的游标cur中读取条目的信息
     * 
     * @param cur 
     */
    void read(cursor *cur)
    {
        offset = cur->offset();
        cur->string(name);
    }
};

} // namespace dwarf

#endif