set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/tool)

add_executable(dump-lines dump-lines.cc)
target_link_libraries(dump-lines dwarf)
target_link_libraries(dump-lines elf)

add_executable(dump-sections dump-sections.cc)
target_link_libraries(dump-sections	dwarf)
target_link_libraries(dump-sections	elf)

add_executable(dump-segments dump-segments.cc)
target_link_libraries(dump-segments dwarf)
target_link_libraries(dump-segments elf)

add_executable(dump-syms dump-syms.cc)
target_link_libraries(dump-syms dwarf)
target_link_libraries(dump-syms elf)

add_executable(dump_tree dump-tree.cc)
target_link_libraries(dump_tree dwarf)
target_link_libraries(dump_tree elf)

add_executable(find_pc find-pc.cc)
target_link_libraries(find_pc dwarf)
target_link_libraries(find_pc elf)