find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Register a C unit test (no mslang_core linkage needed).
# task: task label for ctest -L, e.g. T003
function(ms_add_test task name src)
    add_executable(${name} ${src})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    target_link_libraries(${name} PRIVATE mslang_warnings)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# Register a C unit test that links mslang_core.
function(ms_add_test_with_core task name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE mslang_core)
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/include)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# Register a golden test (stdout exact-match via golden_runner.py).
function(ms_add_golden_test task name cmd input expected)
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/golden_runner.py
                --cmd $<TARGET_FILE:${cmd}>
                --input ${input}
                --expected ${expected})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# Register a .ms script test (available after P4-T067 when mslang can run scripts).
# Reuses golden_runner.py with mslang as the command.
function(ms_add_ms_test task name script expected)
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/golden_runner.py
                --cmd $<TARGET_FILE:mslang>
                --input ${script}
                --expected ${expected})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# Register a symbol-absence check test (runs check_symbol_absent.py against a library).
# lib_target: CMake target whose binary is checked (e.g. mslang_core).
# symbol: exact symbol name that must be absent.
function(ms_add_symbol_absent_test task name lib_target symbol)
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/ci/check_symbol_absent.py
                --lib $<TARGET_FILE:${lib_target}>
                --symbol ${symbol})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# T001: mslang with no arguments must exit non-zero (skeleton prints usage+error)
add_test(NAME test_mslang_exits_nonzero COMMAND mslang)
set_tests_properties(test_mslang_exits_nonzero PROPERTIES WILL_FAIL TRUE LABELS T001)

# Framework self-tests (T003)
ms_add_test(T003 test_framework_self ${CMAKE_CURRENT_SOURCE_DIR}/core/test_framework_self.c)
ms_add_test(T003 test_framework_fail ${CMAKE_CURRENT_SOURCE_DIR}/core/test_framework_fail.c)
set_tests_properties(test_framework_fail PROPERTIES WILL_FAIL TRUE)

# T002 common utils test
ms_add_test_with_core(T002 test_common_utils ${CMAKE_CURRENT_SOURCE_DIR}/core/test_common_utils.c)

# T002 Debug-only: MS_ASSERT(0) must abort (exit non-zero) in Debug build
ms_add_test_with_core(T002 test_ms_assert_abort
    ${CMAKE_CURRENT_SOURCE_DIR}/core/test_ms_assert_abort.c)
set_tests_properties(test_ms_assert_abort PROPERTIES
    WILL_FAIL TRUE CONFIGURATIONS Debug)

# T002 Release-only: msInternalPanic symbol must be absent from Release build
ms_add_symbol_absent_test(T002 test_symbol_absent_msInternalPanic
    mslang_core msInternalPanic)
set_tests_properties(test_symbol_absent_msInternalPanic PROPERTIES
    CONFIGURATIONS Release)

# T004 CLI unit tests
ms_add_test_with_core(T004 test_cli_parse ${CMAKE_CURRENT_SOURCE_DIR}/cli/test_cli_parse.c)

# T005 public headers test
ms_add_test_with_core(T005 test_public_headers ${CMAKE_CURRENT_SOURCE_DIR}/core/test_public_headers.c)

# T006 lexer framework test
ms_add_test_with_core(T006 test_lexer_framework ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_lexer_framework.c)

# T007 identifier and keyword recognition
ms_add_test_with_core(T007 test_ident_keywords ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_ident_keywords.c)

# T008 integer literal lexer test
ms_add_test_with_core(T008 test_int_literals ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_int_literals.c)

# T009 float literal lexer test
ms_add_test_with_core(T009 test_float_literals ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_float_literals.c)

# T010 string literal lexer test
ms_add_test_with_core(T010 test_string_literals ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_string_literals.c)

# T011 f-string lexer test
ms_add_test_with_core(T011 test_fstring ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_fstring.c)

# T012 bytes literal lexer test
ms_add_test_with_core(T012 test_bytes_literal ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_bytes_literals.c)

# T013 operators and delimiters lexer test
ms_add_test_with_core(T013 test_operators ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_operators.c)

# T014 comments and whitespace lexer test
ms_add_test_with_core(T014 test_comments_whitespace ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_comments_whitespace.c)

# T015 ASI (automatic semicolon insertion) lexer test
ms_add_test_with_core(T015 test_asi ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_asi.c)

