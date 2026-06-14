# Lexer microbenchmark (T016)
add_executable(bench_lexer bench_lexer.c)
target_link_libraries(bench_lexer PRIVATE mslang_core)
target_include_directories(bench_lexer PRIVATE ${PROJECT_SOURCE_DIR}/include)
