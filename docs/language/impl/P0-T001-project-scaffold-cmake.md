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
| `c-style.md` | §1.3 文件编码与换行（UTF-8/LF/trim，对应 .editorconfig） |
| `c-style.md` | §2 文件结构（§2.1 文件命名、§2.4 #include 顺序、§2.5 文件头注释） |
| `c-style.md` | §3 命名约定（对应 .clang-tidy）、§5 格式排版（对应 .clang-format）、§13.1 工具链文件清单 |
| `execution.md` | §`__mscache__` 字节码缓存 / `.msc` 格式（.gitignore 条目来源） |

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
tests/ci/build_check.sh       # 构建验证脚本
benchmarks/CMakeLists.txt
.editorconfig                 # UTF-8 LF、trim 行尾空白、末尾换行（c-style.md §13.1）
.clang-format                 # 对应 c-style.md §5 格式规则
.clang-tidy                   # 对应 c-style.md §3 命名规则
.gitignore                    # 追加 build/ __mscache__/ *.msc
```

### 根 CMakeLists.txt 关键内容

```cmake
cmake_minimum_required(VERSION 3.21)
project(mslang C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 主库（后续任务逐步添加源文件）
add_library(mslang_core STATIC
    src/main.c          # 临时占位，后续移出
)
target_include_directories(mslang_core PUBLIC include)

# 可执行文件
add_executable(mslang src/main.c)
target_link_libraries(mslang PRIVATE mslang_core)

# include 必须在目标定义之后：cmake 文件内部直接引用已有目标
include(cmake/CompilerOptions.cmake)
include(cmake/Platform.cmake)

enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
```

### cmake/CompilerOptions.cmake

```cmake
# INTERFACE 库：编译选项通过 PUBLIC 传播给 mslang_core 和 mslang 两个目标
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
        $<$<CONFIG:Debug>:/Od /RTC1 /fsanitize:address>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
endif()

target_link_libraries(mslang_core PUBLIC  mslang_warnings)
target_link_libraries(mslang      PRIVATE mslang_warnings)
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
6. **src/main.c 骨架**：本任务只需让 `mslang` 可编译链接通过，向 stderr 打印占位错误信息并返回非零退出码即可；真正的 CLI 逻辑由 T004 实现。

```c
// main.c
// CLI entry point skeleton; full argument parsing is implemented in T004.

#include <stdio.h>

int main(int argc, char** argv) {
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
- [ ] 根目录存在 `.editorconfig`、`.clang-format`、`.clang-tidy`（`c-style.md §13.1` 要求）。
- [ ] `tests/ci/build_check.sh` 存在且在 Linux/macOS 下 `bash tests/ci/build_check.sh` 输出 `BUILD OK`。

---

## 测试用例（C 单测 / .ms）

### 编译验证（CI shell 脚本，非 C 单测）

```bash
# tests/ci/build_check.sh
#!/usr/bin/env bash
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
# 确认退出码非零（在 if 被测试上下文中，避免 set -e 提前中止）
if ./build/mslang; then
  echo "ERROR: mslang should exit non-zero" >&2
  exit 1
fi
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
- **CMake 最低版本**：要求 3.21（3.21 起 MSVC 对 `CMAKE_C_STANDARD 17` 的识别更完整；`$<CONFIG:…>` 生成器表达式自 CMake 2.8 起即受支持，非 3.21 新特性）；低版本 CI 需升级。
- **MSVC C17 支持**：MSVC 的 C17 支持为 partial（缺少 `_Atomic`、复合字面量等特性）；`CMAKE_C_STANDARD_REQUIRED ON` 可阻止 CMake 静默降级，但无法补全 MSVC 未实现的 C17 特性。
- **src/main.c 占位**：本任务的 main.c 是临时骨架；T004 会替换其内容为真正的 CLI 解析逻辑，届时需将 `src/main.c` 从 `mslang_core` 库移出，避免 ODR 冲突。
- **ws2_32 依赖传播**：`Platform.cmake` 将 `ws2_32` 以 `PRIVATE` 挂载在 `mslang_core`；该依赖不会自动传播给链接 `mslang_core` 的下游目标。T185（stdlib/socket）的 CMakeLists 需在 Windows 下再次显式声明 `target_link_libraries(... ws2_32)`。
- **未覆盖**：Android/iOS 交叉编译留待后续需求；Emscripten/WASM 目标不在初版范围内。
