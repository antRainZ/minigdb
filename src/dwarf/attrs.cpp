#include "dwarf/dwarf.hpp"
using namespace std;

namespace dwarf
{
/**
 * @brief 根据name 返回 die
 * 
 */
#define AT_ANY(name)              \
    value at_##name(const die &d) \
    {                             \
        return d[DW_AT::name];    \
    }                             \
    static_assert(true, "")

/**
 * @brief 根据name 返回 die 的地址
 * 
 */
#define AT_ADDRESS(name)                    \
    taddr at_##name(const die &d)           \
    {                                       \
        return d[DW_AT::name].as_address(); \
    }                                       \
    static_assert(true, "")
/**
 * @brief 根据name 返回 die 的无符号常量
 * 
 */
#define AT_ENUM(name, type)                         \
    type at_##name(const die &d)                    \
    {                                               \
        return (type)d[DW_AT::name].as_uconstant(); \
    }                                               \
    static_assert(true, "")

/**
 * @brief 根据name 返回 die 的flag
 * 
 */
#define AT_FLAG(name)                    \
    bool at_##name(const die &d)         \
    {                                    \
        return d[DW_AT::name].as_flag(); \
    }                                    \
    static_assert(true, "")

#define AT_FLAG_(name)                      \
    bool at_##name(const die &d)            \
    {                                       \
        return d[DW_AT::name##_].as_flag(); \
    }                                       \
    static_assert(true, "")

#define AT_REFERENCE(name)                    \
    die at_##name(const die &d)               \
    {                                         \
        return d[DW_AT::name].as_reference(); \
    }                                         \
    static_assert(true, "")

#define AT_STRING(name)                    \
    string at_##name(const die &d)         \
    {                                      \
        return d[DW_AT::name].as_string(); \
    }                                      \
    static_assert(true, "")

/**
 * @brief 用于处理动态属性。该宏接受一个额外的参数expr_context *ctx，用于指定表达式的上下文
 * 
 */
#define AT_UDYNAMIC(name)                               \
    uint64_t at_##name(const die &d, expr_context *ctx) \
    {                                                   \
        return _at_udynamic(DW_AT::name, d, ctx);       \
    }                                                   \
    static_assert(true, "")

/**
 * @brief 用于获取动态属性的值
 * 函数通过调用die对象的operator[]函数来获取属性的值
 * @param attr 深度超过了16，则抛出一个异常。
 * @param d 
 * @param ctx 
 * @param depth 
 * @return uint64_t 
 */
static uint64_t _at_udynamic(DW_AT attr, const die &d, expr_context *ctx, int depth = 0)
{
    // DWARF4 section 2.19
    if (depth > 16)
        throw format_error("reference depth exceeded for " + to_string(attr));

    value v(d[attr]);
    switch (v.get_type()) {
    case value::type::constant:
    case value::type::uconstant:
        return v.as_uconstant();
    case value::type::reference:
        return _at_udynamic(attr, v.as_reference(), ctx, depth + 1);
    case value::type::exprloc:
        return v.as_exprloc().evaluate(ctx).value;
    default:
        throw format_error(to_string(attr) + " has unexpected type " +
                           to_string(v.get_type()));
    }
}



AT_REFERENCE(sibling);
AT_STRING(name);
AT_ENUM(ordering, DW_ORD);
AT_UDYNAMIC(byte_size);
AT_UDYNAMIC(bit_offset);
AT_UDYNAMIC(bit_size);

AT_ADDRESS(low_pc);
/**
 * @brief 用于获取DW_AT::high_pc属性的值
 * 果属性值的类型是地址，函数将直接返回该地址。
 * 如果属性值的类型是常量或无符号常量，函数将通过调用at_low_pc函数获取DW_AT::low_pc属性的值，并将其与属性值相加，然后返回结果。
 * @param d 
 * @return taddr 
 */
taddr at_high_pc(const die &d)
{
    value v(d[DW_AT::high_pc]);
    switch (v.get_type()) {
    case value::type::address:
        return v.as_address();
    case value::type::constant:
    case value::type::uconstant:
        return at_low_pc(d) + v.as_uconstant();
    default:
        throw format_error(to_string(DW_AT::high_pc) + " has unexpected type " +
                           to_string(v.get_type()));
    }
}
AT_ENUM(language, DW_LANG);
AT_REFERENCE(discr);
AT_ANY(discr_value);
AT_ENUM(visibility, DW_VIS);
AT_REFERENCE(import);
AT_REFERENCE(common_reference);
AT_STRING(comp_dir);
AT_ANY(const_value);
AT_REFERENCE(containing_type);

DW_INL at_inline(const die &d)
{
    return (DW_INL)d[DW_AT::inline_].as_uconstant();
}
AT_FLAG(is_optional);
AT_UDYNAMIC(lower_bound); 
AT_STRING(producer);
AT_FLAG(prototyped);
AT_UDYNAMIC(bit_stride);
AT_UDYNAMIC(upper_bound);

AT_REFERENCE(abstract_origin);
AT_ENUM(accessibility, DW_ACCESS);
AT_FLAG(artificial);
AT_ENUM(calling_convention, DW_CC);
AT_UDYNAMIC(count);

/**
 * @brief 用于获取DW_AT::data_member_location属性的值
 * 如果属性值的类型是常量或无符号常量，函数将返回一个expr_result结构体，其中location_type字段设置为expr_result::type::address，value字段设置为基地址加上属性值，implicit字段设置为nullptr，implicit_len字段设置为0。
 * 如果属性值的类型是表达式位置，函数将通过调用evaluate函数来计算表达式的值，并返回一个expr_result结构体
 * @param d 
 * @param ctx 
 * @param base 
 * @param pc 
 * @return expr_result 
 */
expr_result at_data_member_location(const die &d, expr_context *ctx, taddr base, taddr pc)
{
    value v(d[DW_AT::data_member_location]);
    switch (v.get_type()) {
    case value::type::constant:
    case value::type::uconstant:
        return {.location_type = expr_result::type::address,
                .value = base + v.as_uconstant(),
                .implicit = nullptr,
                .implicit_len = 0};
    case value::type::exprloc:
        return v.as_exprloc().evaluate(ctx, base);
    case value::type::loclist:
        throw std::runtime_error("not implemented");
    default:
        throw format_error("DW_AT_data_member_location has unexpected type " +
                           to_string(v.get_type()));
    }
}

AT_FLAG(declaration);
AT_ENUM(encoding, DW_ATE);
AT_FLAG(external);

die at_friend(const die &d)
{
    return d[DW_AT::friend_].as_reference();
}
AT_ENUM(identifier_case, DW_ID);
AT_REFERENCE(namelist_item);
AT_REFERENCE(priority);
AT_REFERENCE(specification);
AT_REFERENCE(type);
AT_FLAG(variable_parameter);
AT_ENUM(virtuality, DW_VIRTUALITY);
AT_UDYNAMIC(allocated);
AT_UDYNAMIC(associated);


AT_UDYNAMIC(byte_stride);
AT_ADDRESS(entry_pc);
AT_FLAG(use_UTF8);
AT_REFERENCE(extension);
rangelist at_ranges(const die &d)
{
    return d[DW_AT::ranges].as_rangelist();
}
AT_STRING(description);
AT_REFERENCE(small);


AT_STRING(picture_string);
AT_FLAG_(mutable);
AT_FLAG(threads_scaled);
AT_FLAG_(explicit);
AT_REFERENCE(object_pointer);
AT_ENUM(endianity, DW_END);
AT_FLAG(elemental);
AT_FLAG(pure);
AT_FLAG(recursive);
AT_REFERENCE(signature);
AT_FLAG(main_subprogram);
AT_FLAG(const_expr);
AT_FLAG(enum_class);
AT_STRING(linkage_name);

/**
 * @brief 用于获取die对象的PC范围
 * 
 * @param d 
 * @return rangelist 
 */
rangelist die_pc_range(const die &d)
{

    if (d.has(DW_AT::ranges))
        return at_ranges(d);
    taddr low = at_low_pc(d);
    taddr high = d.has(DW_AT::high_pc) ? at_high_pc(d) : (low + 1);
    return rangelist({{low, high}});
}

} // namespace dwarf