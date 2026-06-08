# Test registration populated by T003 (ms_test.h framework).

add_executable(test_common_utils ${CMAKE_CURRENT_SOURCE_DIR}/core/test_common_utils.c)
target_include_directories(test_common_utils PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_common_utils PRIVATE mslang_core)
add_test(NAME test_common_utils COMMAND test_common_utils)
