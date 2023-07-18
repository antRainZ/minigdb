#ifndef FAULT_INJECT_DWARF_DWARF_H
#define FAULT_INJECT_DWARF_DWARF_H

#include "data.hpp"
#include "small_vector.hpp"

#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace dwarf
{

class dwarf;
class loader;
class compilation_unit;
class type_unit;
class die;
class value;
class expr;
class expr_context;
class expr_result;
class loclist;
class rangelist;
class line_table;

// 内部使用
struct section;
struct abbrev_entry;
struct cursor;

// XXX Big missing support: .debug_aranges, .debug_frame, loclists,macros

/**
 * @brief 用于表示DWARF数据格式错误
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
 * @brief 这段代码定义了一个枚举类型section_type，表示DWARF节的类型。
 * 这些类型对应于ELF节的名称
 * abbrev: 包含了DWARF调试信息的缩写表，用于解码其他节中的数据
 * aranges: 包含了地址范围信息，用于快速查找给定地址所属的编译单元
 * frame: 包含了调用帧信息，用于在调试器中还原函数调用堆栈
 * info: 包含了DWARF调试信息的主要内容，包括编译单元、类型、变量、函数等的描述信息
 * line: 包含了源代码行号信息，用于在调试器中显示源代码的行号和文件名
 * loc: 包含了位置表，用于描述变量或表达式的内存位置
 * macinfo: 包含了宏定义和宏调用的信息
 * pubnames: 包含了全局变量和函数的名称及其对应的地址
 * pubtypes: 包含了类型定义的名称及其对应的地址
 * ranges: 包含了地址范围信息，用于描述编译单元中的函数或变量的地址范围
 * str: 包含了字符串表，用于存储其他节中使用的字符串
 * types: 包含了类型定义的详细信息，包括结构体、枚举、联合等的成员及其类型
 */
enum class section_type {
    abbrev,
    aranges,
    frame,
    info,
    line,
    loc,
    macinfo,
    pubnames,
    pubtypes,
    ranges,
    str,
    types,
};

std::string to_string(section_type v);

///////////////////////////// dwarf /////////////////////////////////
/**
 * @brief 表示一个DWARF文件
 *
 */
class dwarf
{
  public:
    /**
     * @brief 据给定的加载器读取DWARF文件的节，并创建一个dwarf对象
     *
     * @param l
     */
    explicit dwarf(const std::shared_ptr<loader> &l);

    /**
     * 创建一个初始无效的dwarf对象
     */
    dwarf() = default;
    dwarf(const dwarf &) = default;
    dwarf(dwarf &&) = default;
    ~dwarf() {}

    dwarf &operator=(const dwarf &o) = default;
    dwarf &operator=(dwarf &&o) = default;

    bool operator==(const dwarf &o) const
    {
        return m == o.m;
    }

    bool operator!=(const dwarf &o) const
    {
        return m != o.m;
    }

    /**
     * @brief 判断该对象是否表示一个有效的DWARF文件
     *
     * @return true
     * @return false
     */
    bool valid() const
    {
        return !!m;
    }

    /**
     * @brief 返回该DWARF文件中的 compilation_units 列表
     *
     * @return const std::vector<compilation_unit>&
     */
    const std::vector<compilation_unit> &compilation_units() const;

    /**
     * @brief 根据给定的类型签名返回对应的类型单元。
     * 如果找不到对应的类型单元，则抛出out_of_range异常
     *
     * @param type_signature
     * @return const type_unit&
     */
    const type_unit &get_type_unit(uint64_t type_signature) const;

    /**
     * @brief 从该文件中获取指定类型的节。如果节不存在，则抛出format_error异常
     *
     * @param type
     * @return std::shared_ptr<section>
     */
    std::shared_ptr<section> get_section(section_type type) const;

  private:
    struct impl;
    // elf 的 loader
    std::shared_ptr<impl> m;
};

/**
 * @brief 用于延迟加载DWARF 的 section
 *
 */
class loader
{
  public:
    virtual ~loader() {}

    /**
     * @brief 加载请求的DWARF节到内存中，并返回指向其开头的指针。
     * 这段内存必须在加载器被销毁之前保持有效且不变。
     * 如果请求的节不存在，应该返回nullptr
     * 如果节存在但由于任何原因无法加载，应该抛出异常
     *
     * @param section
     * @param size_out
     * @return const void*
     */
    virtual const void *load(section_type section, size_t *size_out) = 0;
};

/**
 * @brief 用于表示DWARF文件中的编译单元或类型单元。
 * 一个单元由一棵有根的DIE（Debugging Information Entry）树组成，
 * 以及依赖于单元类型的其他元数据
 *
 */
class unit
{
  public:
    virtual ~unit() = 0;

    bool operator==(const unit &o) const
    {
        return m == o.m;
    }

    bool operator!=(const unit &o) const
    {
        return m != o.m;
    }

    /**
     * @brief 返回该对象是否有效。默认构造的unit对象是无效的
     *
     * @return true
     * @return false
     */
    bool valid() const
    {
        return !!m;
    }

    /**
     * @brief 返回该单元所属的DWARF文件
     *
     * @return const dwarf&
     */
    const dwarf &get_dwarf() const;

    /**
     * @brief 返回该单元头部在其所属节（.debug_info或.debug_types）中的字节偏移量
     *
     * @return section_offset
     */
    section_offset get_section_offset() const;

    /**
     * @brief 返回该单元的根DIE（Debugging Information Entry）。对于编译单元，
     * 根DIE应该是DW_TAG::compilation_unit或DW_TAG::partial_unit
     *
     * @return const die&
     */
    const die &root() const;

    /**
     * @brief 返回该单元的数据
     *
     * @return const std::shared_ptr<section>&
     */
    const std::shared_ptr<section> &data() const;

    /**
     * @brief 返回指定abbrev code的abbrev
     *
     * @param acode
     * @return const abbrev_entry&
     */
    const abbrev_entry &get_abbrev(std::uint64_t acode) const;

  protected:
    friend struct ::std::hash<unit>;
    // impl结构是一个私有结构体，用于存储unit类的实现细节
    struct impl;
    std::shared_ptr<impl> m;
};

/**
 * @brief 示一个DWARF文件中的编译单元（compilation unit）。
 * 在DWARF文件中，大部分信息都是按编译单元进行划分的。
 * 该类是内部引用计数的，并且可以高效地进行复制
 *
 */
class compilation_unit : public unit
{
  public:
    compilation_unit() = default;
    compilation_unit(const compilation_unit &o) = default;
    compilation_unit(compilation_unit &&o) = default;

    compilation_unit &operator=(const compilation_unit &o) = default;
    compilation_unit &operator=(compilation_unit &&o) = default;

    /**
     * @brief 用于在文件的.debug_info节中指定的偏移处开始构造一个编译单元
     *
     * @param file
     * @param offset
     */
    compilation_unit(const dwarf &file, section_offset offset);

    /**
     * @brief 返回该编译单元的行号表（line table）。
     * 如果该编译单元没有行号表，则返回一个无效的行号表对象
     *
     * @return const line_table&
     */
    const line_table &get_line_table() const;
};

/**
 * @brief 表示一个类型单元
 *
 */
class type_unit : public unit
{
  public:
    type_unit() = default;
    type_unit(const type_unit &o) = default;
    type_unit(type_unit &&o) = default;

    type_unit &operator=(const type_unit &o) = default;
    type_unit &operator=(type_unit &&o) = default;

    /**
     * @brief 根据给定的DWARF文件和偏移量创建一个类型单元
     *
     * @param file
     * @param offset
     */
    type_unit(const dwarf &file, section_offset offset);

    /**
     * @brief 返回一个64位的唯一签名，用于标识该类型单元
     *
     * @return uint64_t
     */
    uint64_t get_type_signature() const;

    /**
     * @brief 返回描述该类型单元的DIE
     * 如果类型嵌套在命名空间或其他结构中，可能不是该单元的根DIE。
     *
     * @return const die&
     */
    const die &type() const;
};

///////////////////////////// DIE /////////////////////////////////

class die
{

  public:
    DW_TAG tag; // DIE的标签，用于标识DIE的类型

    die() : cu(nullptr), abbrev(nullptr) {}
    die(const die &o) = default;
    die(die &&o) = default;

    die &operator=(const die &o) = default;
    die &operator=(die &&o) = default;

    /**
     * @brief 是否有效
     *
     * @return true 该对象表示DWARF文件中的DIE
     * @return false 某些方法返回无效的DIE以指示失败
     */
    bool valid() const
    {
        return abbrev != nullptr;
    }

    /**
     * @brief 返回包含该DIE的单元
     *
     * @return const unit&
     */
    const unit &get_unit() const;

    /**
     * @brief返回该DIE在其编译单元中的字节偏移量
     *
     * @return section_offset
     */
    section_offset get_unit_offset() const
    {
        return offset;
    }

    /**
     * @brief 返回该DIE在其节中的字节偏移量
     *
     * @return section_offset
     */
    section_offset get_section_offset() const;

    /**
     * @brief 如果该DIE具有指定的属性，则返回true
     *
     * @param attr
     * @return true
     * @return false
     */
    bool has(DW_AT attr) const;

    /**
     * @brief 返回属性attr的值。如果该DIE没有指定属性，则抛出out_of_range异常
     *
     * @param attr
     * @return value
     */
    value operator[](DW_AT attr) const;

    /**
     * @brief 在解析规范和抽象起源引用后返回属性attr的值。
     * 如果无法解析属性，则返回无效值。
     * 声明DIE可以“完成”先前的非定义声明DIE，并继承非定义声明的属性。
     * 同样，任何作为具体内联实例的子DIE都可以指定另一个DIE作为其“抽象起源”，
     * 原始DIE将继承其抽象起源的属性
     *
     * @param attr
     * @return value
     */
    value resolve(DW_AT attr) const;

    class iterator;

    /**
     * @brief 此迭代器返回的DIE是临时的，因此如果需要在多个循环迭代中存储DIE，则必须复制它
     *
     * @return iterator
     */
    iterator begin() const;
    iterator end() const;

    /**
     * @brief 返回该DIE的属性的向量
     *
     * @return const std::vector<std::pair<DW_AT, value>>
     */
    const std::vector<std::pair<DW_AT, value>> attributes() const;

    bool operator==(const die &o) const;
    bool operator!=(const die &o) const;

    /**
     * @brief 如果给定的节偏移量包含在该DIE中，则返回true
     *
     * @param off
     * @return true
     * @return false
     */
    bool contains_section_offset(section_offset off) const;

  private:
    friend class unit;
    friend class type_unit;
    friend class value;
    // XXX If we can get the CU, we don't need this
    friend struct ::std::hash<die>;

    const unit *cu;
    /**
     * @brief 该DIE的缩写。按照约定，如果该DIE表示兄弟列表的终止符，则该指针为空。
     * 该对象由编译单元（CU）保持活动状态
     *
     */
    const abbrev_entry *abbrev;
    // 该DIE相对于编译单元的起始位置的偏移量
    section_offset offset;
    /**
     * @brief 属性的偏移量，相对于编译单元的子节。
     * 但大多数DIE往往具有六个或更少的属性，默认使用六个属性的空间
     *
     */
    small_vector<section_offset, 6> attrs;
    /**
     * @brief 下一个DIE的偏移量，相对于编译单元的子节。
     * 即使对于兄弟列表的终止符，也会设置该偏移量
     *
     */
    section_offset next;

    die(const unit *cu);

    /**
     * @brief 从给定的偏移量在编译单元中读取该DIE
     *
     * @param off
     */
    void read(section_offset off);
};

/**
 * @brief 用于遍历一系列兄弟DIEs
 *
 */
class die::iterator
{
  public:
    iterator() = default;
    iterator(const iterator &o) = default;
    iterator(iterator &&o) = default;

    iterator &operator=(const iterator &o) = default;
    iterator &operator=(iterator &&o) = default;

    const die &operator*() const
    {
        return d;
    }

    const die *operator->() const
    {
        return &d;
    }

    bool operator!=(const iterator &o) const
    {
        if (d.abbrev != o.d.abbrev)
            return true;
        if (d.abbrev == nullptr)
            return false;
        return d.next != o.d.next || d.cu != o.d.cu;
    }
    /**
     * @brief
     * 1）如果不存在缩写码，直接返回this
     * 2) 如果当前`die`对象没有子节点, 读取下一个die
     * 3) 如果有兄弟节点，将迭代器指向兄弟节点
     * 4）否则循环遍历 sub
     * @return iterator& *this
     */
    iterator &operator++();

  private:
    friend class die;

    iterator(const unit *cu, section_offset off);

    die d;
};

inline die::iterator die::end() const
{
    return iterator();
}

/**
 * @brief 异常类，表示值的类型不匹配
 *
 */
class value_type_mismatch : public std::logic_error
{
  public:
    explicit value_type_mismatch(const std::string &what_arg)
        : std::logic_error(what_arg) {}
    explicit value_type_mismatch(const char *what_arg)
        : std::logic_error(what_arg) {}
};

/**
 * @brief 表示 DIE 属性值
 *
 */
class value
{
  public:
    enum class type {
        invalid,
        address,
        block,
        constant,
        uconstant,
        sconstant,
        exprloc,
        flag,
        line,
        loclist,
        mac,
        rangelist,
        reference,
        string
    };

    /**
     * 默认构造函数，将typ初始化为 type::invalid
     */
    value() : cu(nullptr), typ(type::invalid) {}

    value(const value &o) = default;
    value(value &&o) = default;

    value &operator=(const value &o) = default;
    value &operator=(value &&o) = default;

    /**
     * @brief 表示该值是否有效
     *
     * @return true
     * @return false
     */
    bool valid() const
    {
        return typ != type::invalid;
    }

    /**
     * @brief 返回该值在编译单元内的字节偏移量。
     *
     * @return section_offset
     */
    section_offset get_unit_offset() const
    {
        return offset;
    }

    /**
     * @brief 返回该值在节内的字节偏移量
     *
     * @return section_offset
     */
    section_offset get_section_offset() const;

    /**
     * @brief 返回该值的类型
     *
     * @return type
     */
    type get_type() const
    {
        return typ;
    }

    /**
     * @brief 返回该值的属性编码
     *
     * @return DW_FORM
     */
    DW_FORM get_form() const
    {
        return form;
    }

    /**
     * @brief 将该值作为目标机器地址返回
     *
     * @return taddr
     */
    taddr as_address() const;

    /**
     * @brief 将该值作为块返回。返回的指针直接指向节数据，所以调用者必须确保数据在使用期间保持有效。
     * size_out指针被设置为返回块的长度（以字节为单位）
     *
     * @param size_out
     * @return const void*
     */
    const void *as_block(size_t *size_out) const;

    /**
     * @brief 将该值作为无符号常量返回。自动将"constant"类型的值强制转换为无符号类型
     *
     * @return uint64_t
     */
    uint64_t as_uconstant() const;

    /**
     * @brief 将该值作为有符号常量返回。自动将"constant"类型的值强制转换为有符号类型
     *
     * @return int64_t
     */
    int64_t as_sconstant() const;

    /**
     * @brief 将该值作为表达式返回。自动将"block"类型的值强制转换为表达式
     *
     * @return expr
     */
    expr as_exprloc() const;

    /**
     * @brief 将该值作为布尔标志返回
     *
     * @return true
     * @return false
     */
    bool as_flag() const;

    /**
     * @brief 将该值作为位置列表返回
     *
     * @return loclist
     */
    loclist as_loclist() const;

    /**
     * @brief 将该值作为范围列表返回
     *
     * @return rangelist
     */
    rangelist as_rangelist() const;

    /**
     * @brief 对于引用类型的值，返回引用的DIE
     *
     * @return die
     */
    die as_reference() const;

    /**
     * @brief 将该值作为字符串返回
     *
     * @return std::string
     */
    std::string as_string() const;

    /**
     * @brief 将该值的字符串值填充到给定的字符串缓冲区中
     *
     * @param buf
     */
    void as_string(std::string &buf) const;

    /**
     * @brief 将该值作为以NUL结尾的字符串返回。
     * 返回的指针直接指向节数据，所以调用者必须确保数据在使用期间保持有效。
     * 如果size_out不为nullptr，则将被设置为返回字符串的长度（不包括NUL终止符）
     *
     * @param size_out
     * @return const char*
     */
    const char *as_cstr(size_t *size_out = nullptr) const;

    /**
     * @brief 将该值作为节偏移量返回。
     * 适用于lineptr、loclistptr、macptr和rangelistptr
     *
     * @return section_offset
     */
    section_offset as_sec_offset() const;

  private:
    friend class die;

    value(const unit *cu,
          DW_AT name, DW_FORM form, type typ, section_offset offset);

    void resolve_indirect(DW_AT name);

    const unit *cu;
    DW_FORM form;
    type typ;
    section_offset offset;
};

std::string to_string(value::type v);

std::string to_string(const value &v);

///////////////////// 表达式和位置描述 //////////////////////////////

/**
 * 异常： 表达式解析错误
 */
class expr_error : public std::runtime_error
{
  public:
    explicit expr_error(const std::string &what_arg)
        : std::runtime_error(what_arg) {}
    explicit expr_error(const char *what_arg)
        : std::runtime_error(what_arg) {}
};

/**
 * 表示DWARF表达式或位置描述
 */
class expr
{
  public:
    /**
     * @brief 使用指定的表达式上下文评估该表达式，并返回结果。
     * 表达式堆栈将使用给定的参数初始化，使得第一个参数位于堆栈顶部，最后一个参数位于堆栈底部
     *
     * @param ctx
     * @return expr_result
     */
    expr_result evaluate(expr_context *ctx) const;

    /**
     * @brief 使用指定的表达式上下文和参数评估该表达式，并返回结果
     *
     * @param ctx
     * @param argument
     * @return expr_result
     */
    expr_result evaluate(expr_context *ctx, taddr argument) const;

    /**
     * @brief 使用指定的表达式上下文和参数列表评估该表达式，并返回结果
     *
     * @param ctx
     * @param arguments
     * @return expr_result
     */
    expr_result evaluate(expr_context *ctx, const std::initializer_list<taddr> &arguments) const;

  private:
    expr(const unit *cu,
         section_offset offset, section_length len);

    friend class value;

    const unit *cu;
    section_offset offset;
    section_length len;
};

/**
 * @brief 用于为表达式求值提供上下文信息。
 * 调用`expr::evaluate`的调用者需要继承此类，以向表达式求值引擎提供上下文信息。
 * 默认实现会为所有方法抛出`expr_error`异常。
 *
 */
class expr_context
{
  public:
    virtual ~expr_context() {}

    /**
     * @brief 返回寄存器regnum中存储的值。用于实现DW_OP_breg*操作
     *
     * @param regnum
     * @return taddr
     */
    virtual taddr reg(unsigned regnum)
    {
        throw expr_error("DW_OP_breg* operations not supported");
    }

    /**
     * @brief 实现 DW_OP_deref_size 操作
     *
     * @param address
     * @param size
     * @return taddr
     */
    virtual taddr deref_size(taddr address, unsigned size)
    {
        throw expr_error("DW_OP_deref_size operations not supported");
    }

    /**
     * @brief 实现DW_OP_xderef_size操作
     *
     * @param address
     * @param asid
     * @param size
     * @return taddr
     */
    virtual taddr xderef_size(taddr address, taddr asid, unsigned size)
    {
        throw expr_error("DW_OP_xderef_size operations not supported");
    }

    /**
     * @brief 实现DW_OP_form_tls_address操作
     *
     * @param address
     * @return taddr
     */
    virtual taddr form_tls_address(taddr address)
    {
        throw expr_error("DW_OP_form_tls_address operations not supported");
    }

    /**
     * @brief 实现loclist操作
     *
     * @return taddr
     */
    virtual taddr pc()
    {
        throw expr_error("loclist operations not supported");
    }
};

// 这个实例对所有方法都抛出expr_error异常
extern expr_context no_expr_context;

/**
 * @brief 是一个表示DWARF表达式或位置描述求值结果的类
 *
 */
class expr_result
{
  public:
    /**
     * @brief ：用于描述此结果的位置类型。它是一个枚举类型，
     * address:表示对象在内存中的地址。这也是用于不涉及对象位置的通用表达式的结果类型
     * reg: 表示存储对象的寄存器
     * literal: 表示对象没有位置，value字段存储对象的值
     * implicit: 表示对象没有位置，其值由implicit字段指向
     * empty: 表示对象存在于源代码中，但在目标代码中不存在，因此没有位置或值
     *
     */
    enum class type {
        address,
        reg,
        literal,
        implicit,
        empty,
    };

    /**
     * @brief 用于描述此结果的位置类型
     *
     */
    type location_type;

    /**
     * @brief 对于通用表达式，它是表达式的结果值。
     * 对于地址位置描述，它是对象在内存中的地址。
     * 对于寄存器位置描述，它是存储对象的寄存器。
     * 对于文字位置描述，它是对象的值
     *
     */
    taddr value;

    /**
     * @brief 对于隐式位置描述，它是指向目标机器内存表示中值的块的指针
     *
     */
    const char *implicit;
    size_t implicit_len;
};

std::string to_string(expr_result::type v);

/**
 * @brief 位置列表
 *
 */
class loclist
{
  public:
    /**
     * @brief 使用指定的表达式上下文评估此位置列表的表达式。
     * 表达式堆栈将使用给定的参数进行初始化，使得第一个参数位于堆栈的顶部，最后一个参数位于堆栈的底部。如果评估表达式时出现错误（例如未知操作、堆栈下溢、边界错误等），
     * 则会抛出`expr_error`异常
     *
     * @param ctx
     * @return expr_result
     */
    expr_result evaluate(expr_context *ctx) const;

  private:
    loclist(const unit *cu,
            section_offset offset);

    friend class value;

    const unit *cu;
    section_offset offset;
};

/////////////////////////Range lists//////////////////////////////

/**
 * @brief 表示一个描述可能非连续地址集的DWARF范围列表
 *
 */
class rangelist
{
  public:
    /**
     * @brief根据给定的偏移量和地址大小构造一个范围列表
     *
     * @param sec 指向存储范围列表数据的section对象的共享指针
     * @param off 范围列表数据在section中的偏移量
     * @param cu_addr_size 关联编译单元的地址大小
     * @param cu_low_pc 包含引用DIE的编译单元的DW_AT::low_pc属性或0（用作范围列表的基地址）
     */
    rangelist(const std::shared_ptr<section> &sec, section_offset off,
              unsigned cu_addr_size, taddr cu_low_pc);

    /**
     * @brief 根据一系列{低地址，高地址}对构造一个范围列表
     *
     * @param ranges
     */
    rangelist(const std::initializer_list<std::pair<taddr, taddr>> &ranges);

    /**
     * 构造一个空的范围列表
     */
    rangelist() = default;
    rangelist(const rangelist &o) = default;
    rangelist(rangelist &&o) = default;
    rangelist &operator=(const rangelist &o) = default;
    rangelist &operator=(rangelist &&o) = default;

    class entry; // 表示范围列表的条目
    typedef entry value_type;

    class iterator;

    /**
     * @brief 返回一个迭代器，用于遍历范围列表中的条目。此迭代器返回的范围是临时的，
     * 因此如果需要在多个循环迭代中存储范围，必须进行复制
     *
     * @return iterator
     */
    iterator begin() const;
    iterator end() const;

    /**
     * @brief 如果范围列表包含给定的地址，则返回true
     *
     * @param addr
     * @return true
     * @return false
     */
    bool contains(taddr addr) const;

  private:
    std::vector<taddr> synthetic; // 用于存储合成的范围列表
    std::shared_ptr<section> sec;
    taddr base_addr; // 范围列表的基地址
};

/**
 * @brief 表示范围列表中的一个条目
 *
 */
class rangelist::entry
{
  public:
    taddr low, high; // 范围的起始地址，范围的结束地址（不包含在范围内）

    /**
     * @brief 如果给定的地址在此条目的范围内，则返回true。
     * 范围包括起始地址，但不包括结束地址
     *
     * @param addr
     * @return true
     * @return false
     */
    bool contains(taddr addr) const
    {
        return low <= addr && addr < high;
    }
};

/**
 * An iterator over a sequence of ranges in a range list.
 */
class rangelist::iterator
{
  public:
    iterator() : sec(nullptr), base_addr(0), pos(0) {}

    /**
     * @brief 根据给定的section对象和基地址构造一个迭代器
     *
     * @param sec
     * @param base_addr
     */
    iterator(const std::shared_ptr<section> &sec, taddr base_addr);
    iterator(const iterator &o) = default;
    iterator(iterator &&o) = default;
    iterator &operator=(const iterator &o) = default;
    iterator &operator=(iterator &&o) = default;

    /**
     * @brief 解引用运算符，返回当前范围列表条目的引用。
     * 这个条目在内部被重用，所以如果需要在下一次迭代之后保留它，调用者应该进行复制
     *
     * @return const rangelist::entry&
     */
    const rangelist::entry &operator*() const
    {
        return entry;
    }

    const rangelist::entry *operator->() const
    {
        return &entry;
    }

    bool operator==(const iterator &o) const
    {
        return sec == o.sec && pos == o.pos;
    }

    bool operator!=(const iterator &o) const
    {
        return !(*this == o);
    }

    /**
     * @brief 前置递增运算符，将迭代器递增到指向下一个范围列表条目
     *
     * @return iterator&
     */
    iterator &operator++();

  private:
    std::shared_ptr<section> sec;
    taddr base_addr;
    section_offset pos;
    rangelist::entry entry;
};

//////////////////////////// 行号表 //////////////////////////////

/**
 * @brief 表示DWARF行号表。行号表是一组按照"序列"划分的行表条目列表。
 * 在一个序列中，条目按照程序计数器（"address"）递增的顺序排列，并且一个条目提供了
 * 从该条目的地址到下一个条目的地址之间所有程序计数器的信息。每个序列以一个特殊的条目结束，该条目的`line_table::entry::end_sequence`标志被设置。
 * 行号表还记录了给定编译单元的一组源文件，可以从其他DIE属性中引用
 *
 */
class line_table
{
  public:
    line_table(const std::shared_ptr<section> &sec, section_offset offset,
               unsigned cu_addr_size, const std::string &cu_comp_dir,
               const std::string &cu_name);
    line_table() = default;
    line_table(const line_table &o) = default;
    line_table(line_table &&o) = default;
    line_table &operator=(const line_table &o) = default;
    line_table &operator=(line_table &&o) = default;

    /**
     * @brief 如果此对象表示一个已初始化的行号表，则返回true。
     *
     * @return true
     * @return false 默认构造的行号表无效
     */
    bool valid() const
    {
        return !!m;
    }

    class file;  // 表示行号表中的文件
    class entry; // 表示行号表中的条目
    typedef entry value_type;

    class iterator;
    iterator begin() const;
    iterator end() const;

    /**
     * @brief 返回一个迭代器，该迭代器指向包含地址`addr`的行号表条目（大致上是最接近或等于`addr`的地址的条目，但考虑到`end_sequence`条目）
     *
     * @param addr
     * @return iterator 如果没有这样的条目，则返回`end()`
     */
    iterator find_address(taddr addr) const;

    /**
     * @brief 返回行号表中第`index`个文件。这些索引通常用于声明和调用坐标。
     * 如果索引超出范围，将抛出`out_of_range`异常
     *
     * @param index
     * @return const file*
     */
    const file *get_file(unsigned index) const;

  private:
    friend class iterator;

    struct impl;
    std::shared_ptr<impl> m;
};

/**
 * @brief 表示行号表中的源文件
 *
 */
class line_table::file
{
  public:
    // 源文件的绝对路径
    std::string path;
    // 源文件的最后修改时间，以实现定义的编码表示，如果未知则为0
    uint64_t mtime;
    // 源文件的大小（以字节为单位），如果未知则为0
    uint64_t length;
    file(std::string path = "", uint64_t mtime = 0, uint64_t length = 0);
};

/**
 * @brief 表示行号表中的一个条目
 *
 */
class line_table::entry
{
  public:
    // 与编译器生成的机器指令对应的程序计数器值
    taddr address;
    // VLIW指令中操作的索引。第一个操作的索引为0。对于非VLIW架构，该值始终为0
    unsigned op_index;
    // 包含此指令的源文件
    const line_table::file *file;
    // 包含此指令的源文件的索引
    unsigned file_index;
    // 此指令的源代码行号，从1开始计数。如果无法将此指令归属到任何源代码行，则为0
    unsigned line;
    // 此指令在源代码行中的列号，从1开始计数。值为0表示语句从行的“左边缘”开始
    unsigned column;
    // 如果此指令是推荐的断点位置，则为true。通常，这是语句的开头
    bool is_stmt;
    // 如果此指令是基本块的开头，则为true
    bool basic_block;
    // 如果此地址是一系列目标机器指令结束后的第一个字节，则为true。在这种情况下，除了address以外的所有其他字段都没有意义
    bool end_sequence;
    // 如果此地址是函数的入口断点处，则为true
    bool prologue_end;
    // 如果此地址是函数的出口断点处，则为true
    bool epilogue_begin;
    // 此指令的指令集架构。该字段的含义通常由架构的ABI定义
    unsigned isa;
    // 如果与同一源文件、行号和列号关联的多个块，则标识包含当前指令的块的编号
    unsigned discriminator;
    /**
     * @brief 将此行信息对象重置为所有字段的默认初始值。
     * is_stmt没有默认值，所以调用者必须提供它
     *
     * @param is_stmt
     */
    void reset(bool is_stmt);
    /**
     * @brief 返回形如"filename[:line[:column]]"的描述性字符串
     *
     * @return std::string
     */
    std::string get_description() const;
};

/**
 * @brief
 *
 */
class line_table::iterator
{
  public:
    iterator() = default;
    iterator(const line_table *table, section_offset pos);
    iterator(const iterator &o) = default;
    iterator(iterator &&o) = default;
    iterator &operator=(const iterator &o) = default;
    iterator &operator=(iterator &&o) = default;
    const line_table::entry &operator*() const
    {
        return entry;
    }
    const line_table::entry *operator->() const
    {
        return &entry;
    }
    bool operator==(const iterator &o) const
    {
        return o.pos == pos && o.table == table;
    }
    bool operator!=(const iterator &o) const
    {
        return !(*this == o);
    }
    iterator &operator++();
    iterator operator++(int)
    {
        iterator tmp(*this);
        ++(*this);
        return tmp;
    }

  private:
    // 行号表对象
    const line_table *table;
    // 当前行号表条目和寄存器
    line_table::entry entry, regs;
    // 迭代器的偏移量
    section_offset pos;
    /**
     * @brief 处理下一个操作码
     *
     * @param cur
     * @return true 如果操作码“向表中添加一行”，则更新entry
     * @return false
     */
    bool step(cursor *cur);
};

//////////////////////////////////////////////////////////////////
// Type-safe attribute getters
//

// XXX More

die at_abstract_origin(const die &d);
DW_ACCESS at_accessibility(const die &d);
uint64_t at_allocated(const die &d, expr_context *ctx);
bool at_artificial(const die &d);
uint64_t at_associated(const die &d, expr_context *ctx);
uint64_t at_bit_offset(const die &d, expr_context *ctx);
uint64_t at_bit_size(const die &d, expr_context *ctx);
uint64_t at_bit_stride(const die &d, expr_context *ctx);
uint64_t at_byte_size(const die &d, expr_context *ctx);
uint64_t at_byte_stride(const die &d, expr_context *ctx);
DW_CC at_calling_convention(const die &d);
die at_common_reference(const die &d);
std::string at_comp_dir(const die &d);
value at_const_value(const die &d);
bool at_const_expr(const die &d);
die at_containing_type(const die &d);
uint64_t at_count(const die &d, expr_context *ctx);
expr_result at_data_member_location(const die &d, expr_context *ctx, taddr base, taddr pc);
bool at_declaration(const die &d);
std::string at_description(const die &d);
die at_discr(const die &d);
value at_discr_value(const die &d);
bool at_elemental(const die &d);
DW_ATE at_encoding(const die &d);
DW_END at_endianity(const die &d);
taddr at_entry_pc(const die &d);
bool at_enum_class(const die &d);
bool at_explicit(const die &d);
die at_extension(const die &d);
bool at_external(const die &d);
die at_friend(const die &d);
taddr at_high_pc(const die &d);
DW_ID at_identifier_case(const die &d);
die at_import(const die &d);
DW_INL at_inline(const die &d);
bool at_is_optional(const die &d);
DW_LANG at_language(const die &d);
std::string at_linkage_name(const die &d);
taddr at_low_pc(const die &d);
uint64_t at_lower_bound(const die &d, expr_context *ctx);
bool at_main_subprogram(const die &d);
bool at_mutable(const die &d);
std::string at_name(const die &d);
die at_namelist_item(const die &d);
die at_object_pointer(const die &d);
DW_ORD at_ordering(const die &d);
std::string at_picture_string(const die &d);
die at_priority(const die &d);
std::string at_producer(const die &d);
bool at_prototyped(const die &d);
bool at_pure(const die &d);
rangelist at_ranges(const die &d);
bool at_recursive(const die &d);
die at_sibling(const die &d);
die at_signature(const die &d);
die at_small(const die &d);
die at_specification(const die &d);
bool at_threads_scaled(const die &d);
die at_type(const die &d);
uint64_t at_upper_bound(const die &d, expr_context *ctx);
bool at_use_UTF8(const die &d);
bool at_variable_parameter(const die &d);
DW_VIRTUALITY at_virtuality(const die &d);
DW_VIS at_visibility(const die &d);

/**
 * @brief 用于返回由DIE的代码跨度的PC范围
 * DIE必须具有DW_AT::ranges或DW_AT::low_pc属性，
 * 可以选择具有DW_AT::high_pc属性
 * @param d
 * @return rangelist
 */
rangelist die_pc_range(const die &d);

//////////////////////////// 工具类 //////////////////////////////

/**
 * @brief 表示由某个字符串属性对兄弟DIE进行索引。该索引是惰性构建的，并且占用空间较少
 *
 */
class die_str_map
{
  public:
    die_str_map(const die &parent, DW_AT attr,
                const std::initializer_list<DW_TAG> &accept);
    die_str_map() = default;
    die_str_map(const die_str_map &o) = default;
    die_str_map(die_str_map &&o) = default;
    die_str_map &operator=(const die_str_map &o) = default;
    die_str_map &operator=(die_str_map &&o) = default;

    /**
     * @brief 静态函数，根据父DIE的直接子节点的类型名称构建字符串映射
     *
     * @param parent
     * @return die_str_map
     */
    static die_str_map from_type_names(const die &parent);

    /**
     * @brief 索引运算符，返回属性与val匹配的DIE。
     * 如果不存在这样的DIE，则返回一个无效的DIE对象
     *
     * @param val
     * @return const die&
     */
    const die &operator[](const char *val) const;

    /**
     * @brief 重载索引运算符，使用字符串作为参数
     *
     * @param val
     * @return const die&
     */
    const die &operator[](const std::string &val) const
    {
        return (*this)[val.c_str()];
    }

  private:
    struct impl;
    std::shared_ptr<impl> m;
};

/**
 * @brief 声明或内联实例的声明或调用坐标。
 *
 */
class coordinates
{
  public:
    enum class type {
        decl,
        call
    };

    coordinates(const die &die, type typ = type::decl) : d(die), t(typ) {}

    /**
     * @brief 用于返回包含该声明或调用的源文件的指针。如果没有指定源文件，则返回nullptr
     *
     * @return const line_table::file*
     */
    const line_table::file *get_file() const;

    /**
     * @brief 用于返回该声明或调用的第一个字符的源行号。如果没有指定源行号，则返回0
     *
     * @return unsigned
     */
    unsigned get_line() const;

    /**
     * @brief 用于返回该声明或调用的第一个字符的源列号。如果没有指定源列号，则返回0
     *
     * @return unsigned
     */
    unsigned get_column() const;

    /**
     * @brief 用于返回一个描述性字符串，格式为"filename[:line[:column]]"
     *
     * @return std::string
     */
    std::string get_description() const;

  private:
    const die &d;
    const type t;
};

/**
 * @brief 表示一个子程序、内联子程序或其他入口点
 *
 */
class subroutine
{
  public:
    subroutine(const die &die) : d(die) {}

    /**
     * @brief 用于返回该子程序的声明坐标
     *
     * @return coordinates
     */
    coordinates get_decl() const;

    /**
     * @brief 用于返回具体内联子程序的调用坐标。对于其他类型的子程序，返回的坐标部分都将是"unknown
     *
     * @return coordinates
     */
    coordinates get_call() const;

    /**
     * @brief 用于返回该子程序的名称。如果没有名称，则返回空字符串""
     *
     * @return std::string
     */
    std::string get_name() const;

  private:
    // 表示与该子程序相关联的DIE对象。
    const die &d;
};

///////////////////////////// ELF //////////////////////////////

namespace elf
{

/**
 * @brief 将一个ELF节名转换为对应的DWARF节类型
 * 返回是否是一个有效的DWARF节名
 * @param name 表示要转换的ELF节名
 * @param out 用于存储转换后的DWARF节类型
 * @return true
 * @return false
 */
bool section_name_to_type(const char *name, section_type *out);

/**
 * @brief 将DWARF节类型转换为ELF节名
 *
 * @param type
 * @return const char*
 */
const char *section_type_to_name(section_type type);

/**
 * @brief 用于加载ELF文件的DWARF节（section）的类模板
 *
 * @tparam Elf
 */
template <typename Elf>
class elf_loader : public loader
{
    Elf f;

  public:
    elf_loader(const Elf &file) : f(file) {}

    const void *load(section_type section, size_t *size_out)
    {
        auto sec = f.get_section(section_type_to_name(section));
        if (!sec.valid())
            return nullptr;
        *size_out = sec.size();
        return sec.data();
    }
};

/**
 * @brief 用于创建一个由给定ELF文件支持的DWARF节加载器。
 * 该函数的模板参数Elf用于消除libelf和libdwarf之间的静态依赖关系
 * 但它只能与libelf的elf::elf类型合理使用
 *
 * @tparam Elf
 * @param f
 * @return std::shared_ptr<elf_loader<Elf>>
 */
template <typename Elf>
std::shared_ptr<elf_loader<Elf>> create_loader(const Elf &f)
{
    return std::make_shared<elf_loader<Elf>>(f);
}
}; // namespace elf
} // namespace dwarf

/**
 * @brief 用于自定义哈希函数，以便在使用标准库容器时对dwarf::unit
 * 和dwarf::die类型的对象进行哈希操作
 *
 */
namespace std
{
template <>
struct hash<dwarf::unit> {
    // 表示哈希结果的类型
    typedef size_t result_type;
    // 表示哈希函数的参数类型
    typedef const dwarf::unit &argument_type;
    result_type operator()(argument_type a) const
    {
        return hash<decltype(a.m)>()(a.m);
    }
};

template <>
struct hash<dwarf::die> {
    typedef size_t result_type;
    typedef const dwarf::die &argument_type;
    result_type operator()(argument_type a) const;
};
} // namespace std

#endif