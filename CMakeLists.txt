cmake_minimum_required(VERSION 3.16)
project(ykr_cache C)

# --- Dil standardı: gnu17 (Makefile'daki gcc varsayılanıyla eşleşir) ---
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # gnu17 -> strdup/POSIX fonksiyonları görünür

# --- clangd (nvim LSP) için compile_commands.json üret ---
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Varsayılan build tipi: Debug (-g). Release için: -DCMAKE_BUILD_TYPE=Release ---
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

# --- Kaynaklar (multi-TU: her .c ayrı derlenir/linklenir) ---
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

# Kök dizini include yoluna ekle (main.c "core/..." gibi include'lar için)
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR})

# --- pthread (ileride TCP + mutex için gerekli olacak) ---
find_package(Threads REQUIRED)
target_link_libraries(main PRIVATE Threads::Threads)