# Register a golden test for `mslang tokens <file>` output.
# task: ctest label; name: test name; input: .ms path; expected: .expected path
# expect_exit (optional, default 0): expected process exit code
function(ms_add_tokens_test task name input expected)
    if(ARGC GREATER 4)
        set(_exit_arg --expect-exit ${ARGV4})
    else()
        set(_exit_arg "")
    endif()
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/golden_runner.py
                --cmd $<TARGET_FILE:mslang> tokens
                --input ${input}
                --expected ${expected}
                ${_exit_arg})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# T016 msTokenPrint C unit test
ms_add_test_with_core(T016 test_token_print
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer/test_token_print.c)

# T016 golden tests — lexer token dump
set(_GL ${CMAKE_CURRENT_SOURCE_DIR}/golden/lexer)

ms_add_tokens_test(T016 golden_lexer_basic_ident
    ${_GL}/basic_ident.ms
    ${_GL}/basic_ident.expected)

ms_add_tokens_test(T016 golden_lexer_int_literals
    ${_GL}/int_literals.ms
    ${_GL}/int_literals.expected)

ms_add_tokens_test(T016 golden_lexer_errors
    ${_GL}/errors.ms
    ${_GL}/errors.expected
    1)

ms_add_tokens_test(T016 golden_lexer_fstring
    ${_GL}/fstring.ms
    ${_GL}/fstring.expected)

ms_add_tokens_test(T016 golden_lexer_asi
    ${_GL}/asi.ms
    ${_GL}/asi.expected)

