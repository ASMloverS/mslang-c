# P0-T004 CLI 骨架：子命令与标志解析

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `mslang` 命令行工具的完整入口：子命令派发（`run`/`compile`/`disasm`/`tokens`/`parse`）、全局标志解析（`-B`/`--no-cache`/`--hash-cache`/`-v`）、环境变量读取。各子命令的实际功能由后续任务填充；本任务只建立调度框架并输出 `usage`。这样 golden runner（T003）和后续 lexer golden 测试（T016）可以立即调用 `mslang tokens` 子命令。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T002 | 内存工具（`ms_common.h`）就绪 |
| P0-T003 | 测试框架就绪（本任务可被 golden runner 调用） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `execution.md` | §2 CLI 接口（子命令/标志/环境变量） |
| `c-style.md` | §main 函数与 CLI 规范 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/cli.h          # CLI 上下文结构与接口
src/cli.c          # 参数解析与子命令派发
src/main.c         # 替换 T001 骨架（仅调用 cliMain）
```

### 关键结构体（`src/cli.h`）

```c
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
  const char* script;    // 脚本文件路径（或 NULL）
  int         scriptArgc; // sys.argv 中脚本参数数量
  char**      scriptArgv; // 脚本参数列表
};
```

### 关键函数签名

```c
// 解析 argc/argv，填充 struct MsCliCtx；失败返回 -1 并打印 usage
int  cliParse(int argc, char** argv, struct MsCliCtx* ctx);

// 读取环境变量并应用到 flags
void cliApplyEnv(struct MsCliFlags* flags);

// 派发到对应子命令（各实现函数在后续任务中注册）
int  cliRun(struct MsCliCtx* ctx);

// usage 输出
void cliUsage(void);
```

---

## 实现要点

1. **子命令识别**：`argv[1]` 若为 `run`/`compile`/`disasm`/`tokens`/`parse` 则为显式子命令；若为 `.ms` 文件则隐式 `run`（与 `execution.md §2.1` 一致）。
2. **标志解析顺序**：全局标志（`-B`/`-v`/`--no-cache`/`--hash-cache`）可位于子命令前或后；脚本路径后面的参数均视为脚本的 `sys.argv`，不再解析为标志。
3. **环境变量**：`cliApplyEnv` 读取 `MSLANG_DONT_WRITE_BYTECODE` / `MSLANG_HASH_CACHE` / `MSLANG_PATH`（存入 `ctx->flags`，`MSLANG_PATH` 留给模块系统 T090 使用）。命令行标志优先级高于环境变量。
4. **未实现子命令的占位**：本任务各子命令实现函数只打印 `"not implemented yet\n"` 并返回 0，不返回错误码——允许后续任务逐步替换。
5. **`src/main.c` 替换**：移除 T001 的骨架 `main()`，改为：

```c
// src/main.c（T004 版本）
#include "cli.h"

int main(int argc, char** argv) {
  struct MsCliCtx ctx;
  if (cliParse(argc, argv, &ctx) < 0) return 1;
  return cliRun(&ctx);
}
```

6. **`mslang_core` 库不含 main.c**：T001 的 CMakeLists 需调整，将 `src/main.c` 从库中移出，仅放在可执行文件 target；`src/cli.c` 加入 `mslang_core`。

---

## 验收标准（checklist）

- [ ] `mslang` 不带参数打印 usage 并退出码 1。
- [ ] `mslang run script.ms` 打印 "not implemented yet" 并退出码 0。
- [ ] `mslang compile dir/` 打印 "not implemented yet" 并退出码 0。
- [ ] `mslang disasm file.ms` 打印 "not implemented yet" 并退出码 0。
- [ ] `mslang tokens file.ms` 打印 "not implemented yet" 并退出码 0。
- [ ] `mslang parse file.ms` 打印 "not implemented yet" 并退出码 0。
- [ ] `mslang -B -v run script.ms`：flags.noCache=true，flags.verbose=true 被正确设置。
- [ ] `mslang --no-cache script.ms`：等价隐式 run，flags.noCacheRead=true。
- [ ] `MSLANG_DONT_WRITE_BYTECODE=1 mslang run x.ms`：flags.noCache=true。
- [ ] `MSLANG_HASH_CACHE=1 mslang run x.ms`：flags.hashCache=true。
- [ ] 命令行 `-B` 在 `MSLANG_DONT_WRITE_BYTECODE=0` 时仍有效（命令行优先）。
- [ ] `mslang run script.ms arg1 arg2`：`ctx.scriptArgc=2`，`ctx.scriptArgv[0]="arg1"`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/cli/test_cli_parse.c`）

```c
#include "ms_test.h"
#include "cli.h"

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
  cliParse(3, argv, &ctx);
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_TOKENS, "tokens cmd");
  MS_ASSERT_STR_EQ(ctx.script, "foo.ms", "script path");
}

static void testFlags(void) {
  char* argv[] = {"mslang", "-B", "--hash-cache", "-v", "run", "a.ms", NULL};
  struct MsCliCtx ctx;
  cliParse(6, argv, &ctx);
  MS_ASSERT_TRUE(ctx.flags.noCache,    "noCache");
  MS_ASSERT_TRUE(ctx.flags.hashCache,  "hashCache");
  MS_ASSERT_TRUE(ctx.flags.verbose,    "verbose");
}

static void testScriptArgs(void) {
  char* argv[] = {"mslang", "run", "s.ms", "a", "b", NULL};
  struct MsCliCtx ctx;
  cliParse(5, argv, &ctx);
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

```
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
- **标志与脚本参数的歧义**：若脚本文件名以 `-` 开头，解析器会误识别为标志；初版规定 `--` 终止标志解析（标准 POSIX 约定）。
- **子命令未实现时的占位**：`"not implemented yet"` 输出到 stdout 还是 stderr？建议输出到 stderr（不干扰 golden 比对），返回 0（不中断 CI）。
- **未覆盖**：`--help`/`--version` 初版简单打印 usage 和版本号（硬编码 "0.1.0"），无需解析复杂选项。
