find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Register a C unit test (no mslang_core linkage needed)
function(ms_add_test name src)
    add_executable(${name} ${src})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Register a C unit test that links mslang_core
function(ms_add_test_with_core name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE mslang_core)
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/include)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Register a golden test
function(ms_add_golden_test name cmd input expected)
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/golden_runner.py
                --cmd $<TARGET_FILE:${cmd}>
                --input ${input}
                --expected ${expected})
endfunction()

# Framework self-tests (T003)
ms_add_test(test_framework_self ${CMAKE_CURRENT_SOURCE_DIR}/core/test_framework_self.c)
ms_add_test(test_framework_fail ${CMAKE_CURRENT_SOURCE_DIR}/core/test_framework_fail.c)
set_tests_properties(test_framework_fail PROPERTIES WILL_FAIL TRUE)

# T002 common utils test
ms_add_test_with_core(test_common_utils ${CMAKE_CURRENT_SOURCE_DIR}/core/test_common_utils.c)
