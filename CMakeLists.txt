cmake_minimum_required (VERSION 3.0)
project (MiniDbg)

add_compile_options(
    "-gdwarf-4" 
    "-O0"
    "-std=c++14"
)

include_directories(include)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# 编译libdwarf.so
aux_source_directory(src/dwarf DWARF_LIB_SRC)
add_library(dwarf SHARED ${DWARF_LIB_SRC})
set_target_properties(dwarf PROPERTIES OUTPUT_NAME dwarf)

# 编译libelf.so
aux_source_directory(src/elf ELF_LIB_SRC)
add_library(elf SHARED ${ELF_LIB_SRC})
set_target_properties(elf PROPERTIES OUTPUT_NAME elf)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
add_executable(minidbg src/minidbg.cpp src/linenoise.c)

target_link_libraries(minidbg dwarf)              
target_link_libraries(minidbg elf)

add_subdirectory(tool)
add_subdirectory(test)