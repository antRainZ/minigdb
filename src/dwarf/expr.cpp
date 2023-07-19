#include "dwarf/internal.hpp"
using namespace std;

namespace dwarf
{

expr_context no_expr_context;

expr::expr(const unit *cu,
           section_offset offset, section_length len)
    : cu(cu), offset(offset), len(len)
{
}

expr_result expr::evaluate(expr_context *ctx) const
{
    return evaluate(ctx, {});
}

expr_result expr::evaluate(expr_context *ctx, taddr argument) const
{
    return evaluate(ctx, {argument});
}

expr_result expr::evaluate(expr_context *ctx, const std::initializer_list<taddr> &arguments) const
{
    // 堆栈中的元素必须采用目标机器表示形式
    small_vector<taddr, 8> stack;

    // 创建初始堆栈， 函数的参数（arguments）是以相反的顺序传递的，即第一个元素是栈顶
    if (arguments.size() > 0) {
        stack.reserve(arguments.size());
        for (const taddr *elt = arguments.end() - 1;
             elt >= arguments.begin(); elt--)
            stack.push_back(*elt);
    }

    // 为这个表达式创建一个子节，以便可以轻松地检测其结束
    auto cusec = cu->data();
    shared_ptr<section> subsec(make_shared<section>(cusec->type,
                                                    cusec->begin + offset, len,
                                                    cusec->ord, cusec->fmt,
                                                    cusec->addr_size));
    cursor cur(subsec);

    // 准备表达式结果。一些位置描述可以直接创建结果，而不是使用堆栈的栈顶元素
    expr_result result;

    // 2.6.1.1.4 空描述
    if (cur.end()) {
        result.location_type = expr_result::type::empty;
        result.value = 0;
        return result;
    }

    // 假设结果目前是一个地址，并且应该在最后从堆栈的栈顶获取
    result.location_type = expr_result::type::address;

    // 开始计算
    while (!cur.end()) {
#define CHECK()             \
    do {                    \
        if (stack.empty())  \
            goto underflow; \
    } while (0)
#define CHECKN(n)             \
    do {                      \
        if (stack.size() < n) \
            goto underflow;   \
    } while (0)
        union {
            uint64_t u;
            int64_t s;
        } tmp1, tmp2, tmp3;
        static_assert(sizeof(tmp1) == sizeof(taddr), "taddr is not 64 bits");

#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wswitch-enum"
        DW_OP op = (DW_OP)cur.fixed<ubyte>();
        switch (op) {
            // 2.5.1.1 字面量编码
        case DW_OP::lit0... DW_OP::lit31:
            stack.push_back((unsigned)op - (unsigned)DW_OP::lit0);
            break;
        case DW_OP::addr:
            stack.push_back(cur.address());
            break;
        case DW_OP::const1u:
            stack.push_back(cur.fixed<uint8_t>());
            break;
        case DW_OP::const2u:
            stack.push_back(cur.fixed<uint16_t>());
            break;
        case DW_OP::const4u:
            stack.push_back(cur.fixed<uint32_t>());
            break;
        case DW_OP::const8u:
            stack.push_back(cur.fixed<uint64_t>());
            break;
        case DW_OP::const1s:
            stack.push_back(cur.fixed<int8_t>());
            break;
        case DW_OP::const2s:
            stack.push_back(cur.fixed<int16_t>());
            break;
        case DW_OP::const4s:
            stack.push_back(cur.fixed<int32_t>());
            break;
        case DW_OP::const8s:
            stack.push_back(cur.fixed<int64_t>());
            break;
        case DW_OP::constu:
            stack.push_back(cur.uleb128());
            break;
        case DW_OP::consts:
            stack.push_back(cur.sleb128());
            break;

            // 2.5.1.2 基于寄存器的地址
        case DW_OP::fbreg: {
            for (const auto &die : cu->root()) {
                bool found = false;
                if (die.contains_section_offset(offset)) {
                    auto frame_base_at = die[DW_AT::frame_base];
                    expr_result frame_base{};

                    if (frame_base_at.get_type() == value::type::loclist) {
                        auto loclist = frame_base_at.as_loclist();
                        frame_base = loclist.evaluate(ctx);
                    } else if (frame_base_at.get_type() == value::type::exprloc) {
                        auto expr = frame_base_at.as_exprloc();
                        frame_base = expr.evaluate(ctx);
                    }

                    switch (frame_base.location_type) {
                    case expr_result::type::reg:
                        tmp1.u = (unsigned)frame_base.value;
                        tmp2.s = cur.sleb128();
                        stack.push_back((int64_t)ctx->reg(tmp1.u) + tmp2.s);
                        found = true;
                        break;
                    case expr_result::type::address:
                        tmp1.u = frame_base.value;
                        tmp2.s = cur.sleb128();
#if defined(__amd64__) || defined(__x86_64__)
            stack.push_back(tmp1.u + tmp2.s);
#elif defined(__aarch64__) || defined(__arm__)
            stack.push_back(tmp1.u - tmp2.s);
#else
#error "unsupport the arch"
#endif
                        
                        found = true;
                        break;
                    case expr_result::type::literal:
                    case expr_result::type::implicit:
                    case expr_result::type::empty:
                        throw expr_error("Unhandled frame base type for DW_OP_fbreg");
                    }
                }
                if (found)
                    break;
            }
            break;
        }
        case DW_OP::breg0... DW_OP::breg31:
            tmp1.u = (unsigned)op - (unsigned)DW_OP::breg0;
            tmp2.s = cur.sleb128();
            stack.push_back((int64_t)ctx->reg(tmp1.u) + tmp2.s);
            break;
        case DW_OP::bregx:
            tmp1.u = cur.uleb128();
            tmp2.s = cur.sleb128();
            stack.push_back((int64_t)ctx->reg(tmp1.u) + tmp2.s);
            break;

            // 2.5.1.3 栈操作
        case DW_OP::dup:
            CHECK();
            stack.push_back(stack.back());
            break;
        case DW_OP::drop:
            CHECK();
            stack.pop_back();
            break;
        case DW_OP::pick:
            tmp1.u = cur.fixed<uint8_t>();
            CHECKN(tmp1.u);
            stack.push_back(stack.revat(tmp1.u));
            break;
        case DW_OP::over:
            CHECKN(2);
            stack.push_back(stack.revat(1));
            break;
        case DW_OP::swap:
            CHECKN(2);
            tmp1.u = stack.back();
            stack.back() = stack.revat(1);
            stack.revat(1) = tmp1.u;
            break;
        case DW_OP::rot:
            CHECKN(3);
            tmp1.u = stack.back();
            stack.back() = stack.revat(1);
            stack.revat(1) = stack.revat(2);
            stack.revat(2) = tmp1.u;
            break;
        case DW_OP::deref:
            tmp1.u = subsec->addr_size;
            goto deref_common;
        case DW_OP::deref_size:
            tmp1.u = cur.fixed<uint8_t>();
            if (tmp1.u > subsec->addr_size)
                throw expr_error("DW_OP_deref_size operand exceeds address size");
        deref_common:
            CHECK();
            stack.back() = ctx->deref_size(stack.back(), tmp1.u);
            break;
        case DW_OP::xderef:
            tmp1.u = subsec->addr_size;
            goto xderef_common;
        case DW_OP::xderef_size:
            tmp1.u = cur.fixed<uint8_t>();
            if (tmp1.u > subsec->addr_size)
                throw expr_error("DW_OP_xderef_size operand exceeds address size");
        xderef_common:
            CHECKN(2);
            tmp2.u = stack.back();
            stack.pop_back();
            stack.back() = ctx->xderef_size(tmp2.u, stack.back(), tmp1.u);
            break;
        case DW_OP::push_object_address:
            // XXX
            throw runtime_error("DW_OP_push_object_address not implemented");
        case DW_OP::form_tls_address:
            CHECK();
            stack.back() = ctx->form_tls_address(stack.back());
            break;
        case DW_OP::call_frame_cfa:

#if defined(__amd64__) || defined(__x86_64__)
            tmp1.u = 6;
            stack.push_back((int64_t)ctx->reg(tmp1.u)+16);

#elif defined(__aarch64__) || defined(__arm__)
            tmp1.u = 29;
            stack.push_back((int64_t)ctx->reg(tmp1.u));
#else
#error "unsupport the arch"
#endif
           
            
            break;
            // 2.5.1.4 算术和逻辑操作
#define UBINOP(binop)                       \
    do {                                    \
        CHECKN(2);                          \
        tmp1.u = stack.back();              \
        stack.pop_back();                   \
        tmp2.u = stack.back();              \
        stack.back() = tmp2.u binop tmp1.u; \
    } while (0)
        case DW_OP::abs:
            CHECK();
            tmp1.u = stack.back();
            if (tmp1.s < 0)
                tmp1.s = -tmp1.s;
            stack.back() = tmp1.u;
            break;
        case DW_OP::and_:
            UBINOP(&);
            break;
        case DW_OP::div:
            CHECKN(2);
            tmp1.u = stack.back();
            stack.pop_back();
            tmp2.u = stack.back();
            tmp3.s = tmp1.s / tmp2.s;
            stack.back() = tmp3.u;
            break;
        case DW_OP::minus:
            UBINOP(-);
            break;
        case DW_OP::mod:
            UBINOP(%);
            break;
        case DW_OP::mul:
            UBINOP(*);
            break;
        case DW_OP::neg:
            CHECK();
            tmp1.u = stack.back();
            tmp1.s = -tmp1.s;
            stack.back() = tmp1.u;
            break;
        case DW_OP::not_:
            CHECK();
            stack.back() = ~stack.back();
            break;
        case DW_OP::or_:
            UBINOP(|);
            break;
        case DW_OP::plus:
            UBINOP(+);
            break;
        case DW_OP::plus_uconst:
            tmp1.u = cur.uleb128();
            CHECK();
            stack.back() += tmp1.u;
            break;
        case DW_OP::shl:
            CHECKN(2);
            tmp1.u = stack.back();
            stack.pop_back();
            tmp2.u = stack.back();
            // 移位处理
            if (tmp1.u < sizeof(tmp2.u) * 8)
                stack.back() = tmp2.u << tmp1.u;
            else
                stack.back() = 0;
            break;
        case DW_OP::shr:
            CHECKN(2);
            tmp1.u = stack.back();
            stack.pop_back();
            tmp2.u = stack.back();
            if (tmp1.u < sizeof(tmp2.u) * 8)
                stack.back() = tmp2.u >> tmp1.u;
            else
                stack.back() = 0;
            break;
        case DW_OP::shra:
            CHECKN(2);
            tmp1.u = stack.back();
            stack.pop_back();
            tmp2.u = stack.back();
            // 在C++中，对一个负数进行移位操作（左移或右移）是具有实现定义的行为
            tmp3.u = (tmp2.s < 0);
            if (tmp3.u)
                tmp2.s = -tmp2.s;
            if (tmp1.u < sizeof(tmp2.u) * 8)
                tmp2.u >>= tmp1.u;
            else
                tmp2.u = 0;
            // DWARF 表明在C++中对一个负数进行过度移位操作，结果应该是0
            if (tmp3.u)
                tmp2.s = -tmp2.s;
            stack.back() = tmp2.u;
            break;
        case DW_OP::xor_:
            UBINOP(^);
            break;
#undef UBINOP

            // 2.5.1.5 控制流操作
#define SRELOP(relop)                              \
    do {                                           \
        CHECKN(2);                                 \
        tmp1.u = stack.back();                     \
        stack.pop_back();                          \
        tmp2.u = stack.back();                     \
        stack.back() = (tmp2.s <= tmp1.s) ? 1 : 0; \
    } while (0)
        case DW_OP::le:
            SRELOP(<=);
            break;
        case DW_OP::ge:
            SRELOP(>=);
            break;
        case DW_OP::eq:
            SRELOP(==);
            break;
        case DW_OP::lt:
            SRELOP(<);
            break;
        case DW_OP::gt:
            SRELOP(>);
            break;
        case DW_OP::ne:
            SRELOP(!=);
            break;
        case DW_OP::skip:
            tmp1.s = cur.fixed<int16_t>();
            goto skip_common;
        case DW_OP::bra:
            tmp1.s = cur.fixed<int16_t>();
            CHECK();
            tmp2.u = stack.back();
            stack.pop_back();
            if (tmp2.u == 0)
                break;
        skip_common:
            cur = cursor(subsec, (int64_t)cur.get_section_offset() + tmp1.s);
            break;
        case DW_OP::call2:
        case DW_OP::call4:
        case DW_OP::call_ref:
            // XXX
            throw runtime_error(to_string(op) + " not implemented");
#undef SRELOP

            // 2.5.1.6 特殊操作
        case DW_OP::nop:
            break;

            // 2.6.1.1.2 指在调试信息中描述程序中寄存器的值和位置的数据结构
        case DW_OP::reg0... DW_OP::reg31:
            result.location_type = expr_result::type::reg;
            result.value = (unsigned)op - (unsigned)DW_OP::reg0;
            break;
        case DW_OP::regx:
            result.location_type = expr_result::type::reg;
            result.value = cur.uleb128();
            break;

            // 2.6.1.1.3 隐式位置描述
        case DW_OP::implicit_value:
            result.location_type = expr_result::type::implicit;
            result.implicit_len = cur.uleb128();
            cur.ensure(result.implicit_len);
            result.implicit = cur.pos;
            break;
        case DW_OP::stack_value:
            CHECK();
            result.location_type = expr_result::type::literal;
            result.value = stack.back();
            break;

            // 2.6.1.2 复合位置描述
        case DW_OP::piece:
        case DW_OP::bit_piece:
            // XXX
            throw runtime_error(to_string(op) + " not implemented");

        case DW_OP::lo_user... DW_OP::hi_user:
            // 通过访问光标提供上下文来进行评估
            throw expr_error("unknown user op " + to_string(op));

        default:
            throw expr_error("bad operation " + to_string(op));
        }
#pragma GCC diagnostic pop
#undef CHECK
#undef CHECKN
    }

    if (result.location_type == expr_result::type::address) {
        // 结果类型仍然是一个地址，因此需要从堆栈顶部获取它
        if (stack.empty())
            throw expr_error("final stack is empty; no result given");
        result.value = stack.back();
    }

    return result;

underflow:
    throw expr_error("stack underflow evaluating DWARF expression");
}

} // namespace dwarf
