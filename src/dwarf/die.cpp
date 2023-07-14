#include "dwarf/internal.hpp"
using namespace std;

namespace dwarf
{
die::die(const unit *cu)
    : cu(cu), abbrev(nullptr)
{
}

const unit & die::get_unit() const
{
    return *cu;
}

section_offset die::get_section_offset() const
{
    return cu->get_section_offset() + offset;
}

void die::read(section_offset off)
{
    cursor cur(cu->data(), off);

    offset = off;

    abbrev_code acode = cur.uleb128();
    if (acode == 0) {
        // 如果acode的值为0，表示没有缩写码，即当前die对象不包含任何属性
        abbrev = nullptr;
        next = cur.get_section_offset();
        return;
    }
    // 获取对应的缩写条目，并将其地址存储在abbrev指针中
    abbrev = &cu->get_abbrev(acode);
    tag = abbrev->tag;
    attrs.clear();
    attrs.reserve(abbrev->attributes.size());
    for (auto &attr : abbrev->attributes) {
        attrs.push_back(cur.get_section_offset());
        cur.skip_form(attr.form);
    }
    // 获取下一个die对象的偏移量
    next = cur.get_section_offset();
}

bool die::has(DW_AT attr) const
{
    if (!abbrev)
        return false;
    for (auto &a : abbrev->attributes)
        if (a.name == attr)
            return true;
    return false;
}

value die::operator[](DW_AT attr) const
{
    if (abbrev) {
        int i = 0;
        for (auto &a : abbrev->attributes) {
            if (a.name == attr)
                return value(cu, a.name, a.form, a.type, attrs[i]);
            i++;
        }
    }
    throw out_of_range("DIE does not have attribute " + to_string(attr));
}

value die::resolve(DW_AT attr) const
{
    if (has(attr))
        return (*this)[attr];

    if (has(DW_AT::abstract_origin)) {
        // 是一个具体的内联实例，并且存在一个对应的抽象实例
        die ao = (*this)[DW_AT::abstract_origin].as_reference();
        if (ao.has(attr))
            return ao[attr];
        // 如果抽象实例有DW_AT::specification属性，表示存在一个对应的具体实例，函数获取具体实例的die对象
        if (ao.has(DW_AT::specification)) {
            die s = ao[DW_AT::specification].as_reference();
            if (s.has(attr))
                return s[attr];
        }
    } else if (has(DW_AT::specification)) {
        die s = (*this)[DW_AT::specification].as_reference();
        if (s.has(attr))
            return s[attr];
    }

    return value();
}

die::iterator die::begin() const
{
    if (!abbrev || !abbrev->children)
        return end();
    return iterator(cu, next);
}

die::iterator::iterator(const unit *cu, section_offset off)
    : d(cu)
{
    d.read(off);
}

die::iterator & die::iterator::operator++()
{
    if (!d.abbrev)
        return *this;

    if (!d.abbrev->children) {
        d.read(d.next);
    } else if (d.has(DW_AT::sibling)) {
        d = d[DW_AT::sibling].as_reference();
    } else {
        iterator sub(d.cu, d.next);
        while (sub->abbrev)
            ++sub;
        d.read(sub->next);
    }

    return *this;
}

const vector<pair<DW_AT, value>> die::attributes() const
{
    vector<pair<DW_AT, value>> res;

    if (!abbrev)
        return res;

    int i = 0;
    for (auto &a : abbrev->attributes) {
        res.push_back(make_pair(a.name, value(cu, a.name, a.form, a.type, attrs[i])));
        i++;
    }
    return res;
}

bool die::operator==(const die &o) const
{
    return cu == o.cu && offset == o.offset;
}

bool die::operator!=(const die &o) const
{
    return !(*this == o);
}

bool die::contains_section_offset(section_offset off) const
{
    // 用于判断一个die对象是否包含给定的section_offset
    auto contains_off = [off](const die &d) { return off >= d.get_section_offset() && off < d.next; };

    if (contains_off(*this))
        return true;

    for (const auto &child : *this) {
        if (contains_off(child))
            return true;
    }

    return false;
}
} // namespace dwarf

size_t
std::hash<dwarf::die>::operator()(const dwarf::die &a) const
{
    return hash<decltype(a.cu)>()(a.cu) ^
           hash<decltype(a.get_unit_offset())>()(a.get_unit_offset());
}
