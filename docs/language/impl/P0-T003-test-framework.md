# P0-T003 极简 C 单测框架 `ms_test.h` + golden runner

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现零依赖的 C 单元测试框架（单头文件 `ms_test.h`）和 golden 文件比对 runner（CTest 集成），用于在 VM 可用（P4-T067）之前验证 lexer、parser、compiler 各模块。VM 可用后改用 `.ms` 脚本测试，但 C 单测框架继续服务于 GC、C API 等底层模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T001 | CMake 与目录结构就绪 |

---

## 设计文档引用

无（框架本身不对应语言设计文档，属工程基础设施）。

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
tests/ms_test.h                   # 单头文件测试框架（无 .c 实现）
tests/golden_runner.py            # golden 文件比对脚本（Python 3，无第三方库）
tests/CMakeLists.txt              # CTest 注册工具函数
```

### `tests/ms_test.h`

```c
// 零依赖单头文件，直接 #include 到测试 .c 文件
#ifndef MS_TEST_H
#define MS_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 内部计数器（文件级静态，每个测试 .c 独立统计）
static int msTest_passed_ = 0;
static int msTest_failed_ = 0;
static const char* msTest_current_ = NULL;

// 注册并运行一个测试函数
#define MS_RUN(fn) do {             \
    msTest_current_ = #fn;          \
    fn();                           \
} while(0)

// 断言：整数相等（int64_t 宽化）
#define MS_ASSERT_EQ(actual, expected, msg) do {                          \
    int64_t a_ = (int64_t)(actual);                                        \
    int64_t e_ = (int64_t)(expected);                                      \
    if (a_ == e_) { msTest_passed_++; }                                    \
    else {                                                                  \
        fprintf(stderr, "FAIL [%s] %s: got %lld, want %lld\n",            \
                msTest_current_, (msg), (long long)a_, (long long)e_);    \
        msTest_failed_++;                                                   \
    }                                                                       \
} while(0)

// 断言：字符串相等
#define MS_ASSERT_STR_EQ(actual, expected, msg) do {                      \
    const char* a_ = (actual);                                             \
    const char* e_ = (expected);                                           \
    if (a_ && e_ && strcmp(a_, e_) == 0) { msTest_passed_++; }            \
    else {                                                                  \
        fprintf(stderr, "FAIL [%s] %s: got \"%s\", want \"%s\"\n",        \
                msTest_current_, (msg), a_ ? a_ : "(null)", e_);           \
        msTest_failed_++;                                                   \
    }                                                                       \
} while(0)

// 断言：条件为真
#define MS_ASSERT_TRUE(cond, msg) do {                                    \
    if (cond) { msTest_passed_++; }                                        \
    else {                                                                  \
        fprintf(stderr, "FAIL [%s] %s: condition is false\n",             \
                msTest_current_, (msg));                                    \
        msTest_failed_++;                                                   \
    }                                                                       \
} while(0)

// 断言：条件为假
#define MS_ASSERT_FALSE(cond, msg) MS_ASSERT_TRUE(!(cond), msg)

// 断言：两个内存块相等
#define MS_ASSERT_MEM_EQ(actual, expected, len, msg) do {                 \
    if (memcmp((actual), (expected), (len)) == 0) { msTest_passed_++; }   \
    else {                                                                  \
        fprintf(stderr, "FAIL [%s] %s: memory mismatch (%zu bytes)\n",    \
                msTest_current_, (msg), (size_t)(len));                    \
        msTest_failed_++;                                                   \
    }                                                                       \
} while(0)

// 标记当前测试失败并打印消息（无条件）
#define MS_FAIL(msg) do {                                                  \
    fprintf(stderr, "FAIL [%s] %s\n", msTest_current_, (msg));            \
    msTest_failed_++;                                                       \
} while(0)

// 打印汇总并返回退出码（0=全过，1=有失败）
static inline int msTestSummary(void) {
    fprintf(stderr, "\n%d passed, %d failed\n",
            msTest_passed_, msTest_failed_);
    return msTest_failed_ > 0 ? 1 : 0;
}

#endif // MS_TEST_H
```

### `tests/golden_runner.py`

```python
#!/usr/bin/env python3
"""
golden_runner.py  --cmd CMD --input INPUT --expected EXPECTED_FILE

运行 CMD 并将 INPUT 文件路径作为参数传入，比对 stdout 与 EXPECTED_FILE 内容。
退出码：0=匹配，1=不匹配，2=运行错误。
用于 CTest add_test 驱动 lexer/parser/compiler golden 测试。
"""
import sys, subprocess, argparse, pathlib

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cmd",      required=True, nargs="+")
    p.add_argument("--input",    required=True)
    p.add_argument("--expected", required=True)
    args = p.parse_args()

    try:
        result = subprocess.run(
            args.cmd + [args.input],
            capture_output=True, text=True, timeout=10)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    expected = pathlib.Path(args.expected).read_text(encoding="utf-8")
    if result.stdout == expected:
        return 0
    else:
        import difflib
        diff = difflib.unified_diff(
            expected.splitlines(keepends=True),
            result.stdout.splitlines(keepends=True),
            fromfile="expected", tofile="actual")
        sys.stderr.writelines(diff)
        return 1

