set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/test)

# 添加测试文件
add_executable(hello hello.cpp)
add_executable(variable variable.cpp)
add_executable(unwinding stack_unwinding.cpp)
