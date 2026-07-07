cmake_minimum_required(VERSION 3.21)
project(mslang C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Main library (source files added incrementally by later tasks)
add_library(mslang_core STATIC
    src/core/ms_alloc.c
    src/core/ms_vec.c
    src/core/ms_hash.c
    src/core/ms_cli.c
    src/lexer/ms_lexer.c
    src/lexer/ms_lexer_unescape.c
    src/lexer/ms_lexer_print.c
    src/parser/ms_arena.c
    src/parser/ms_parser.c
    src/parser/ms_parse_expr.c
    src/parser/ms_ast_print.c
    src/compiler/ms_chunk.c
    src/compiler/ms_scope.c
    src/compiler/ms_compiler.c
    src/compiler/ms_disasm.c
    src/runtime/ms_value.c
)
target_include_directories(mslang_core PUBLIC include PRIVATE src)

# Executable
add_executable(mslang src/main.c)
target_link_libraries(mslang PRIVATE mslang_core)

# Compiler options and platform settings must come after target definitions
include(cmake/CompilerOptions.cmake)
include(cmake/Platform.cmake)

enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
