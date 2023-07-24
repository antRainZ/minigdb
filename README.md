# 简介
主要是对 [A mini x86 linux debugger for teaching purposes Topics](https://github.com/TartanLlama/minidbg) 进行修改
支持 x86_64 和 aarch 64 架构

```sh
# gcc11 生成 dwarf4 内容不一致
gcc version 9.5.0 (Ubuntu 9.5.0-1ubuntu1~22.04)
```

# 编译
```sh
rm -rf build && mkdir -p build && cd build 
cmake ..
make -j4
```

# 运行

```sh
cd build
./bin/minidbg ./bin/test/variable
./bin/minidbg ./bin/test/unwinding
```

# 工具
```sh
# 输出
./bin/tool/dump_tree ./bin/test/unwinding > unwinding.log

# 寻找某个值所在DIE
./bin/tool/find_pc ./bin/test/unwinding 5205
```

# 问题

## aarch64
```sh
minidbg> n
Set breakpoint at address 0x5555550974
Set breakpoint at address 0x5555550984
Set breakpoint at address 0x5555550994
Set breakpoint at address 0x4
Got signal Illegal instruction

minidbg> back
frame #0: 0x974 f
terminate called after throwing an instance of 'std::out_of_range'
  what():  Cannot find function
Aborted (core dumped)
```

# 参考
+ [DWARF for the Arm® 64-bit Architecture (AArch64)](https://github.com/ARM-software/abi-aa/blob/main/aadwarf64/aadwarf64.rst)
```cpp
Getting the address of variable a in the dwarf4 format on the arm64 platform is now known
dwarf4 output:
<1><275>: Abbrev Number: 10 (DW_TAG_subprogram)
    <276>   DW_AT_external    : 1
    <276>   DW_AT_name        : (indirect string, offset: 0x29): funca
    <27a>   DW_AT_decl_file   : 1
    <27b>   DW_AT_decl_line   : 2
    <27c>   DW_AT_decl_column : 6
    <27d>   DW_AT_linkage_name: (indirect string, offset: 0xf8): _Z5funcallllll
    <281>   DW_AT_low_pc      : 0xad4
    <289>   DW_AT_high_pc     : 0xe4
    <291>   DW_AT_frame_base  : 1 byte block: 9c        (DW_OP_call_frame_cfa)
    <293>   DW_AT_GNU_all_tail_call_sites: 1
 <2><293>: Abbrev Number: 9 (DW_TAG_formal_parameter)
    <294>   DW_AT_name        : (indirect string, offset: 0x91): arg1
    <298>   DW_AT_decl_file   : 1
    <299>   DW_AT_decl_line   : 2
    <29a>   DW_AT_decl_column : 17
    <29b>   DW_AT_type        : <0x5e>
    <29f>   DW_AT_location    : 2 byte block: 91 48     (DW_OP_fbreg: -56)
 <2><2a2>: Abbrev Number: 9 (DW_TAG_formal_parameter)

Then output through gdb：
(gdb) disassemble 
Dump of assembler code for function funca(long, long, long, long, long, long):
   0x0000005555550ad4 <+0>:     stp     x29, x30, [sp, #-112]!
   0x0000005555550ad8 <+4>:     mov     x29, sp
   0x0000005555550adc <+8>:     str     x0, [sp, #56]
   0x0000005555550ae0 <+12>:    str     x1, [sp, #48]
   0x0000005555550ae4 <+16>:    str     x2, [sp, #40]
   0x0000005555550ae8 <+20>:    str     x3, [sp, #32]
   0x0000005555550aec <+24>:    str     x4, [sp, #24]
   0x0000005555550af0 <+28>:    str     x5, [sp, #16]

x29            0x7fffffeee0        549755809504
x30            0x5555550c28        366503857192
sp             0x7fffffeee0        0x7fffffeee0
pc             0x5555550af4        0x5555550af4 <funca(long, long, long, long, long, long)+32>

(gdb) p &a
$15 = (long *) 0x7fffffef38
(gdb) p (long)&a - (long)$sp
```