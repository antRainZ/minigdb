#include "dwarf/internal.hpp"
using namespace std;

namespace dwarf
{
rangelist::rangelist(const std::shared_ptr<section> &sec, section_offset off,
                     unsigned cu_addr_size, taddr cu_low_pc)
    : sec(sec->slice(off, ~0, format::unknown, cu_addr_size)),
      base_addr(cu_low_pc)
{
}

rangelist::rangelist(const initializer_list<pair<taddr, taddr>> &ranges)
{
    synthetic.reserve(ranges.size() * 2 + 2);
    for (auto &range : ranges) {
        synthetic.push_back(range.first);
        synthetic.push_back(range.second);
    }
    synthetic.push_back(0);
    synthetic.push_back(0);

    sec = make_shared<section>(
        section_type::ranges, (const char *)synthetic.data(),
        synthetic.size() * sizeof(taddr),
        native_order(), format::unknown, sizeof(taddr));

    base_addr = 0;
}

rangelist::iterator rangelist::begin() const
{
    if (sec)
        return iterator(sec, base_addr);
    return end();
}

rangelist::iterator rangelist::end() const
{
    return iterator();
}

bool rangelist::contains(taddr addr) const
{
    for (auto ent : *this)
        if (ent.contains(addr))
            return true;
    return false;
}

rangelist::iterator::iterator(const std::shared_ptr<section> &sec, taddr base_addr)
    : sec(sec), base_addr(base_addr), pos(0)
{
    ++(*this);
}

rangelist::iterator & rangelist::iterator::operator++()
{

    // 初始化为无符号整数的最大值
    taddr largest_offset = ~(taddr)0;
    if (sec->addr_size < sizeof(taddr))
        largest_offset += 1 << (8 * sec->addr_size);

    cursor cur(sec, pos);
    while (true) {
        entry.low = cur.address();
        entry.high = cur.address();

        if (entry.low == 0 && entry.high == 0) {
            // 表示到达了范围列表的结尾
            sec.reset();
            pos = 0;
            break;
        } else if (entry.low == largest_offset) {
            // 表示基地址发生了更改
            base_addr = entry.high;
        } else {
            //  如果读取到的是正常条目，
            entry.low += base_addr;
            entry.high += base_addr;
            pos = cur.get_section_offset();
            break;
        }
    }

    return *this;
}

} // namespace dwarf