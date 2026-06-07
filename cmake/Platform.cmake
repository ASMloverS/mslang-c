if(WIN32)
    target_compile_definitions(mslang_core PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        MS_PLATFORM_WINDOWS
    )
    target_link_libraries(mslang_core PRIVATE ws2_32)
elseif(APPLE)
    target_compile_definitions(mslang_core PRIVATE MS_PLATFORM_MACOS)
else()
    target_compile_definitions(mslang_core PRIVATE MS_PLATFORM_LINUX)
    target_link_libraries(mslang_core PRIVATE m pthread)
endif()
