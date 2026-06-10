# P0-T004 CLI 骨架：子命令与标志解析

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 `mslang` 命令行工具的完整入口：子命令派发（`run`/`compile`/`disasm`/`tokens`/`parse`）、全局标志解析（`-B`/`--no-cache`/`--hash-cache`/`-v`）、环境变量读取。各子命令的实际功能由后续任务填充；本任务只建立调度框架并输出 `usage`。这样 golden runner（T003）和后续 lexer golden 测试（T016）可以立即调用 `mslang tokens` 子命令。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T002 | 内存工具（`ms_common.h`）就绪 |
| P0-T003 | 测试框架就绪（本任务可被 golden runner 调用） |
| —（文档） | 先更新 `execution.md §2.1` 补充 `tokens`/`parse` 调试子命令的正式定义（可标注「调试用，输出格式待定」），使本任务实现有据可依 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `execution.md` | §2 CLI 接口（子命令/标志/环境变量）。注：`tokens`/`parse` 两个调试子命令的正式定义需**先**补充至 §2.1（见前置依赖），再据此实现本任务。 |
| `c-style.md` | §3 命名约定、§7 函数设计（返回值与错误码约定）。注：CLI 的退出码与 usage 行为属于本任务自定义约定。 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_cli.h   # CLI 上下文结构与接口
src/core/ms_cli.c         # 参数解析与子命令派发
src/main.c                # 替换 T001 骨架（仅调用 cliParse + cliRun）
```

### 关键结构体（`include/mslang/ms_cli.h`）

```c
// ms_cli.h — CLI 上下文结构与接口
#pragma once

#include <stdbool.h>

// 全局 CLI 选项（由环境变量与命令行标志合并）
struct MsCliFlags {
  bool noCache;          // -B / MSLANG_DONT_WRITE_BYTECODE=1
  bool noCacheRead;      // --no-cache（完全跳过读写）
  bool hashCache;        // --hash-cache / MSLANG_HASH_CACHE=1
  bool verbose;          // -v
};

// 子命令枚举
typedef enum MsCliCmd {
  CLI_CMD_RUN,
  CLI_CMD_COMPILE,
  CLI_CMD_DISASM,
  CLI_CMD_TOKENS,    // 词法分析结果（P1 实现后填充）
  CLI_CMD_PARSE,     // AST 转储（P2 实现后填充）
  CLI_CMD_UNKNOWN,
} MsCliCmd;

struct MsCliCtx {
  MsCliCmd    cmd;
  struct MsCliFlags  flags;
  const char* script;    // 位置参数（或 NULL）：run/tokens/parse 为 .ms 脚本路径，
                         // disasm 为 .ms/.msc 文件，compile 为首个路径（多路径见 T094）
  int         scriptArgc; // sys.argv 中脚本参数数量
  char**      scriptArgv; // 脚本参数列表
};
```

### 关键函数签名

```c
// 解析 argc/argv，填充 struct MsCliCtx；失败返回 -1 并打印 usage
int  cliParse(int argc, char** argv, struct MsCliCtx* ctx);

// 读取环境变量并应用到 flags（由 cliParse 在解析命令行标志前内部调用）
void cliApplyEnv(struct MsCliFlags* flags);

// 派发到对应子命令（各实现函数在后续任务中注册）
int  cliRun(struct MsCliCtx* ctx);

// usage 输出
void cliUsage(void);
```

---

## 实现要点

1. **子命令识别**：`argv[1]` 若为 `run`/`compile`/`disasm`/`tokens`/`parse` 则为显式子命令；若为 `.ms` 文件则隐式 `run`（与 `execution.md §2.1` 一致）。`disasm` 子命令的位置参数允许 `.ms` 或 `.msc` 后缀，解析层不限定文件后缀（`execution.md §2.1` 定义 `disasm <file.ms | file.msc>`）。
2. **标志解析顺序**：全局标志（`-B`/`-v`/`--no-cache`/`--hash-cache`）可位于子命令前或后；脚本路径后面的参数均视为脚本的 `sys.argv`，不再解析为标志。
3. **环境变量**：`cliApplyEnv` 由 `cliParse` 在解析命令行标志**之前**内部调用——先以环境变量填充 `flags` 默认值，再由命令行标志覆盖（命令行优先级高于环境变量，`execution.md §2.3`）。读取 `MSLANG_DONT_WRITE_BYTECODE` / `MSLANG_HASH_CACHE`；取值为 `0` 或未设置均视为关闭（不反向强制），故 `MSLANG_DONT_WRITE_BYTECODE=0` 时命令行 `-B` 仍有效。`MSLANG_PATH` 属于模块系统（T090），本任务不读取。
4. **未实现子命令的占位**：本任务各子命令实现函数只向 **stderr** 打印 `"not implemented yet\n"` 并返回 0，不返回错误码——不干扰 golden 比对，允许后续任务逐步替换。
5. **`src/main.c` 替换**：移除 T001 的骨架 `main()`，改为：

```c
// src/main.c（T004 版本）
#include "mslang/ms_cli.h"

