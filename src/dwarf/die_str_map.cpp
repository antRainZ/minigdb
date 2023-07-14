#include "dwarf/internal.hpp"
#include <cstring>
#include <unordered_set>

using namespace std;

namespace std
{
template <>
struct hash<dwarf::DW_TAG> {
    typedef size_t result_type;
    typedef dwarf::DW_TAG argument_type;
    result_type operator()(argument_type a) const
    {
        return (result_type)a;
    }
};
} // namespace std

namespace dwarf
{
struct string_hash {
    typedef size_t result_type;
    typedef const char *argument_type;
    result_type operator()(const char *s) const
    {
        result_type h = 0;
        for (; *s; ++s)
            h += 33 * h + *s;
        return h;
    }
};

struct string_eq {
    typedef bool result_type;
    typedef const char *first_argument_type;
    typedef const char *second_argument_type;
    bool operator()(const char *x, const char *y) const
    {
        return strcmp(x, y) == 0;
    }
};

struct die_str_map::impl {
    impl(const die &parent, DW_AT attr,
         const initializer_list<DW_TAG> &accept)
        : attr(attr), accept(accept.begin(), accept.end()),
          pos(parent.begin()), end(parent.end()) {}
    // 用于存储字符串值和对应的die对象的映射关系
    unordered_map<const char *, die, string_hash, string_eq> str_map;
    DW_AT attr;
    // 用于存储允许的标签集合
    unordered_set<DW_TAG> accept;
    die::iterator pos, end;
    die invalid;
};

die_str_map::die_str_map(const die &parent, DW_AT attr,
                         const initializer_list<DW_TAG> &accept)
    : m(make_shared<impl>(parent, attr, accept))
{
}

die_str_map die_str_map::from_type_names(const die &parent)
{
    return die_str_map(parent, DW_AT::name,
                       {DW_TAG::array_type, DW_TAG::class_type,
                        DW_TAG::enumeration_type, DW_TAG::pointer_type,
                        DW_TAG::reference_type, DW_TAG::string_type,
                        DW_TAG::structure_type, DW_TAG::subroutine_type,
                        DW_TAG::union_type, DW_TAG::ptr_to_member_type,
                        DW_TAG::set_type, DW_TAG::subrange_type,
                        DW_TAG::base_type, DW_TAG::const_type,
                        DW_TAG::file_type, DW_TAG::packed_type,
                        DW_TAG::volatile_type, DW_TAG::restrict_type,
                        DW_TAG::interface_type, DW_TAG::unspecified_type,
                        DW_TAG::shared_type, DW_TAG::rvalue_reference_type});
}

/**
 * @brief 用于在die_str_map对象中查找给定值的die对象
 * 
 * @param val 
 * @return const die& 
 */
const die & die_str_map::operator[](const char *val) const
{
    auto it = m->str_map.find(val);
    if (it != m->str_map.end())
        return it->second;
    while (m->pos != m->end) {
        const die &d = *m->pos;
        ++m->pos;

        if (!m->accept.count(d.tag) || !d.has(m->attr))
            continue;
        value dval(d[m->attr]);
        if (dval.get_type() != value::type::string)
            continue;
        const char *dstr = dval.as_cstr();
        m->str_map[dstr] = d;
        if (strcmp(val, dstr) == 0)
            return m->str_map[dstr];
    }
    return m->invalid;
}

} // namespace dwarf