# T017 AST arena test
ms_add_test_with_core(T017 test_arena ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_arena.c)
target_include_directories(test_arena PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T018 Pratt parser framework test
ms_add_test_with_core(T018 test_pratt_framework
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_pratt_framework.c)
target_include_directories(test_pratt_framework PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T019 expression parser: unary/binary/power/bitwise/comparison/logical
ms_add_test_with_core(T019 test_expr_binary
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_expr_binary.c)
target_include_directories(test_expr_binary PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T020 if-expression (ternary) parser
ms_add_test_with_core(T020 test_if_expr
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_if_expr.c)
target_include_directories(test_if_expr PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T021 call/attr/index/slice/postfix parser
ms_add_test_with_core(T021 test_call_attr
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_call_attr.c)
target_include_directories(test_call_attr PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T022 container literals parser
ms_add_test_with_core(T022 test_container_literals
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_container_literals.c)
target_include_directories(test_container_literals PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T023 tuple literal / grouping parentheses parser
ms_add_test_with_core(T023 test_tuple
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_tuple.c)
target_include_directories(test_tuple PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T024 function literal / anonymous function parser
ms_add_test_with_core(T024 test_func_literal
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_func_literal.c)
target_include_directories(test_func_literal PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T025 make/recv expression parser
ms_add_test_with_core(T025 test_make_recv
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_make_recv.c)
target_include_directories(test_make_recv PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T026 var declaration / short declaration / assignment statement parser
ms_add_test_with_core(T026 test_var_assign
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_var_assign.c)
target_include_directories(test_var_assign PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T027 if / else if / else statement parser
ms_add_test_with_core(T027 test_if_stmt
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_if_stmt.c)
target_include_directories(test_if_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T028 for statement parser
ms_add_test_with_core(T028 test_for_stmt
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_for_stmt.c)
target_include_directories(test_for_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T029 switch statement parser
ms_add_test_with_core(T029 test_switch_stmt
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_switch_stmt.c)
target_include_directories(test_switch_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T030 jump statements (return/break/continue/pass/del/assert)
ms_add_test_with_core(T030 test_jump_stmts
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_jump_stmts.c)
target_include_directories(test_jump_stmts PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T031 try/catch/finally/raise
ms_add_test_with_core(T031 test_try_raise
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_try_raise.c)
target_include_directories(test_try_raise PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T032 go/select statement parser
ms_add_test_with_core(T032 test_go_select
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_go_select.c)
target_include_directories(test_go_select PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T033 with statement parser
ms_add_test_with_core(T033 test_with_stmt
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_with_stmt.c)
target_include_directories(test_with_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T034 func/class declaration parser
ms_add_test_with_core(T034 test_func_class_decl
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_func_class_decl.c)
target_include_directories(test_func_class_decl PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T035 import statement parser
ms_add_test_with_core(T035 test_import
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/test_import.c)
target_include_directories(test_import PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Register a golden test for `mslang parse <file>` output.
function(ms_add_parse_test task name input expected)
    if(ARGC GREATER 4)
        set(_exit_arg --expect-exit ${ARGV4})
    else()
        set(_exit_arg "")
    endif()
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/golden_runner.py
                --cmd $<TARGET_FILE:mslang> parse
                --input ${input}
                --expected ${expected}
                ${_exit_arg})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# T036 parser golden tests — AST dump
set(_GP ${CMAKE_CURRENT_SOURCE_DIR}/golden/parser)

ms_add_parse_test(T036 parser_hello
    ${_GP}/hello.ms
    ${_GP}/hello.expected)

ms_add_parse_test(T036 parser_func_decl
    ${_GP}/func_decl.ms
    ${_GP}/func_decl.expected)

ms_add_parse_test(T036 parser_class_decl
    ${_GP}/class_decl.ms
    ${_GP}/class_decl.expected)

ms_add_parse_test(T036 parser_control_flow
    ${_GP}/control_flow.ms
    ${_GP}/control_flow.expected)

ms_add_parse_test(T036 parser_import_stmts
    ${_GP}/import_stmts.ms
    ${_GP}/import_stmts.expected)

ms_add_parse_test(T036 parser_full_program
    ${_GP}/full_program.ms
    ${_GP}/full_program.expected)

# T037 bytecode chunk test
ms_add_test_with_core(T037 test_chunk
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_chunk.c)

# T038 compiler scope test
ms_add_test_with_core(T038 test_scope
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_scope.c)
target_include_directories(test_scope PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T039 expression compilation test
ms_add_test_with_core(T039 test_expr_compile
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_expr_compile.c)
target_include_directories(test_expr_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T040 variable load/store compilation test
ms_add_test_with_core(T040 test_variables
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_variables.c)
target_include_directories(test_variables PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T041 container build instruction compilation test
ms_add_test_with_core(T041 test_containers
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_containers.c)
target_include_directories(test_containers PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T042 control flow compilation test
ms_add_test_with_core(T042 test_control_flow
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_control_flow.c)
target_include_directories(test_control_flow PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T043 function compilation test
ms_add_test_with_core(T043 test_func_compile
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_func_compile.c)
target_include_directories(test_func_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T044 class compilation test
ms_add_test_with_core(T044 test_class_compile
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_class_compile.c)
target_include_directories(test_class_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T045 call compilation test
ms_add_test_with_core(T045 test_call_compile
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_call_compile.c)
target_include_directories(test_call_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T046 exception compilation test
ms_add_test_with_core(T046 test_exception_compile
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_exception_compile.c)
target_include_directories(test_exception_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T047 with/go/select/send/import compilation test
ms_add_test_with_core(T047 test_misc_compile
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_misc_compile.c)
target_include_directories(test_misc_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)

# T048 disassembler unit test
ms_add_test_with_core(T048 test_disasm
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/test_disasm.c)
target_include_directories(test_disasm PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Register a golden test for `mslang disasm <file>` output.
function(ms_add_disasm_test task name input expected)
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/golden_runner.py
                --cmd $<TARGET_FILE:mslang> disasm
                --input ${input}
                --expected ${expected})
    set_tests_properties(${name} PROPERTIES LABELS ${task})
endfunction()

# T049 MsValue/MsObject/MsType helper functions test
ms_add_test_with_core(T049 test_value ${CMAKE_CURRENT_SOURCE_DIR}/runtime/test_value.c)

# T048 disasm golden tests
set(_GD ${CMAKE_CURRENT_SOURCE_DIR}/golden/disasm)

ms_add_disasm_test(T048 golden_disasm_arith
    ${_GD}/arith.ms
    ${_GD}/arith.disasm)

ms_add_disasm_test(T048 golden_disasm_func
    ${_GD}/func.ms
    ${_GD}/func.disasm)

ms_add_disasm_test(T048 golden_disasm_closure
    ${_GD}/closure.ms
    ${_GD}/closure.disasm)

ms_add_disasm_test(T048 golden_disasm_try_catch
    ${_GD}/try_catch.ms
    ${_GD}/try_catch.disasm)

ms_add_disasm_test(T048 golden_disasm_import
    ${_GD}/import.ms
    ${_GD}/import.disasm)

# T050 simple STW mark-sweep GC unit test
ms_add_test_with_core(T050 test_gc_basic ${CMAKE_CURRENT_SOURCE_DIR}/gc/test_gc_basic.c)

# T051 MsFrame/MsThread + eval loop skeleton test
ms_add_test_with_core(T051 test_eval_basic ${CMAKE_CURRENT_SOURCE_DIR}/vm/test_eval_basic.c)
