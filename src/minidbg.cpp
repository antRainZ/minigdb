#include <vector>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/personality.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "linenoise.h"

#include "debugger.hpp"
#include "register.hpp"

using namespace minidbg;

class ptrace_expr_context : public dwarf::expr_context {
public:
    ptrace_expr_context (pid_t pid, uint64_t load_address) : 
       m_pid{pid}, m_load_address(load_address) {}

    dwarf::taddr reg (unsigned regnum) override {
        return get_register_value_from_dwarf_register(m_pid, regnum);
    }

    dwarf::taddr pc() override {
        return get_register_value(m_pid, PROGRAM_COUNT) - m_load_address;
    }

    dwarf::taddr deref_size (dwarf::taddr address, unsigned size) override {
        //TODO take into account size
        return ptrace(PTRACE_PEEKDATA, m_pid, address + m_load_address, nullptr);
    }

private:
    pid_t m_pid;
    uint64_t m_load_address;
};

bool find_pc(const dwarf::die &d, dwarf::taddr pc, std::vector<dwarf::die> *stack)
{
    using namespace dwarf;

    // Scan children first to find most specific DIE
    bool found = false;
    for (auto &child : d) {
        if ((found = find_pc(child, pc, stack)))
            break;
    }
    switch (d.tag) {
    case DW_TAG::subprogram:
    case DW_TAG::inlined_subroutine:
        try {
            if (found || die_pc_range(d).contains(pc)) {
                found = true;
                stack->push_back(d);
            }
        } catch (std::exception &e) {

        } 
        break;
    default:
        break;
    }
    return found;
}

/**
 * @brief 获取对应的比特位
 * 
 * @param val 
 * @param start 
 * @param end 
 * @return unsigned int 
 */
unsigned int select_bits (unsigned int val, unsigned int start, unsigned int end)
{
    unsigned int size = (end - start) + 1;
    unsigned int mask = ((1 << size) - 1) << start;
    return (val & mask) >> start;
}

/**
 * @brief 
 * 
 * @param bits 
 * @param numbits 
 * @return unsigned int 
 */
unsigned int arm64_sign_extend (unsigned int bits, int numbits)
{
    if(bits & (1 << (numbits - 1)))
        return bits | ~((1 << numbits) - 1);
    return bits;
}

int get_offset(uint32_t opcode) {
    unsigned imm7 = select_bits (opcode, 15, 21);
    unsigned opc = select_bits (opcode, 30, 31);
    unsigned int scale = 2 + (opc >> 1);
    unsigned int imm = arm64_sign_extend (imm7, 7) << scale;
    return (int)imm;
}

template class std::initializer_list<dwarf::taddr>;
void debugger::read_variables() {
    using namespace dwarf;

    auto func = get_function_from_pc(get_offset_pc());
    std::vector<Variable> farg; // 函数参数
    std::vector<Variable> larg; // 函数局部变量


    for (const auto& die : func) {
        if (die.tag == DW_TAG::variable || die.tag == DW_TAG::formal_parameter) {
            auto loc_val = die[DW_AT::location];

            //only supports exprlocs for now
            if (loc_val.get_type() == value::type::exprloc) {
                ptrace_expr_context context {m_pid, m_load_address};
                auto result = loc_val.as_exprloc().evaluate(&context);

                switch (result.location_type) {
                case expr_result::type::address:
                {
                    if (die.tag == DW_TAG::variable) 
                        larg.emplace_back(at_name(die), result.value);
                    else 
                        farg.emplace_back(at_name(die), result.value);
                    break;
                }

                case expr_result::type::reg:
                {
                    auto value = get_register_value_from_dwarf_register(m_pid, result.value);
                    std::cout << at_name(die) << " (reg " << result.value << ") = " << value << std::endl;
                    break;
                }

                default:
                    throw std::runtime_error{"Unhandled variable location"};
                }
            }
            else {
                throw std::runtime_error{"Unhandled variable location"};
            }
        }
    }
#if defined(__aarch64__) || defined(__arm64__)

    uint64_t res = read_memory(get_register_value(m_pid, FRAME_POINTER)-farg.size()*4-8);
    uint32_t code = res&(0xffffffff);
    int offset = get_offset(code);
    for(auto &v:farg) {
        v.addr = v.addr-offset;
    }
    for(auto &v:larg) {
        v.addr = v.addr-offset;
    }

#endif
    std::cout << "function argument:" << std::endl;
    for(auto &v:farg) {
        auto value = read_memory(v.addr);
        std::cout << v.name << " (0x" << std::hex << v.addr << ") = " << value << std::endl;
    }
    std::cout << "function localtion variable:" << std::endl;
    for(auto &v:larg) {
        auto value = read_memory(v.addr);
        std::cout << v.name << " (0x" << std::hex << v.addr << ") = " << value << std::endl;
    }
}


