# 软件断点设置

```cpp
// https://github.com/bminor/binutils-gdb/blob/a89e364b45a93acd20f48abd787ef5cb7c07f683/gdb/aarch64-tdep.c#L2273
/* AArch64 BRK software debug mode instruction.
   Note that AArch64 code is always little-endian.
   1101.0100.0010.0000.0000.0000.0000.0000 = 0xd4200000.  */
constexpr gdb_byte aarch64_default_breakpoint[] = {0x00, 0x00, 0x20, 0xd4};

// https://github.com/bminor/binutils-gdb/blob/gdb-9-branch/gdb/i386-tdep.c#L609
/* Use the program counter to determine the contents and size of a
   breakpoint instruction.  Return a pointer to a string of bytes that
   encode a breakpoint instruction, store the length of the string in
   *LEN and optionally adjust *PC to point to the correct memory
   location for inserting the breakpoint.

   On the i386 we have a single breakpoint that fits in a single byte
   and can be inserted anywhere.

   This function is 64-bit safe.  */

constexpr gdb_byte i386_break_insn[] = { 0xcc }; /* int 3 */
```