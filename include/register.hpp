#ifndef FAULT_INJECT_REGISTER_HPP
#define FAULT_INJECT_REGISTER_HPP

namespace minidbg
{
enum class reg;
struct reg_descriptor;

/**
 * @brief 获取pid 的 reg 寄存器的值
 * 
 * @param pid 
 * @param r 
 * @return uint64_t 
 */
uint64_t get_register_value(pid_t pid, reg r);

/**
 * @brief 设置 pid 的 reg 寄存器的值
 * 
 * @param pid 
 * @param r 
 * @param value 
 */
void set_register_value(pid_t pid, reg r, uint64_t value);

/**
 * @brief 获取 寄存器 r 的字符串描述
 * 
 * @param r 
 * @return std::string 
 */
std::string get_register_name(reg r);

/**
 * @brief 从寄存器名称获取寄存器
 * 
 * @param name 
 * @return reg 
 */
reg get_register_from_name(const std::string &name);

/**
 * @brief 断点回退使需要修改的pc值偏移量
 *
 * @return int
 */
int get_breakpoint_rollback();

}

#if defined(__amd64__) || defined(__x86_64__)
// 用于描述不同架构下的pc 描述
#include "x86_64/register.hpp"
#define PROGRAM_COUNT reg::rip
#define FRAME_POINTER reg::rbp

#elif defined(__aarch64__) || defined(__arm__)

#include "aarch64/register.hpp"
#define PROGRAM_COUNT reg::pc
#define FRAME_POINTER reg::fp

#else
#error "unsupport the arch"
#endif

#endif