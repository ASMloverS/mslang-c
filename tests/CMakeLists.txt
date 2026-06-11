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
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    ms_add_test_with_core(T002 test_ms_assert_abort
        ${CMAKE_CURRENT_SOURCE_DIR}/core/test_ms_assert_abort.c)
    set_tests_properties(test_ms_assert_abort PROPERTIES WILL_FAIL TRUE)
endif()

# T002 Release-only: msInternalPanic symbol must be absent from Release build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    ms_add_symbol_absent_test(T002 test_symbol_absent_msInternalPanic
        mslang_core msInternalPanic)
endif()

# T004 CLI unit tests
ms_add_test_with_core(T004 test_cli_parse ${CMAKE_CURRENT_SOURCE_DIR}/cli/test_cli_parse.c)

# T005 public headers test
ms_add_test_with_core(T005 test_public_headers ${CMAKE_CURRENT_SOURCE_DIR}/core/test_public_headers.c)
