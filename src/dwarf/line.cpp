#include "dwarf/internal.hpp"
#include <cassert>
using namespace std;

namespace dwarf
{

/**
 * @brief 用于标准操作码的预期参数数量。它用于检查 opcode_lengths 头部字段的兼容性
 * 
 */
static const int opcode_lengths[] = {
    0,
    // DW_LNS::copy
    0, 1, 1, 1, 1,
    // DW_LNS::negate_stmt
    0, 0, 0, 1, 0,
    // DW_LNS::set_epilogue_begin
    0, 1};

struct line_table::impl {
    shared_ptr<section> sec;

    // Header information
    section_offset program_offset;
    ubyte minimum_instruction_length;
    ubyte maximum_operations_per_instruction;
    bool default_is_stmt;
    sbyte line_base;
    ubyte line_range;
    ubyte opcode_base;
    vector<ubyte> standard_opcode_lengths;
    vector<string> include_directories;
    vector<file> file_names;

    // 表示上一次读取文件名条目后，在节中的偏移量。
    // 这个变量用于追踪已读取文件名的位置，以避免重复添加相同的文件名条目
    section_offset last_file_name_end;
    // 表示文件名是否已经完整地读取
    bool file_names_complete;

    impl() : last_file_name_end(0), file_names_complete(false){};

