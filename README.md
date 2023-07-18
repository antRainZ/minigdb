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
./bin/minidbg ./bin/test/variable
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