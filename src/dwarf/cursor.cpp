#include "dwarf/internal.hpp"
#include <cstring>
#include <stdexcept>

using namespace std;

namespace dwarf
{
int64_t cursor::sleb128()
{
    uint64_t result = 0;
    unsigned shift = 0;
    while (pos < sec->end) {
        uint8_t byte = *(uint8_t *)(pos++);
        result |= (uint64_t)(byte & 0x7f) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if (shift < sizeof(result) * 8 && (byte & 0x40))
                result |= -((uint64_t)1 << shift);
            return result;
        }
    }
    underflow();
    return 0;
}

shared_ptr<section> cursor::subsection()
{

    const char *begin = pos;
    section_length length = fixed<uword>();
    format fmt;
    if (length < 0xfffffff0) {
        fmt = format::dwarf32;
        length += sizeof(uword);
    } else if (length == 0xffffffff) {
        length = fixed<uint64_t>();
        fmt = format::dwarf64;
        length += sizeof(uword) + sizeof(uint64_t);
    } else {
        throw format_error("initial length has reserved value");
    }
    pos = begin + length;
    return make_shared<section>(sec->type, begin, length, sec->ord, fmt);
}

void cursor::skip_initial_length()
{
    switch (sec->fmt) {
    case format::dwarf32:
        pos += sizeof(uword);
        break;
    case format::dwarf64:
        pos += sizeof(uword) + sizeof(uint64_t);
        break;
    default:
        throw logic_error("cannot skip initial length with unknown format");
    }
}

void cursor::skip_unit_type()
{
    pos += sizeof(sbyte);
}

section_offset cursor::offset()
{
    switch (sec->fmt) {
    case format::dwarf32:
        return fixed<uint32_t>();
    case format::dwarf64:
        return fixed<uint64_t>();
    default:
        throw logic_error("cannot read offset with unknown format");
    }
}

void cursor::string(std::string &out)
{
    size_t size;
    const char *p = this->cstr(&size);
    out.resize(size);
    memmove(&out.front(), p, size);
}

const char * cursor::cstr(size_t *size_out)
{
    const char *p = pos;
    while (pos < sec->end && *pos)
        pos++;
    if (pos == sec->end)
        throw format_error("unterminated string");
    if (size_out)
        *size_out = pos - p;
    pos++;
    return p;
}

void cursor::skip_form(DW_FORM form)
{
    section_offset tmp;
    switch (form) {
    case DW_FORM::addr:
        pos += sec->addr_size;
        break;
    case DW_FORM::sec_offset:
    case DW_FORM::ref_addr:
    case DW_FORM::strp:
        switch (sec->fmt) {
        case format::dwarf32:
            pos += 4;
            break;
        case format::dwarf64:
            pos += 8;
            break;
        case format::unknown:
            throw logic_error("cannot read form with unknown format");
        }
        break;
    case DW_FORM::block1:
        tmp = fixed<ubyte>();
        pos += tmp;
        break;
    case DW_FORM::block2:
        tmp = fixed<uhalf>();
        pos += tmp;
        break;
    case DW_FORM::block4:
        tmp = fixed<uword>();
        pos += tmp;
        break;
    case DW_FORM::block:
    case DW_FORM::exprloc:
        tmp = uleb128();
        pos += tmp;
        break;

    // 固定长度
    case DW_FORM::flag_present:
        break;
    case DW_FORM::flag:
    case DW_FORM::data1:
    case DW_FORM::ref1:
        pos += 1;
        break;
    case DW_FORM::data2:
    case DW_FORM::ref2:
        pos += 2;
        break;
    case DW_FORM::data4:
    case DW_FORM::ref4:
        pos += 4;
        break;
    case DW_FORM::data8:
    case DW_FORM::ref_sig8:
        pos += 8;
        break;

    // 变长
    case DW_FORM::sdata:
    case DW_FORM::udata:
    case DW_FORM::ref_udata:
        while (pos < sec->end && (*(uint8_t *)pos & 0x80))
            pos++;
        pos++;
        break;
    case DW_FORM::string:
        while (pos < sec->end && *pos)
            pos++;
        pos++;
        break;

    case DW_FORM::indirect:
        skip_form((DW_FORM)uleb128());
        break;

    default:
        throw format_error("unknown form " + to_string(form));
    }
}

void cursor::underflow()
{
    throw underflow_error("cannot read past end of DWARF section");
}
} // namespace dwarf