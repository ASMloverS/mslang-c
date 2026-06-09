# INTERFACE library: options propagate to mslang_core and mslang via PUBLIC/PRIVATE links
add_library(mslang_warnings INTERFACE)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mslang_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:-g3 -O0 -fsanitize=address,undefined>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
    )
    target_link_options(mslang_warnings INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
elseif(MSVC)
    target_compile_options(mslang_warnings INTERFACE
        /W4 /WX /wd4100
        /wd4127   # conditional expression is constant (do{}while(0) macro pattern)
        $<$<CONFIG:Debug>:/Od /RTC1>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
endif()

target_link_libraries(mslang_core PUBLIC  mslang_warnings)
target_link_libraries(mslang      PRIVATE mslang_warnings)