int main(int argc, char** argv) {
  struct MsCliCtx ctx;
  if (cliParse(argc, argv, &ctx) < 0) {
    return 1;
  }
  return cliRun(&ctx);
}
```

6. **`mslang_core` 库不含 main.c**：T001 的 CMakeLists 需调整，将 `src/main.c` 从库中移出，仅放在可执行文件 target；`src/core/ms_cli.c` 加入 `mslang_core`。

---

## 验收标准（checklist）

- [x] `mslang` 不带参数打印 usage 并退出码 1。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang run script.ms` 向 **stderr** 打印 "not implemented yet"，stdout 为空，退出码 0。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang compile dir/` 向 **stderr** 打印 "not implemented yet"，stdout 为空，退出码 0。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang disasm file.ms` 向 **stderr** 打印 "not implemented yet"，stdout 为空，退出码 0。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang disasm file.msc` 向 **stderr** 打印 "not implemented yet"，stdout 为空，退出码 0。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang tokens file.ms` 向 **stderr** 打印 "not implemented yet"，stdout 为空，退出码 0。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang parse file.ms` 向 **stderr** 打印 "not implemented yet"，stdout 为空，退出码 0。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang -B -v run script.ms`：flags.noCache=true，flags.verbose=true 被正确设置。 <!-- v:ctest:test_cli_parse -->
- [x] `mslang --no-cache script.ms`：等价隐式 run，flags.noCacheRead=true。 <!-- v:ctest:test_cli_parse -->
- [ ] `MSLANG_DONT_WRITE_BYTECODE=1 mslang run x.ms`：flags.noCache=true。 <!-- v:manual:需手动设置环境变量执行 -->
- [ ] `MSLANG_HASH_CACHE=1 mslang run x.ms`：flags.hashCache=true。 <!-- v:manual:需手动设置环境变量执行 -->
- [ ] 命令行 `-B` 在 `MSLANG_DONT_WRITE_BYTECODE=0` 时仍有效（命令行优先）。 <!-- v:manual:需手动设置环境变量执行 -->
- [x] `mslang run script.ms arg1 arg2`：`ctx.scriptArgc=2`，`ctx.scriptArgv[0]="arg1"`。 <!-- v:ctest:test_cli_parse -->

> 验证提示：stdout 与 stderr 需分别重定向核对——`mslang run x.ms 2>/dev/null` 应无任何 stdout 输出；`mslang run x.ms 2>&1 >/dev/null` 应包含 "not implemented yet"。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/cli/test_cli_parse.c`）

```c
#include "ms_test.h"
#include "mslang/ms_cli.h"

static void testImplicitRun(void) {
  char* argv[] = {"mslang", "script.ms", NULL};
  struct MsCliCtx ctx;
  int r = cliParse(2, argv, &ctx);
  MS_ASSERT_EQ(r, 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_RUN, "implicit run");
  MS_ASSERT_STR_EQ(ctx.script, "script.ms", "script path");
}

static void testExplicitTokensCmd(void) {
  char* argv[] = {"mslang", "tokens", "foo.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(3, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_TOKENS, "tokens cmd");
  MS_ASSERT_STR_EQ(ctx.script, "foo.ms", "script path");
}

static void testFlags(void) {
  char* argv[] = {"mslang", "-B", "--hash-cache", "-v", "run", "a.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(6, argv, &ctx), 0, "parse ok");
  MS_ASSERT_TRUE(ctx.flags.noCache,    "noCache");
  MS_ASSERT_TRUE(ctx.flags.hashCache,  "hashCache");
  MS_ASSERT_TRUE(ctx.flags.verbose,    "verbose");
}

static void testScriptArgs(void) {
  char* argv[] = {"mslang", "run", "s.ms", "a", "b", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(5, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.scriptArgc, 2, "scriptArgc");
  MS_ASSERT_STR_EQ(ctx.scriptArgv[0], "a", "argv[0]");
}

int main(void) {
  MS_RUN(testImplicitRun);
  MS_RUN(testExplicitTokensCmd);
  MS_RUN(testFlags);
  MS_RUN(testScriptArgs);
  return msTestSummary();
}
```

---

## .ms 使用示例

> 以下为 shell 命令行调用示例，并非 `.ms` 脚本源码（`#` 为 shell 注释，不适用 `ms-style.md` 的 `//` 注释规范）。

```sh
# 命令行使用（shell）：
mslang script.ms                    # 运行脚本
mslang run script.ms arg1 arg2      # 显式 run + 传递参数
mslang compile src/                 # 预编译目录下所有 .ms 到 __mscache__
mslang disasm script.ms             # 反汇编字节码
mslang tokens script.ms             # 输出 token 流（调试用）
mslang parse script.ms              # 输出 AST 转储（调试用）
mslang -B run script.ms             # 禁止写入 __mscache__
mslang --no-cache script.ms         # 完全跳过缓存
mslang --hash-cache run script.ms   # 使用内容哈希失效
mslang -v run script.ms             # 详细输出（缓存命中/未命中/写入路径）
```

---

## Benchmark

N/A（CLI 解析仅在启动时执行一次，不影响稳态性能）。

---

## 风险与边界

- **`getenv` 跨平台**：Windows 下 `getenv` 线程安全性较弱；初版在 `main()` 启动时单次读取，无并发问题。
- **`compile` 多路径**：`execution.md §2.1` 定义 `compile` 可接收多个路径参数，但骨架阶段 `MsCliCtx` 仅保留 `script` 字段记录首个路径；多路径支持推迟到 compile 实现任务（T094）时再扩展结构。
- **标志与脚本参数的歧义**：若脚本文件名以 `-` 开头，解析器会误识别为标志；初版规定 `--` 终止标志解析（标准 POSIX 约定）。
- **子命令未实现时的占位**：占位输出写入 **stderr**，不影响 stdout 的 golden 比对；返回 0（不中断 CI）。
- **未覆盖**：`--help`/`--version` 初版简单打印 usage 和版本号（硬编码 "0.1.0"），无需解析复杂选项。