sys.exit(main())
```

### `tests/CMakeLists.txt` 工具函数

```cmake
# 注册 C 单测
function(ms_add_test name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE mslang_core)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# 注册 golden 测试
function(ms_add_golden_test name cmd input expected)
    add_test(NAME ${name}
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/golden_runner.py
                --cmd $<TARGET_FILE:${cmd}>
                --input ${input}
                --expected ${expected})
endfunction()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
```

---

## 实现要点

1. **单头文件**：`ms_test.h` 无 `.c` 实现，直接 `#include` 到测试文件，避免链接依赖。
2. **静态计数器**：`msTest_passed_`/`msTest_failed_` 为 `static`，每个测试可执行文件独立，互不干扰（每个 `.c` 编译为独立可执行文件）。
3. **golden_runner.py**：使用 Python 3 标准库（无 pip 依赖），`subprocess` 调用 CLI 工具，`difflib` 输出差异。CTest 通过 `find_package(Python3)` 定位解释器。
4. **`ms_add_test`**：自动链接 `mslang_core` 和 `tests/` 头路径；T001 的 `mslang_core` 需分离 `main.c`（T004 实现后处理）。
5. **超时**：`golden_runner.py` 设 10 秒超时，防止死循环 CLI 工具阻塞 CI。

---

## 验收标准（checklist）

- [ ] `tests/ms_test.h` 存在且可直接 `#include`。
- [ ] 编写一个示例测试 `tests/core/test_framework_self.c`（验证 `MS_ASSERT_EQ` 通过与失败情况），`ctest -R test_framework_self` 通过。
- [ ] `MS_ASSERT_EQ` 在值不匹配时打印 FAIL 行且 `main()` 返回 1。
- [ ] `MS_ASSERT_STR_EQ` 对 `NULL` 不崩溃（打印 `"(null)"`）。
- [ ] `tests/golden_runner.py` 对 stdout 完全匹配时退出码 0，不匹配时退出码 1 且输出 diff。
- [ ] `cmake --build build && ctest --test-dir build` 全部注册测试通过。
- [ ] Python 3 不可用时 `ms_add_golden_test` 优雅跳过（`find_package` 带 `REQUIRED` 则报错，初版按 REQUIRED 处理，CI 确保 Python 3 可用）。

---

## 测试用例（C 单测 / .ms）

### 框架自验测试（`tests/core/test_framework_self.c`）

```c
#include "ms_test.h"

static void testPassingAsserts(void) {
    MS_ASSERT_EQ(1 + 1, 2, "1+1==2");
    MS_ASSERT_STR_EQ("hello", "hello", "str eq");
    MS_ASSERT_TRUE(1 > 0, "1>0");
    MS_ASSERT_FALSE(0, "false");
}

// 注意：验证"失败路径"需要单独的 negative test 可执行文件
// 这里只验证"通过路径"的计数

int main(void) {
    MS_RUN(testPassingAsserts);
    // 期望：passed=4, failed=0
    int code = msTestSummary();
    // 此处 code 应为 0
    return code;
}
```

### golden runner 自验（shell / CI 脚本）

```bash
# 手动验证（golden_runner.py 本身的测试）
echo "hello world" > /tmp/expected.txt
# 构造一个输出 "hello world\n" 的 cmd
echo '#!/bin/sh; echo "hello world"' > /tmp/hello.sh; chmod +x /tmp/hello.sh
python3 tests/golden_runner.py --cmd /tmp/hello.sh --input /dev/null \
        --expected /tmp/expected.txt
# 期望退出码 0
```

---

## .ms 使用示例

N/A（测试框架为 C 层工具，不暴露给脚本层）。

---

## Benchmark

N/A（测试框架本身不需性能指标）。

---

## 风险与边界

- **Windows `echo` 差异**：golden_runner.py 在 Windows 下 `\r\n` 换行可能导致误判；`text=True` + `subprocess` 自动换行转换应解决（但需在 CI 验证）。
- **静态计数器线程安全**：单测设计为单线程执行，无并发安全需求；并发测试留给 `.ms` 层。
- **`test_framework_self.c` 与 `mslang_core` 的依赖**：此测试只需 `ms_test.h` 不需 `mslang_core`；T003 可让 `ms_add_test` 支持可选的 `libs` 参数。
- **未覆盖**：benchmark 框架（`.ms` 层）在 T067 后的任务中按需引入；本任务只提供 C 单测基础设施。