void debugger::print_backtrace() {
    auto output_frame = [frame_number = 0] (auto&& func) mutable {
        std::cout << "frame #" << frame_number++ << ": 0x" << dwarf::at_low_pc(func)
                  << ' ' << dwarf::at_name(func) << std::endl;
    };

    auto current_func = get_function_from_pc(offset_load_address(get_pc()));
    output_frame(current_func);

    auto frame_pointer = get_register_value(m_pid, FRAME_POINTER);
    auto return_address = read_memory(frame_pointer+8);

    while (dwarf::at_name(current_func) != "main") {
        current_func = get_function_from_pc(offset_load_address(return_address));
        output_frame(current_func);
        frame_pointer = read_memory(frame_pointer);
        return_address = read_memory(frame_pointer+8);
    }
}

symbol_type to_symbol_type(elf::stt sym) {
    switch (sym) {
    case elf::stt::notype: return symbol_type::notype;
    case elf::stt::object: return symbol_type::object;
    case elf::stt::func: return symbol_type::func;
    case elf::stt::section: return symbol_type::section;
    case elf::stt::file: return symbol_type::file;
    default: return symbol_type::notype;
    }
};

std::vector<symbol> debugger::lookup_symbol(const std::string& name) {
   std::vector<symbol> syms;

   for (auto& sec : m_elf.sections()) {
      if (sec.get_hdr().type != elf::sht::symtab && sec.get_hdr().type != elf::sht::dynsym)
         continue;

      for (auto sym : sec.as_symtab()) {
         if (sym.get_name() == name) {
            auto& d = sym.get_data();
            syms.push_back(symbol{ to_symbol_type(d.type()), sym.get_name(), d.value });
         }
      }
   }

   return syms;
}

void debugger::initialise_load_address() {
   //If this is a dynamic library (e.g. PIE)
   if (m_elf.get_hdr().type == elf::et::dyn) {
      //The load address is found in /proc/<pid>/maps
      std::ifstream map("/proc/" + std::to_string(m_pid) + "/maps");

      //Read the first address from the file
      std::string addr;
      std::getline(map, addr, '-');

      m_load_address = std::stoll(addr, 0, 16);
   }
}

uint64_t debugger::offset_load_address(uint64_t addr) {
   return addr - m_load_address;
}

uint64_t debugger::offset_dwarf_address(uint64_t addr) {
   return addr + m_load_address;
}

void debugger::remove_breakpoint(std::intptr_t addr) {
    if (m_breakpoints.at(addr).is_enabled()) {
        m_breakpoints.at(addr).disable();
    }
    m_breakpoints.erase(addr);
}

void debugger::step_out() {
    auto frame_pointer = get_register_value(m_pid, FRAME_POINTER);
    auto return_address = read_memory(frame_pointer+8);

    bool should_remove_breakpoint = false;
    if (!m_breakpoints.count(return_address)) {
        set_breakpoint_at_address(return_address);
        should_remove_breakpoint = true;
    }

    continue_execution();

    if (should_remove_breakpoint) {
        remove_breakpoint(return_address);
    }
}