    bool read_file_entry(cursor *cur, bool in_header);
};

line_table::line_table(const shared_ptr<section> &sec, section_offset offset,
                       unsigned cu_addr_size, const string &cu_comp_dir,
                       const string &cu_name)
    : m(make_shared<impl>())
{
    // DW_AT_comp_dir是一种DWARF调试信息中的属性，它表示编译单元的目录路径
    string comp_dir, abs_path;
    if (cu_comp_dir.empty() || cu_comp_dir.back() == '/')
        comp_dir = cu_comp_dir;
    else
        comp_dir = cu_comp_dir + '/';

    // 读取行号表的头部信息
    cursor cur(sec, offset);
    m->sec = cur.subsection();
    cur = cursor(m->sec);
    cur.skip_initial_length();
    m->sec->addr_size = cu_addr_size;

    // 基本头部信息
    uhalf version = cur.fixed<uhalf>();
    if (version < 2 || version > 4)
        throw format_error("unknown line number table version " +
                           std::to_string(version));
    section_length header_length = cur.offset();
    m->program_offset = cur.get_section_offset() + header_length;
    m->minimum_instruction_length = cur.fixed<ubyte>();
    m->maximum_operations_per_instruction = 1;
    if (version >= 4)
        m->maximum_operations_per_instruction = cur.fixed<ubyte>();
    if (m->maximum_operations_per_instruction == 0)
        throw format_error("maximum_operations_per_instruction cannot"
                           " be 0 in line number table");
    m->default_is_stmt = cur.fixed<ubyte>();
    m->line_base = cur.fixed<sbyte>();
    m->line_range = cur.fixed<ubyte>();
    if (m->line_range == 0)
        throw format_error("line_range cannot be 0 in line number table");
    m->opcode_base = cur.fixed<ubyte>();

    static_assert(sizeof(opcode_lengths) / sizeof(opcode_lengths[0]) == 13,
                  "opcode_lengths table has wrong length");


    m->standard_opcode_lengths.resize(m->opcode_base);
    m->standard_opcode_lengths[0] = 0;
    for (unsigned i = 1; i < m->opcode_base; i++) {
        ubyte length = cur.fixed<ubyte>();
        if (length != opcode_lengths[i])
            // 规范没有明确说明如果标准操作码的操作码长度与头部不匹配时应该怎么做
            throw format_error(
                "expected " +
                std::to_string(opcode_lengths[i]) +
                " arguments for line number opcode " +
                std::to_string(i) + ", got " +
                std::to_string(length));
        m->standard_opcode_lengths[i] = length;
    }

    // 存储包含目录列表
    // 目录0隐含地表示编译单元的当前目录
    string incdir;
    m->include_directories.push_back(comp_dir);
    while (true) {
        cur.string(incdir);
        if (incdir.empty())
            break;
        if (incdir.back() != '/')
            incdir += '/';
        if (incdir[0] == '/')
            m->include_directories.push_back(move(incdir));
        else
            m->include_directories.push_back(comp_dir + incdir);
    }

    // 文件名列表
    // 当索引为0时，file_name变量表示编译单元的文件名
    string file_name;
    if (!cu_name.empty() && cu_name[0] == '/')
        m->file_names.emplace_back(cu_name);
    else
        m->file_names.emplace_back(comp_dir + cu_name);
    while (m->read_file_entry(&cur, true))
        ;
}

line_table::iterator line_table::begin() const
{
    if (!valid())
        return iterator(nullptr, 0);
    return iterator(this, m->program_offset);
}

line_table::iterator line_table::end() const
{
    if (!valid())
        return iterator(nullptr, 0);
    return iterator(this, m->sec->size());
}

line_table::iterator line_table::find_address(taddr addr) const
{
    iterator prev = begin(), e = end();
    if (prev == e)
        return prev;

    iterator it = prev;
    for (++it; it != e; prev = it++) {
        if (prev->address <= addr && it->address > addr &&
            !prev->end_sequence)
            return prev;
    }
    prev = e;
    return prev;
}

const line_table::file * line_table::get_file(unsigned index) const
{
    if (index >= m->file_names.size()) {
        // 如果索引超出了范围，代码将尝试在行号表的程序中查找是否存在该文件的声明。
        // 然而，这样的情况可能很罕见
        if (!m->file_names_complete) {
            for (auto &ent : *this)
                (void)ent;
        }
        if (index >= m->file_names.size())
            throw out_of_range("file name index " + std::to_string(index) +
                               " exceeds file table size of " +
                               std::to_string(m->file_names.size()));
    }
    return &m->file_names[index];
}

bool line_table::impl::read_file_entry(cursor *cur, bool in_header)
{
    assert(cur->sec == sec);

    string file_name;
    cur->string(file_name);
    if (in_header && file_name.empty())
        return false;
    uint64_t dir_index = cur->uleb128();
    uint64_t mtime = cur->uleb128();
    uint64_t length = cur->uleb128();

    // 已经处理过
    if (cur->get_section_offset() <= last_file_name_end)
        return true;
    last_file_name_end = cur->get_section_offset();

    if (file_name[0] == '/')
        file_names.emplace_back(move(file_name), mtime, length);
    else if (dir_index < include_directories.size())
        file_names.emplace_back(
            include_directories[dir_index] + file_name,
            mtime, length);
    else
        throw format_error("file name directory index out of range: " +
                           std::to_string(dir_index));

    return true;
}

line_table::file::file(string path, uint64_t mtime, uint64_t length)
    : path(path), mtime(mtime), length(length)
{
}

void line_table::entry::reset(bool is_stmt)
{
    address = op_index = 0;
    file = nullptr;
    file_index = line = 1;
    column = 0;
    this->is_stmt = is_stmt;
    basic_block = end_sequence = prologue_end = epilogue_begin = false;
    isa = discriminator = 0;
}

string line_table::entry::get_description() const
{
    string res = file->path;
    if (line) {
        res.append(":").append(std::to_string(line));
        if (column)
            res.append(":").append(std::to_string(column));
    }
    return res;
}

line_table::iterator::iterator(const line_table *table, section_offset pos)
    : table(table), pos(pos)
{
    if (table) {
        regs.reset(table->m->default_is_stmt);
        ++(*this);
    }
}

line_table::iterator & line_table::iterator::operator++()
{
    cursor cur(table->m->sec, pos);

    // 执行指令直到达到流的结尾或者某个指令生成了一个行号表
    bool stepped = false, output = false;
    while (!cur.end() && !output) {
        output = step(&cur);
        stepped = true;
    }
    // 如果循环执行了至少一次且没有产生输出
    if (stepped && !output)
        throw format_error("unexpected end of line table");
    if (stepped && cur.end()) {
        table->m->file_names_complete = true;
    }
    if (output) {
        // 解析相应的文件名
        if (entry.file_index < table->m->file_names.size())
            entry.file = &table->m->file_names[entry.file_index];
        else
            throw format_error("bad file index " +
                               std::to_string(entry.file_index) +
                               " in line table");
    }

    pos = cur.get_section_offset();
    return *this;
}

bool line_table::iterator::step(cursor *cur)
{
    struct line_table::impl *m = table->m.get();

    // 读取操作码
    ubyte opcode = cur->fixed<ubyte>();
    if (opcode >= m->opcode_base) {
        // 用于处理特殊操作码
        ubyte adjusted_opcode = opcode - m->opcode_base;
        unsigned op_advance = adjusted_opcode / m->line_range;
        signed line_inc = m->line_base + (signed)adjusted_opcode % m->line_range;

        regs.line += line_inc;
        regs.address += m->minimum_instruction_length *
                        ((regs.op_index + op_advance) / m->maximum_operations_per_instruction);
        regs.op_index = (regs.op_index + op_advance) % m->maximum_operations_per_instruction;
        entry = regs;

        regs.basic_block = regs.prologue_end =
            regs.epilogue_begin = false;
        regs.discriminator = 0;

        return true;
    } else if (opcode != 0) {
        // 标准操作码
        // 特定厂商的操作码 ，并将其视为标准操作码进行处理
        uint64_t uarg;
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wswitch-enum"
        switch ((DW_LNS)opcode) {
        case DW_LNS::copy:
            entry = regs;
            regs.basic_block = regs.prologue_end =
                regs.epilogue_begin = false;
            regs.discriminator = 0;
            break;
        case DW_LNS::advance_pc:
            // 操作码步进的处理
            uarg = cur->uleb128();
        advance_pc:
            regs.address += m->minimum_instruction_length *
                            ((regs.op_index + uarg) / m->maximum_operations_per_instruction);
            regs.op_index = (regs.op_index + uarg) % m->maximum_operations_per_instruction;
            break;
        case DW_LNS::advance_line:
            regs.line = (signed)regs.line + cur->sleb128();
            break;
        case DW_LNS::set_file:
            regs.file_index = cur->uleb128();
            break;
        case DW_LNS::set_column:
            regs.column = cur->uleb128();
            break;
        case DW_LNS::negate_stmt:
            regs.is_stmt = !regs.is_stmt;
            break;
        case DW_LNS::set_basic_block:
            regs.basic_block = true;
            break;
        case DW_LNS::const_add_pc:
            uarg = (255 - m->opcode_base) / m->line_range;
            goto advance_pc;
        case DW_LNS::fixed_advance_pc:
            regs.address += cur->fixed<uhalf>();
            regs.op_index = 0;
            break;
        case DW_LNS::set_prologue_end:
            regs.prologue_end = true;
            break;
        case DW_LNS::set_epilogue_begin:
            regs.epilogue_begin = true;
            break;
        case DW_LNS::set_isa:
            regs.isa = cur->uleb128();
            break;
        default:
            throw format_error("unknown line number opcode " +
                               to_string((DW_LNS)opcode));
        }
        return ((DW_LNS)opcode == DW_LNS::copy);
    } else { // opcode == 0
        // 处理扩展操作码的部分
        assert(opcode == 0);
        uint64_t length = cur->uleb128();
        section_offset end = cur->get_section_offset() + length;
        opcode = cur->fixed<ubyte>();
        switch ((DW_LNE)opcode) {
        case DW_LNE::end_sequence:
            regs.end_sequence = true;
            entry = regs;
            regs.reset(m->default_is_stmt);
            break;
        case DW_LNE::set_address:
            regs.address = cur->address();
            regs.op_index = 0;
            break;
        case DW_LNE::define_file:
            m->read_file_entry(cur, false);
            break;
        case DW_LNE::set_discriminator:
            regs.discriminator = cur->uleb128();
            break;
        case DW_LNE::lo_user... DW_LNE::hi_user:
            throw runtime_error("vendor line number opcode " +
                                to_string((DW_LNE)opcode) +
                                " not implemented");
        default:
            // 在DWARF4之前的版本中，任何操作码号都可以视为厂商扩展
            throw format_error("unknown line number opcode " +
                               to_string((DW_LNE)opcode));
        }
#pragma GCC diagnostic pop
        if (cur->get_section_offset() > end)
            throw format_error("extended line number opcode exceeded its size");
        cur += end - cur->get_section_offset();
        return ((DW_LNE)opcode == DW_LNE::end_sequence);
    }
}
} // namespace dwarf