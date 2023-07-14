cmake_minimum_required (VERSION 3.0)
project (MiniDbg)

add_compile_options(-std=c++14)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# 编译libdwarf.so
aux_source_directory(src/dwarf DWARF_LIB_SRC)
add_library(dwarf SHARED ${DWARF_LIB_SRC})
set_target_properties(dwarf PROPERTIES OUTPUT_NAME dwarf)

# 编译libelf.so
aux_source_directory(src/elf ELF_LIB_SRC)
add_library(elf SHARED ${ELF_LIB_SRC})
set_target_properties(elf PROPERTIES OUTPUT_NAME elf)

include_directories(include)
add_executable(minidbg src/minidbg.cpp src/linenoise.c)


add_executable(hello test/hello.cpp)
set_target_properties(hello
                      PROPERTIES COMPILE_FLAGS "-gdwarf-4 -O0")

add_executable(variable test/variable.cpp)
set_target_properties(variable
                      PROPERTIES COMPILE_FLAGS "-gdwarf-2 -O0")

add_executable(unwinding test/stack_unwinding.cpp)
set_target_properties(unwinding
                      PROPERTIES COMPILE_FLAGS "-gdwarf-4 -O0")

target_link_libraries(minidbg dwarf)              
target_link_libraries(minidbg elf)
set_target_properties(minidbg
                      PROPERTIES COMPILE_FLAGS "-g -O0")