void debugger::step_in() {
   auto line = get_line_entry_from_pc(get_offset_pc())->line;

   while (get_line_entry_from_pc(get_offset_pc())->line == line) {
      single_step_instruction_with_breakpoint_check();
   }

   auto line_entry = get_line_entry_from_pc(get_offset_pc());
   print_source(line_entry->file->path, line_entry->line);
}

void debugger::step_over() {
    auto func = get_function_from_pc(get_offset_pc());
    auto func_entry = at_low_pc(func);
    auto func_end = at_high_pc(func);

    auto line = get_line_entry_from_pc(func_entry);
    auto start_line = get_line_entry_from_pc(get_offset_pc());

    std::vector<std::intptr_t> to_delete{};

    while (line->address < func_end) {
        auto load_address = offset_dwarf_address(line->address);
        if (line->address != start_line->address && !m_breakpoints.count(load_address)) {
            set_breakpoint_at_address(load_address);
            to_delete.push_back(load_address);
        }
        ++line;
    }

    auto frame_pointer = get_register_value(m_pid, FRAME_POINTER);
    auto return_address = read_memory(frame_pointer+8);
    if (!m_breakpoints.count(return_address)) {
        set_breakpoint_at_address(return_address);
        to_delete.push_back(return_address);
    }

    continue_execution();

    for (auto addr : to_delete) {
        remove_breakpoint(addr);
    }
}

void debugger::single_step_instruction() {
    ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
    wait_for_signal();
}

void debugger::single_step_instruction_with_breakpoint_check() {
    //first, check to see if we need to disable and enable a breakpoint
    if (m_breakpoints.count(get_pc())) {
        step_over_breakpoint();
    }
    else {
        single_step_instruction();
    }
}

uint64_t debugger::read_memory(uint64_t address) {
    return ptrace(PTRACE_PEEKDATA, m_pid, address, nullptr);
}

void debugger::write_memory(uint64_t address, uint64_t value) {
    ptrace(PTRACE_POKEDATA, m_pid, address, value);
}

uint64_t debugger::get_pc() {
    return get_register_value(m_pid, PROGRAM_COUNT);
}

uint64_t debugger::get_offset_pc() {
   return offset_load_address(get_pc());
}

void debugger::set_pc(uint64_t pc) {
    set_register_value(m_pid, PROGRAM_COUNT, pc);
}

dwarf::die debugger::get_function_from_pc(uint64_t pc) {
    for (auto &cu : m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            std::vector<dwarf::die> stack;
            if (find_pc(cu.root(), pc, &stack)) {
                for (auto &d : stack) {
                    return d;
                }
            }
        }
    }

    throw std::out_of_range{"Cannot find function"};
}

dwarf::line_table::iterator debugger::get_line_entry_from_pc(uint64_t pc) {
    for (auto &cu : m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            auto &lt = cu.get_line_table();
            auto it = lt.find_address(pc);
            if (it == lt.end()) {
                throw std::out_of_range{"Cannot find line entry"};
            }
            else {
                return it;
            }
        }
    }

    throw std::out_of_range{"Cannot find line entry"};
}

void debugger::print_source(const std::string& file_name, unsigned line, unsigned n_lines_context) {
    std::ifstream file {file_name};

    //Work out a window around the desired line
    auto start_line = line <= n_lines_context ? 1 : line - n_lines_context;
    auto end_line = line + n_lines_context + (line < n_lines_context ? n_lines_context - line : 0) + 1;

    char c{};
    auto current_line = 1u;
    //Skip lines up until start_line
    while (current_line != start_line && file.get(c)) {
        if (c == '\n') {
            ++current_line;
        }
    }

    //Output cursor if we're at the current line
    std::cout << (current_line==line ? "> " : "  ");

    //Write lines up until end_line
    while (current_line <= end_line && file.get(c)) {
        std::cout << c;
        if (c == '\n') {
            ++current_line;
            //Output cursor if we're at the current line
            std::cout << (current_line==line ? "> " : "  ");
        }
    }

    //Write newline and make sure that the stream is flushed properly
    std::cout << std::endl;
}

