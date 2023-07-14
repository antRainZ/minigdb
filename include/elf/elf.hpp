#ifndef FAULT_INJECT_ELF_ELF_H
#define FAULT_INJECT_ELF_ELF_H

#include "common.hpp"
#include "data.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

namespace elf
{
class elf;
class loader;
class section;
class strtab;
class symtab;
class segment;

/**
 * @brief ELF格式异常
 *
 */
class format_error : public std::runtime_error
{
  public:
    explicit format_error(const std::string &what_arg)
        : std::runtime_error(what_arg) {}
    explicit format_error(const char *what_arg)
        : std::runtime_error(what_arg) {}
};

/**
 * @brief 表示一个ELF文件
 *
 */
class elf
{
  public:
    /**
     * @brief 构造函数，通过从给定的loader读取的数据来创建一个ELF文件对象
     * 需要显式使用构造函数
     * @param l
     */
    explicit elf(const std::shared_ptr<loader> &l);

    elf() = default;
    elf(const elf &o) = default;
    elf(elf &&o) = default;

    elf &operator=(const elf &o) = default;
    /**
     * @brief 返回一个布尔值，指示ELF文件是否有效
     *
     * @return true
     * @return false
     */
    bool valid() const
    {
        return !!m;
    }

    /**
     * @brief 返回ELF文件头，以规范形式
     *
     * @return const Ehdr<>&
     */
    const Ehdr<> &get_hdr() const;

    /**
     * @brief 返回该文件使用的loader对象
     *
     * @return std::shared_ptr<loader>
     */
    std::shared_ptr<loader> get_loader() const;

    /**
     * @brief 返回该文件的所有 segment
     *
     * @return const std::vector<segment>&
     */
    const std::vector<segment> &segments() const;

    /**
     * @brief 返回给定索引处的段。如果找不到该段，则返回一个无效的段
     *
     * @param index
     * @return const segment&
     */
    const segment &get_segment(unsigned index) const;

    /**
     * @brief 返回该文件的所有 section
     *
     * @return const std::vector<section>&
     */
    const std::vector<section> &sections() const;

    /**
     * @brief 返回具有指定名称的节。如果找不到该节，则返回一个无效的节
     *
     * @param index
     * @return const section&
     */
    const section &get_section(const std::string &name) const;

    /**
     * @brief 返回给定索引处的节。如果找不到该节，则返回一个无效的节
     *
     * @param index
     * @return const section&
     */
    const section &get_section(unsigned index) const;

  private:
    struct impl;
    std::shared_ptr<impl> m;
};

/**
 * 用于加载ELF文件的接口类
 */
class loader
{
  public:
    virtual ~loader() {}

    /**
     * @brief 将请求的文件节加载到内存中，并返回指向其开头的指针。这块内存必须在loader对象被销毁之前保持有效和不变
     * 如果loader由于任何原因（包括提前到达文件结尾）无法满足完整的请求，它必须抛出异常
     *
     * @param offset
     * @param size
     * @return const void*
     */
    virtual const void *load(off_t offset, size_t size) = 0;
};

/**
 * @brief 这是一个基于mmap的loader创建函数，根据需要映射请求的节。在完成后，它会关闭fd
 * 如果打算继续使用它，应该使用dup函数复制文件描述符
 *
 * @param fd
 * @return std::shared_ptr<loader>
 */
std::shared_ptr<loader> create_mmap_loader(int fd);

/**
 * @brief 这是一个异常类，表示节的类型与请求的类型不匹配
 *
 */
class section_type_mismatch : public std::logic_error
{
  public:
    explicit section_type_mismatch(const std::string &what_arg)
        : std::logic_error(what_arg) {}
    explicit section_type_mismatch(const char *what_arg)
        : std::logic_error(what_arg) {}
};

/**
 * @brief 这是一个表示ELF段（segment）的类
 *
 */
class segment
{
  public:
    /**
     * @brief 构造函数，根据给定的ELF文件和段头（header）创建一个段对象
     *
     */
    segment() {}

    segment(const elf &f, const void *hdr);
    segment(const segment &o) = default;
    segment(segment &&o) = default;

    /**
     * @brief 返回一个布尔值，指示该段是否有效且对应于ELF文件中的一个段
     *
     * @return true
     * @return false
     */
    bool valid() const
    {
        return !!m;
    }

    /**
     * @brief 返回该段的ELF段头，以规范形式
     *
     * @return const void*
     */
    const Phdr<> &get_hdr() const;

    /**
     * @brief 返回该段的数据。返回的缓冲区的长度将为file_size()字节
     *
     * @return size_t
     */
    const void *data() const;

    /**
     * @brief 返回该段在磁盘上的大小（以字节为单位）
     *
     * @return size_t
     */
    size_t file_size() const;

    /**
     * @brief 返回该段在内存中的大小（以字节为单位）。file_size()和mem_size()之间的字节被隐式填充为零
     *
     * @return size_t
     */
    size_t mem_size() const;

  private:
    struct impl;
    std::shared_ptr<impl> m;
};

/**
 * @brief 表示ELF节（section）的类
 *
 */
class section
{
  public:
    section() {}

    /**
     * @brief 构造函数，根据给定的ELF文件和节头（header）创建一个节对象
     *
     */
    section(const elf &f, const void *hdr);
    section(const section &o) = default;
    section(section &&o) = default;

