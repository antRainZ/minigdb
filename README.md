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