siginfo_t debugger::get_signal_info() {
    siginfo_t info;
    ptrace(PTRACE_GETSIGINFO, m_pid, nullptr, &info);
    return info;
}

void debugger::step_over_breakpoint() {
    if (m_breakpoints.count(get_pc())) {
        auto& bp = m_breakpoints[get_pc()];
        if (bp.is_enabled()) {
            bp.disable();
            ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
            wait_for_signal();
            bp.enable();
        }
    }
}

void debugger::wait_for_signal() {
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);

    auto siginfo = get_signal_info();

    switch (siginfo.si_signo) {
    case SIGTRAP:
        handle_sigtrap(siginfo);
        break;
    case SIGSEGV:
        std::cout << "Yay, segfault. Reason: " << siginfo.si_code << std::endl;
        break;
    default:
        std::cout << "Got signal " << strsignal(siginfo.si_signo) << std::endl;
    }
}

void debugger::handle_sigtrap(siginfo_t info) {
    switch (info.si_code) {
        //one of these will be set if a breakpoint was hit
    case SI_KERNEL:
    case TRAP_BRKPT:
    {
        int offset = get_breakpoint_rollback();
        set_pc(get_pc() - offset);
        std::cout << "Hit breakpoint at address 0x" << std::hex << get_pc() << std::endl;
        auto offset_pc = offset_load_address(get_pc()); //rember to offset the pc for querying DWARF
        auto line_entry = get_line_entry_from_pc(offset_pc);
        print_source(line_entry->file->path, line_entry->line);
        return;
    }
    //this will be set if the signal was sent by single stepping
    case TRAP_TRACE:
        return;
    default:
        std::cout << "Unknown SIGTRAP code " << info.si_code << std::endl;
        return;
    }
}

void debugger::continue_execution() {
    step_over_breakpoint();
    ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);
    wait_for_signal();
}

void debugger::dump_registers() {
    for (const auto& rd : g_register_descriptors) {
        std::cout << rd.name << " 0x"
                  << std::setfill('0') << std::setw(16) << std::hex << get_register_value(m_pid, rd.r) << std::endl;
    }
}

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> out{};
    std::stringstream ss {s};
    std::string item;

    while (std::getline(ss,item,delimiter)) {
        out.push_back(item);
    }

    return out;
}

bool is_prefix(const std::string& s, const std::string& of) {
    if (s.size() > of.size()) return false;
    return std::equal(s.begin(), s.end(), of.begin());
}

