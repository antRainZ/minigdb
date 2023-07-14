#ifndef FAULT_INJECT_BREAKPOINT_HPP
#define FAULT_INJECT_BREAKPOINT_HPP

#include <cstdint>

namespace minidbg
{
class breakpoint
{
  public:
    breakpoint() = default;
    breakpoint(pid_t pid, std::intptr_t addr) : pid(pid), addr(addr)
    {
        this->enabled = false;
        this->staging_data = 0;
        this->hit = 0;
    }
    /**
     * @brief 使该addr地址断点生效
     * 
     */
    inline void enable();
    /**
     * @brief 使该addr地址断点失效
     * 
     */
    inline void disable();
    /**
     * @brief 返回当前断点是否生效
     * 
     * @return true 
     * @return false 
     */
    bool is_enabled() const { return enabled; }
    auto get_address() const -> std::intptr_t { return addr; }
    int hit;
  private:
    pid_t pid;
    std::intptr_t addr;
    bool enabled;
    uint64_t staging_data;
    inline uint64_t get_breakpoint_instrument();
};
void breakpoint::enable()
{
    staging_data = ptrace(PTRACE_PEEKDATA, pid, addr, nullptr);

    uint64_t replaced = get_breakpoint_instrument();
    ptrace(PTRACE_POKEDATA, pid, addr, replaced);
    enabled = true;
}

void breakpoint::disable()
{
    ptrace(PTRACE_POKEDATA, pid, addr, staging_data);
    enabled = false;
}
#if defined(__amd64__) || defined(__x86_64__)

uint64_t breakpoint::get_breakpoint_instrument()
{
    // addr 是将要执行的指令位置，默认是小端序，所以直接需要最低位就行
    uint64_t interrupt_instruction = 0xcc; // int3
    return ((staging_data & ~0xff) | interrupt_instruction);
}

#elif defined(__aarch64__) || defined(__arm__)

uint64_t breakpoint::get_breakpoint_instrument()
{
    uint64_t interrupt_instruction = 0xd4200000; // brk
    return ((staging_data & ~0xffffffff) | interrupt_instruction);
}

#else
#error "unsupport the arch"
#endif
} // namespace faultInject

#endif
