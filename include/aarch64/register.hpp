#ifndef FAULT_INJECT_ARM64_REGISTER_HPP
#define FAULT_INJECT_ARM64_REGISTER_HPP

#include <algorithm>
#include <array>
#include <elf.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <string>

namespace minidbg
{

enum class reg {
    x0,
    x1,
    x2,
    x3,
    x4,
    x5,
    x6,
    x7,
    x8,
    x9,
    x10,
    x11,
    x12,
    x13,
    x14,
    x15,
    x16,
    x17,
    x18,
    x19,
    x20,
    x21,
    x22,
    x23,
    x24,
    x25,
    x26,
    x27,
    x28,
    fp,
    lr,
    sp,
    pc,
    cpsr
};

static constexpr std::size_t n_registers = 35;

struct reg_descriptor {
    reg r;
    int dwarf_r;
    std::string name;
};


// have a look in /usr/include/sys/user.h for how to lay this out
static const std::array<reg_descriptor, n_registers> g_register_descriptors{{
    {reg::x0, 0, "x0"},
    {reg::x1, 1, "x1"},
    {reg::x2, 2, "x2"},
    {reg::x3, 3, "x3"},
    {reg::x4, 4, "x4"},
    {reg::x5, 5, "x5"},
    {reg::x6, 6, "x6"},
    {reg::x7, 7, "x7"},
    {reg::x8, 8, "x8"},
    {reg::x9, 9, "x9"},
    {reg::x10, 10, "x10"},
    {reg::x11, 11, "x11"},
    {reg::x12, 12, "x12"},
    {reg::x13, 13, "x13"},
    {reg::x14, 14, "x14"},
    {reg::x15, 15, "x15"},
    {reg::x16, 16, "x16"},
    {reg::x17, 17, "x17"},
    {reg::x18, 18, "x18"},
    {reg::x19, 19, "x19"},
    {reg::x20, 20, "x20"},
    {reg::x21, 21, "x21"},
    {reg::x22, 22, "x22"},
    {reg::x23, 23, "x23"},
    {reg::x24, 24, "x24"},
    {reg::x25, 25, "x25"},
    {reg::x26, 26, "x26"},
    {reg::x27, 27, "x27"},
    {reg::x28, 28, "x28"},
    {reg::fp, 29, "fp(x29)"},
    {reg::lr, 30, "lr(x30)"},
    {reg::sp, 31, "sp"},
    {reg::pc, 32, "pc"},
    {reg::cpsr, 33, "cpsr"},
}};

uint64_t get_register_value(pid_t pid, reg r)
{
    user_regs_struct regs;
    struct iovec iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov);
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [r](auto &&rd) { return rd.r == r; });

    return *(reinterpret_cast<uint64_t *>(&regs) + (it - begin(g_register_descriptors)));
}

void set_register_value(pid_t pid, reg r, uint64_t value)
{
    user_regs_struct regs;
    struct iovec iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov);
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [r](auto &&rd) { return rd.r == r; });

    *(reinterpret_cast<uint64_t *>(&regs) + (it - begin(g_register_descriptors))) = value;
    ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
}

uint64_t get_register_value_from_dwarf_register(pid_t pid, unsigned regnum)
{
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [regnum](auto &&rd) { return static_cast<unsigned int>(rd.dwarf_r) == regnum; });
    if (it == end(g_register_descriptors)) {
        throw std::out_of_range{"Unknown dwarf register"};
    }

    return get_register_value(pid, it->r);
}

std::string get_register_name(reg r)
{
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [r](auto &&rd) { return rd.r == r; });
    return it->name;
}

reg get_register_from_name(const std::string &name)
{
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [name](auto &&rd) { return rd.name == name; });
    return it->r;
}

int get_breakpoint_rollback()
{
    return 0;
}
} // namespace faultInject

#endif
