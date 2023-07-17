# 简介
主要是对 [A mini x86 linux debugger for teaching purposes Topics](https://github.com/TartanLlama/minidbg) 进行修改
支持 x86_64 和 aarch 64 架构

# 编译
```sh
rm -rf build && mkdir -p build && cd build 
cmake ..
make -j4
```

# 运行

```sh
cd build
./minidbg ./variable
```

# 问题

## x86_64

```sh
  #include <stdio.h>
  void funca(long arg1,long arg2,long arg3,long arg4,long arg5,long arg6) {
>     long foo = 1;
      printf("foo=%ld,arg1=%ld,arg2=%ld\n",foo,arg1,arg2);
      printf("arg3=%ld,arg4=%ld,arg5=%ld\n",arg3,arg4,arg5);
      long a = 3;
  
minidbg> var
terminate called after throwing an instance of 'std::out_of_range'
  what():  DIE does not have attribute DW_AT_low_pc
```

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