    /**
     * @brief 返回一个布尔值，指示该节是否有效且对应于ELF文件中的一个节
     *
     * @return true
     * @return false
     */
    bool valid() const
    {
        return !!m;
    }

    /**
     * @brief 返回该节的ELF节头，以规范形式
     *
     * @return const Shdr<>&
     */
    const Shdr<> &get_hdr() const;

    /**
     * @brief 返回该节的名称。如果len_out非空，则将设置为返回字符串的长度
     *
     * @param len_out
     * @return const char*
     */
    const char *get_name(size_t *len_out) const;

    /**
     * @brief 返回该节的名称。返回的字符串是副本，因此不受loader的存活要求限制
     *
     * @return std::string
     */
    std::string get_name() const;

    /**
     * @brief 返回该节的数据。如果这是一个NOBITS节，则返回nullptr
     *
     * @return const void*
     */
    const void *data() const;

    /**
     * @brief 返回该节的大小（以字节为单位）
     *
     * @return size_t
     */
    size_t size() const;

    /**
     * @brief 将该节作为strtab返回。如果该节不是字符串表，则抛出section_type_mismatch异常
     *
     * @return strtab
     */
    strtab as_strtab() const;

    /**
     * @brief 将该节作为symtab返回。如果该节不是符号表，则抛出section_type_mismatch异常
     *
     * @return symtab
     */
    symtab as_symtab() const;

  private:
    struct impl;
    std::shared_ptr<impl> m;
};

/**
 * @brief 表示字符串表的类
 *
 */
class strtab
{
  public:
    strtab() = default;
    /**
     * @brief 构造函数，根据给定的ELF文件、数据和大小创建一个字符串表对象
     *
     * @param f
     * @param data
     * @param size
     */
    strtab(elf f, const void *data, size_t size);

    /**
     * @brief 返回一个布尔值，指示该字符串表是否有效
     *
     * @return true
     * @return false
     */
    bool valid() const
    {
        return !!m;
    }

    /**
     * @brief 返回该字符串表中给定偏移量处的字符串。如果偏移量超出范围，则抛出std::range_error异常。
     * 这非常高效，因为返回的指针直接指向加载的节中的数据，但仍然验证返回的字符串是否以NUL结尾
     *
     * @param offset
     * @param len_out
     * @return const char*
     */
    const char *get(Elf64::Off offset, size_t *len_out) const;

    /**
     * @brief 返回该字符串表中给定偏移量处的字符串
     *
     * @param offset
     * @return std::string
     */
    std::string get(Elf64::Off offset) const;

  private:
    struct impl;
    std::shared_ptr<impl> m;
};

/**
 * @brief 这是一个表示符号表中的符号的类
 *
 */
class sym
{
    const strtab strs;
    Sym<> data;

  public:
    /**
     * @brief 构造函数，根据给定的ELF文件、数据和字符串表创建一个符号对象
     *
     * @return const Sym<>&
     */
    sym(elf f, const void *data, strtab strs);

    /**
     * @brief 返回该符号的原始数据
     *
     * @return const Sym<>&
     */
    const Sym<> &get_data() const
    {
        return data;
    }

    /**
     * @brief 返回该符号的名称。这返回一个指向字符串表的指针，因此非常高效。如果len_out非空，则将设置为返回字符串的长度
     *
     * @param len_out
     * @return const char*
     */
    const char *get_name(size_t *len_out) const;

    /**
     * @brief 返回该符号的名称作为字符串
     *
     * @return std::string
     */
    std::string get_name() const;
};

/**
 * @brief 符号表
 *
 */
class symtab
{
  public:
    symtab() = default;
    /**
     * @brief 构造函数，根据给定的ELF文件、数据、大小和字符串表创建一个符号表对象
     *
     * @param f
     * @param data
     * @param size
     * @param strs
     */
    symtab(elf f, const void *data, size_t size, strtab strs);

    bool valid() const
    {
        return !!m;
    }

    class iterator
    {
        const elf f;
        const strtab strs; // 字符串表
        const char *pos;   // 位置
        size_t stride;     // 步长，即符号表每一项的长度

        iterator(const symtab &tab, const char *pos);
        friend class symtab;

      public:
        sym operator*() const
        {
            return sym(f, pos, strs);
        }

        iterator &operator++()
        {
            return *this += 1;
        }

        iterator operator++(int)
        {
            iterator cur(*this);
            *this += 1;
            return cur;
        }

        iterator &operator+=(std::ptrdiff_t x)
        {
            pos += x * stride;
            return *this;
        }

        iterator &operator-=(std::ptrdiff_t x)
        {
            pos -= x * stride;
            return *this;
        }

        bool operator==(iterator &o) const
        {
            return pos == o.pos;
        }

        bool operator!=(iterator &o) const
        {
            return pos != o.pos;
        }
    };

    /**
     * @brief 返回一个迭代器，指向第一个符号
     *
     * @return iterator
     */
    iterator begin() const;

    /**
     * @brief 返回一个迭代器，指向最后一个符号的下一个位置
     *
     * @return iterator
     */
    iterator end() const;

  private:
    struct impl;
    std::shared_ptr<impl> m;
};
} // namespace elf

#endif