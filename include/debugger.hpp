#ifndef MINIDBG_DEBUGGER_HPP
#define MINIDBG_DEBUGGER_HPP

#include <utility>
#include <string>
#include <linux/types.h>
#include <unordered_map>

#include "breakpoint.hpp"
#include "dwarf/dwarf.hpp"
#include "elf/elf.hpp"

namespace minidbg {
    enum class symbol_type {
        notype,            // No type (e.g., absolute symbol)
        object,            // Data object
        func,              // Function entry point
        section,           // Symbol is associated with a section
        file,              // Source file associated with the
    };                     // object file

    std::string to_string (symbol_type st) {
        switch (st) {
        case symbol_type::notype: return "notype";
        case symbol_type::object: return "object";
        case symbol_type::func: return "func";
        case symbol_type::section: return "section";
        case symbol_type::file: return "file";
        }
        return "none";
    }

    struct symbol {
        symbol_type type;
        std::string name;
        std::uintptr_t addr;
    };

    struct Variable {
        std::string name; // 变量名
        uint64_t addr;    // 地址
        Variable(std::string name, uint64_t addr) : name(name), addr(addr) {}
    };

    class debugger {
    public:
        debugger (std::string prog_name, pid_t pid)
             : m_prog_name{std::move(prog_name)}, m_pid{pid} {
            auto fd = open(m_prog_name.c_str(), O_RDONLY);

            m_elf = elf::elf{elf::create_mmap_loader(fd)};
            m_dwarf = dwarf::dwarf{dwarf::elf::create_loader(m_elf)};
        }
        /**
         * @brief 当该类创建后，调用该函数，开始进行debug
         *
         */
        void run();
        /**
         * @brief 在 addr 地址设置断点
         * 
         * @param addr 
         */
        void set_breakpoint_at_address(std::intptr_t addr);
        /**
         * @brief 在该函数名设置断点
         * 
         * @param name 
         */
        void set_breakpoint_at_function(const std::string& name);
        /**
         * @brief 在某个文件行设置断点
         *
         * @param file
         * @param line
         */
        void set_breakpoint_at_source_line(const std::string& file, unsigned line);
        void dump_registers();
        void print_backtrace();
        void read_variables();
        void print_source(const std::string& file_name, unsigned line, unsigned n_lines_context=2);
        auto lookup_symbol(const std::string& name) -> std::vector<symbol>;

        void single_step_instruction();
        void single_step_instruction_with_breakpoint_check();
        void step_in();
        void step_over();
        void step_out();
        void remove_breakpoint(std::intptr_t addr);

    private:
        void handle_command(const std::string& line);
        void continue_execution();
        auto get_pc() -> uint64_t;
        auto get_offset_pc() -> uint64_t;
        void set_pc(uint64_t pc);
        void step_over_breakpoint();
        void wait_for_signal();
        auto get_signal_info() -> siginfo_t;

        void handle_sigtrap(siginfo_t info);

        void initialise_load_address();
        uint64_t offset_load_address(uint64_t addr);
        uint64_t offset_dwarf_address(uint64_t addr);

        auto get_function_from_pc(uint64_t pc) -> dwarf::die;
        auto get_line_entry_from_pc(uint64_t pc) -> dwarf::line_table::iterator;

        auto read_memory(uint64_t address) -> uint64_t ;
        void write_memory(uint64_t address, uint64_t value);

        std::string m_prog_name;
        pid_t m_pid;
        uint64_t m_load_address = 0;
        std::unordered_map<std::intptr_t,breakpoint> m_breakpoints;
        dwarf::dwarf m_dwarf;
        elf::elf m_elf;
    };
}

#endif
