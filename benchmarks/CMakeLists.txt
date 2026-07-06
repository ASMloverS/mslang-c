# Lexer microbenchmark (T016)
add_executable(bench_lexer bench_lexer.c)
target_link_libraries(bench_lexer PRIVATE mslang_core)
target_include_directories(bench_lexer PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Parser microbenchmark (T036)
add_executable(bench_parser bench_parser.c)
target_link_libraries(bench_parser PRIVATE mslang_core)
target_include_directories(bench_parser PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src)

# Compiler throughput microbenchmark (T048)
add_executable(bench_compiler bench_compiler.c)
target_link_libraries(bench_compiler PRIVATE mslang_core)
target_include_directories(bench_compiler PRIVATE ${PROJECT_SOURCE_DIR}/include)