void debugger::handle_command(const std::string& line) {
    auto args = split(line,' ');
    auto command = args[0];

    if (is_prefix(command, "cont")) {
        continue_execution();
    }
    else if(is_prefix(command, "break")) {
        if (args[1][0] == '0' && args[1][1] == 'x') {
            std::string addr {args[1], 2};
            set_breakpoint_at_address(std::stoll(addr, 0, 16));
        }
        else if (args[1].find(':') != std::string::npos) {
            auto file_and_line = split(args[1], ':');
            set_breakpoint_at_source_line(file_and_line[0], std::stoll(file_and_line[1]));
        }
        else {
            set_breakpoint_at_function(args[1]);
        }
    }
    else if(is_prefix(command, "step")) {
        step_in();
    }
    else if(is_prefix(command, "next")) {
        step_over();
    }
    else if(is_prefix(command, "finish")) {
        step_out();
    }

    else if(is_prefix(command, "finish")) {
        step_out();
    }

    else if (is_prefix(command, "register")) {
        if (is_prefix(args[1], "dump")) {
            dump_registers();
        }
        else if (is_prefix(args[1], "read")) {
            std::cout << get_register_value(m_pid, get_register_from_name(args[2])) << std::endl;
        }
        else if (is_prefix(args[1], "write")) {
            std::string val {args[3], 2}; //assume 0xVAL
            set_register_value(m_pid, get_register_from_name(args[2]), std::stoll(val, 0, 16));
        }
    }

    else if(is_prefix(command, "memory")) {
        std::string addr {args[2], 2}; //assume 0xADDRESS

        if (is_prefix(args[1], "read")) {
            std::cout << std::hex << read_memory(std::stoll(addr, 0, 16)) << std::endl;
        }
        if (is_prefix(args[1], "write")) {
            std::string val {args[3], 2}; //assume 0xVAL
            write_memory(std::stol(addr, 0, 16), std::stoll(val, 0, 16));
        }
    }

    else if(is_prefix(command, "variables")) {
        read_variables();
    }

    else if(is_prefix(command, "backtrace")) {
        print_backtrace();
    }
    else if(is_prefix(command, "symbol")) {
        auto syms = lookup_symbol(args[1]);
        for (auto&& s : syms) {
            std::cout << s.name << ' ' << to_string(s.type) << " 0x" << std::hex << s.addr << std::endl;
        }
    }
    else if(is_prefix(command, "stepi")) {
        single_step_instruction_with_breakpoint_check();
        auto line_entry = get_line_entry_from_pc(get_pc());
        print_source(line_entry->file->path, line_entry->line);
    }
    else {
        std::cerr << "Unknown command\n";
    }
}

bool is_suffix(const std::string& s, const std::string& of) {
    if (s.size() > of.size()) return false;
    auto diff = of.size() - s.size();
    return std::equal(s.begin(), s.end(), of.begin() + diff);
}

void debugger::set_breakpoint_at_function(const std::string& name) {
    for (const auto& cu : m_dwarf.compilation_units()) {
        for (const auto& die : cu.root()) {
            if (die.has(dwarf::DW_AT::name) && at_name(die) == name) {
                auto low_pc = at_low_pc(die);
                auto entry = get_line_entry_from_pc(low_pc);
                ++entry; //skip prologue
                set_breakpoint_at_address(offset_dwarf_address(entry->address));
            }
        }
    }
}

void debugger::set_breakpoint_at_source_line(const std::string& file, unsigned line) {
    for (const auto& cu : m_dwarf.compilation_units()) {
        if (is_suffix(file, at_name(cu.root()))) {
            const auto& lt = cu.get_line_table();

            for (const auto& entry : lt) {
                if (entry.is_stmt && entry.line == line) {
                    set_breakpoint_at_address(offset_dwarf_address(entry.address));
                    return;
                }
            }
        }
    }
}

void debugger::set_breakpoint_at_address(std::intptr_t addr) {
    std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
    breakpoint bp {m_pid, addr};
    bp.enable();
    m_breakpoints[addr] = bp;
}

void debugger::run() {
    wait_for_signal();
    initialise_load_address();

    char* line = nullptr;
    while((line = linenoise("minidbg> ")) != nullptr) {
        handle_command(line);
        linenoiseHistoryAdd(line);
        linenoiseFree(line);
    }
}

void execute_debugee (const std::string& prog_name) {
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
        std::cerr << "Error in ptrace\n";
        return;
    }
    execl(prog_name.c_str(), prog_name.c_str(), nullptr);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Program name not specified";
        return -1;
    }

    auto prog = argv[1];

    auto pid = fork();
    if (pid == 0) {
        //child
        personality(ADDR_NO_RANDOMIZE);
        execute_debugee(prog);
    }
    else if (pid >= 1)  {
        //parent
        std::cout << "Started debugging process " << pid << '\n';
        debugger dbg{prog, pid};
        dbg.run();
    }
}
