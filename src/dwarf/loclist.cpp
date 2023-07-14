#include "dwarf/internal.hpp"
#include <iostream>
#include <map>
using namespace std;

namespace dwarf
{
expr_result loclist::evaluate(expr_context *ctx) const
{
    cursor cur{cu->data(), offset};
    auto loc_offset = cur.fixed<uint32_t>();

    auto dwarf = cu->get_dwarf();
    auto debug_loc = dwarf.get_section(section_type::loc);

    cursor loc_cur{debug_loc, loc_offset};

    std::map<std::pair<uint64_t, uint64_t>, expr_result> ranges_to_locations;

    while (true) {
        auto start_addr = loc_cur.fixed<uint64_t>();
        auto end_addr = loc_cur.fixed<uint64_t>();
        expr_result result;

        if (start_addr == 0 && end_addr == 0) {
            break;
        }

        (void)loc_cur.fixed<uint16_t>();
        auto expr_op = loc_cur.fixed<ubyte>();

        if (expr_op >= (unsigned)DW_OP::reg0 && expr_op < (unsigned)DW_OP::reg31) {
            result.location_type = expr_result::type::address;
            result.value = ctx->reg((unsigned)expr_op - (unsigned)DW_OP::reg0);
        } else if (expr_op == (unsigned)DW_OP::regx) {
            result.location_type = expr_result::type::address;
            result.value = ctx->reg(loc_cur.uleb128());
        } else if (expr_op >= (unsigned)DW_OP::breg0 && expr_op < (unsigned)DW_OP::breg31) {
            result.location_type = expr_result::type::address;
            result.value = ctx->reg((unsigned)expr_op - (unsigned)DW_OP::breg0) + loc_cur.sleb128();
        } else if (expr_op == (unsigned)DW_OP::bregx) {
            result.location_type = expr_result::type::address;
            result.value = ctx->reg(loc_cur.uleb128()) + loc_cur.sleb128();
        } else {
            throw expr_error("Unhandled location description for loclist");
        }

        ranges_to_locations.insert(std::make_pair(std::make_pair(start_addr, end_addr), result));
    }

    auto pc = ctx->pc() - at_low_pc(cu->root());

    expr_result result;
    result.location_type = expr_result::type::empty;

    for (auto &&entry : ranges_to_locations) {
        if (pc >= entry.first.first && pc < entry.first.second) {
            result.location_type = expr_result::type::address;
            result.value = entry.second.value;
            return result;
        }
    }

    return result;
}

loclist::loclist(const unit *cu,
                 section_offset offset)
    : cu(cu), offset(offset)
{
}

} // namespace dwarf