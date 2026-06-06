# P0-T001 项目骨架与 CMake 跨平台构建

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

建立 mslang 的工程根基：CMake 跨平台构建系统（Windows/Linux/macOS）、目录骨架、编译选项、CTest 集成。这是所有后续任务的基础，必须最先完成。

---

## 前置依赖

无。

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `overview.md` | §核心设计决策一览（宿主语言：纯 C17，构建：CMake 跨平台） |
| `c-style.md` | §文件组织与目录结构 |

---

## 待实现（C 文件 / 结构 / 函数）

### 目录结构

```
mslang-c/
  CMakeLists.txt              # 根构建脚本
  cmake/
    CompilerOptions.cmake     # C17 标志、警告、sanitizer
    Platform.cmake            # Windows/Linux/macOS 差异处理
  include/
    mslang/                   # 公共头文件（后续任务填充）
  src/
    main.c                    # CLI 入口（后续 T004 填充）
    core/                     # 核心运行时（后续任务填充）
    lexer/                    # 词法分析器
    parser/                   # 语法分析器
    compiler/                 # 编译器
    vm/                       # 虚拟机
    gc/                       # 垃圾回收器
    stdlib/                   # 标准库模块
  tests/
    CMakeLists.txt
    ms_test.h                 # 极简单测框架（T003 填充）
  benchmarks/
    CMakeLists.txt
  docs/                       # 已有文档（不改动）
```

### 新增文件（本任务）

```
CMakeLists.txt
cmake/CompilerOptions.cmake
cmake/Platform.cmake
src/main.c                    # 最小骨架，仅 main() 返回 0
include/mslang/.gitkeep
src/core/.gitkeep
src/lexer/.gitkeep
src/parser/.gitkeep
src/compiler/.gitkeep
src/vm/.gitkeep
src/gc/.gitkeep
src/stdlib/.gitkeep
tests/CMakeLists.txt
benchmarks/CMakeLists.txt
.gitignore                    # 追加 build/ __mscache__/ *.msc
```

### 根 CMakeLists.txt 关键内容

```cmake
cmake_minimum_required(VERSION 3.20)
project(mslang C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(cmake/CompilerOptions.cmake)
include(cmake/Platform.cmake)

# 主库（后续任务逐步添加源文件）
add_library(mslang_core STATIC
    src/main.c          # 临时占位，后续移出
)
target_include_directories(mslang_core PUBLIC include)

# 可执行文件
add_executable(mslang src/main.c)
target_link_libraries(mslang PRIVATE mslang_core)

enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
```

### cmake/CompilerOptions.cmake

```cmake
# GCC / Clang
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mslang_core PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:-g3 -O0 -fsanitize=address,undefined>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
    )
    target_link_options(mslang_core PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
elseif(MSVC)
    target_compile_options(mslang_core PRIVATE
        /W4 /WX /wd4100
        $<$<CONFIG:Debug>:/Od /RTC1>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
endif()
```

### cmake/Platform.cmake

```cmake
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
```

---

## 实现要点

1. **C17 强制**：`CMAKE_C_STANDARD 17`，`EXTENSIONS OFF`（不使用 GNU 扩展）。禁止 MSVC 降级。
2. **零外部依赖**：不引入任何 `find_package`（所有功能自实现，决策见计划）。
3. **sanitizer only in Debug**：用 `$<CONFIG:Debug>` 生成器表达式，避免污染 Release 二进制。
4. **Windows 网络**：`ws2_32` 链接放在 Platform.cmake，后续网络模块（T185）依赖。
5. **CTest 集成**：根 CMakeLists 调用 `enable_testing()`；`tests/CMakeLists.txt` 由 T003 填充后注册用例。
6. **src/main.c 骨架**：本任务只需让 `mslang` 可编译链接通过，输出 `usage` 即可；真正的 CLI 逻辑由 T004 实现。

```c
// src/main.c（本任务最小骨架）
#include <stdio.h>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    fprintf(stderr, "mslang: no command given\n");
    return 1;
}
```

---

## 验收标准（checklist）

- [ ] `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build` 在 Linux/macOS 下编译通过，无警告无错误。
- [ ] `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build` 在 Windows（MSVC/MinGW）下编译通过。
- [ ] `build/mslang` 可执行文件存在，运行返回非零退出码。
- [ ] `cmake --build build --config Release` 通过（Release 配置无 sanitizer 链接错误）。
- [ ] `ctest --test-dir build` 不崩溃（测试目录为空时报"no tests"为正常）。
- [ ] 目录结构与上方规格一致（各子目录存在 `.gitkeep` 或 `CMakeLists.txt`）。
- [ ] `.gitignore` 包含 `build/`、`__mscache__/`、`*.msc`。

---

## 测试用例（C 单测 / .ms）

### 编译验证（CI shell 脚本，非 C 单测）

```bash
# tests/ci/build_check.sh
#!/usr/bin/env bash
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/mslang; test $? -ne 0   # 确认退出码非零
cmake -B build_rel -DCMAKE_BUILD_TYPE=Release
cmake --build build_rel
echo "BUILD OK"
```

> 本任务无 C 单测（测试框架由 T003 建立），仅构建验证。

---

## .ms 使用示例

N/A（VM 尚未建立）。

---

## Benchmark

N/A（此任务为工程地基，无性能指标）。

---

## 风险与边界

- **Windows 路径**：CMake 在 Windows 下生成 Visual Studio 解决方案或 MinGW Makefiles；注意 `ws2_32` 对 MinGW 也需链接。
- **CMake 最低版本**：要求 3.20（支持 `$<CONFIG:…>` 生成器表达式与 C17）；低版本 CI 需升级。
- **src/main.c 占位**：本任务的 main.c 是临时骨架；T004 会替换其内容为真正的 CLI 解析逻辑，届时需将 `src/main.c` 从 `mslang_core` 库移出，避免 ODR 冲突。
- **未覆盖**：Android/iOS 交叉编译留待后续需求；Emscripten/WASM 目标不在初版范围内。
