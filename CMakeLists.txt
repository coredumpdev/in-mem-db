cmake_minimum_required(VERSION 3.16)
project(ykr_cache C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

add_executable(main
  main.c
  core/hash_table/hash-table.c
  parser/ast.c
  parser/lexer.c
  parser/parser.c
  parser/eval.c
  utils/file.c
  net/tcp.c
)

target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(main PRIVATE Threads::Threads)
