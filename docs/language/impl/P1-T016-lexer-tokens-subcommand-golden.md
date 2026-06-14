# P1-T016 `tokens` 子命令与词法 golden 测试套件

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `mslang tokens <file>` 子命令（将 `.ms` 文件词法化并以文本格式打印所有 token），并搭建词法 golden 测试套件（基于 `tests/golden_runner.py`）。这是 P1 词法阶段的收尾任务，确保 T006–T015 实现的全部词法功能端到端可验证，同时建立后续 parser/compiler 测试的参考模式。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 ~ T015 | 全部词法子系统（T006：框架，T007：标识符/关键字，…T015：ASI） |
| P0-T003 | golden 对比 runner（`tests/golden_runner.py`） |
| P0-T004 | CLI 子命令框架（`CLI_CMD_TOKENS`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1 全部词法规则（参考输出格式） |
| `c-style.md` | §命名规范（输出字段使用英文标识符） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/core/ms_cli.c               # 实现 cliRunTokens(ctx) 分支（CLI_CMD_TOKENS case）
src/lexer/ms_lexer_print.c      # 新建：token 文本序列化
include/mslang/ms_lexer.h       # 导出 msTokenPrint(tok, src, fp)
tests/golden_runner.py          # 扩展 --cmd 支持多值（需支持 "mslang tokens" 子命令注入）
tests/CMakeLists.txt            # 扩展 ms_add_golden_test 包装函数以传入子命令
```

### 新增文件

```
tests/golden/lexer/             # golden 输入/期望输出目录
tests/ms/lexer/                 # .ms 词法测试输入文件
tests/CMakeLists.txt            # 追加所有 golden 测试用例
benchmarks/bench_lexer.c        # 词法 C microbench
```

---

## 实现要点

### 1. Token 文本格式（`msTokenPrint`）

每行一个 token，格式：

```
<line>:<col>  <KIND>  <repr>
```

- `<line>:<col>`：完整 printf 格式串 `"%3u:%-4u"`（行右对齐 3 位，列左对齐 4 位，合计 8 字符），
  后跟一个空格。`%u` 避免负数，列号 1-based，最大支持 999 行/9999 列；超出不报错，仍可输出。
- `<KIND>`：`MS_TOK_` 枚举名去掉 `MS_TOK_` 前缀后的大写字符串（如 `MS_TOK_IDENT` → `IDENT`），
  左对齐填充到 20 字符宽度（`"%-20s"`）。注意：`msTokenPrint` 内部使用独立映射表，
  **不**复用 `msTokName()`（后者面向错误诊断，对关键字返回小写、运算符返回符号字面量，
  语义与此处不同）。
- `<repr>`：token 的文本表示：
  - `MS_TOK_IDENT`：标识符原文。
  - `MS_TOK_INT`：十进制数值（`"%" PRId64`）。
  - `MS_TOK_FLOAT`：`%.17g` 精度。
  - `MS_TOK_STRING`/`MS_TOK_BYTES`/`MS_TOK_FSTRING_*`：原始字节（含引号，转义保留）。
  - 关键字/运算符/括号：固定符号字符串（`+`、`==`、`return` 等）。
  - `MS_TOK_NEWLINE`：`<newline>`（可见占位符）。
  - `MS_TOK_EOF`：`<eof>`。
  - `MS_TOK_ERROR`：`<error: …>`（省略号处填 `lex->errBuf`）。

完整格式串示例：`printf("%3u:%-4u %-20s %s\n", tok->pos.line, tok->pos.col, kind, repr);`

示例输出（对应 `x + 1\n`）：

```
  1:1   IDENT               x
  1:3   PLUS                +
  1:5   INT                 1
  1:6   NEWLINE             <newline>
  2:1   EOF                 <eof>
```

### 2. `cliRunTokens(struct MsCliCtx* ctx)`

`cliRun`（`src/core/ms_cli.c`）在 `switch(ctx->cmd)` 的 `CLI_CMD_TOKENS` 分支中调用此函数。

```c
int cliRunTokens(struct MsCliCtx* ctx) {
  // 1. 读取 ctx->script 文件到内存（msReadFile 辅助函数）
  // 2. 初始化 struct MsLexer
  // 3. bool sawError = false;
  // 4. 循环调用 msLexerNext，打印每个 token（msTokenPrint → stdout）
  //    - 遇 MS_TOK_ERROR 时 sawError = true（继续扫描，不中断）
  //    - 遇 MS_TOK_EOF 后打印 EOF 行并停止
  // 5. 返回 sawError ? 1 : 0
  //
  // 注意：不要用 lex.hasError 判断「全文是否出过错」——该标志是瞬时的，
  //       仅对最近一个 token 有效；循环结束时 lex.hasError 反映的是 EOF，
  //       而非全文错误状态。
}
```

### 3. Golden 测试结构

```
tests/golden/lexer/
  basic_ident.ms          # 输入：标识符、关键字
  basic_ident.expected    # 期望输出：token 列表文本
  int_literals.ms
  int_literals.expected
  float_literals.ms
  float_literals.expected
  string_escapes.ms
  string_escapes.expected
  fstring.ms
  fstring.expected
  bytes.ms
  bytes.expected
  operators.ms
  operators.expected
  asi.ms
  asi.expected
  comments.ms
  comments.expected
  errors.ms               # 触发词法错误的输入
  errors.expected         # 包含 <error: …> 行
```

### 4. CMake 集成

```cmake
# tests/CMakeLists.txt
#
# ms_add_golden_test 签名（见 tests/CMakeLists.txt 第 25 行）：
#   ms_add_golden_test(task name cmd input expected)
#   - task:     CTest 标签（如 T016）
#   - name:     测试名
#   - cmd:      CMake target 名（runner 用 $<TARGET_FILE:${cmd}>）
#   - input:    输入文件路径（相对 build 目录）
#   - expected: 期望输出文件路径
#
# 因 golden_runner.py 当前执行 "cmd input"，而 tokens 子命令需要
# "mslang tokens input"，需扩展 runner 支持多值 --cmd（runner 已有
# nargs="+"）并修改 ms_add_golden_test 包装函数注入 tokens 子命令。
# 可选方案：新增 ms_add_tokens_test(T016 name input expected) 辅助宏。
#
# 示例（扩展后）：
ms_add_tokens_test(T016 lexer_basic_ident
  "${CMAKE_CURRENT_SOURCE_DIR}/golden/lexer/basic_ident.ms"
  "${CMAKE_CURRENT_SOURCE_DIR}/golden/lexer/basic_ident.expected")
# ... 为每个 golden 文件重复（9 个用例）
```

### 5. 词法 Benchmark（`benchmarks/bench_lexer.c`）

```c
// 基准：对 ~1000 行 .ms 文件（含各类 token）重复词法化 1000 次
// 指标：tokens/sec（越高越好）
// 目标：Release 构建 > 50M tokens/sec（典型 C 词法器水平）
int main(void) {
  const char* src = loadFile("benchmarks/data/bench_1k.ms");
  uint32_t srcLen  = (uint32_t)strlen(src);
  uint64_t count   = 0;
  clock_t  start   = clock();
  for (int iter = 0; iter < 1000; iter++) {
    struct MsLexer lex;
    msLexerInit(&lex, src, srcLen, "bench");
    struct MsToken t;
    do { t = msLexerNext(&lex); count++; } while (t.kind != MS_TOK_EOF);
  }
  double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
  printf("%.2f M tokens/sec\n", (double)count / elapsed / 1e6);
  return 0;
}
```

---

## 验收标准（checklist）

- [ ] `mslang tokens tests/golden/lexer/basic_ident.ms` 输出与 `.expected` 文件逐字节一致。 <!-- v:golden:lexer_basic_ident -->
- [ ] 全部 golden 测试（`ctest -L T016`）通过。 <!-- v:ctest:test_token_print -->
- [ ] 词法错误输入（`errors.ms`）在 `<error: …>` 行后继续扫描，最终非零退出（`sawError` 机制）。 <!-- v:golden:lexer_errors -->
- [ ] `mslang tokens /dev/stdin` 可读标准输入（Linux/macOS；Windows 跳过此 case）。 <!-- v:manual:stdin 跨平台 -->
- [ ] benchmark 在 Release 构建中实际运行并打印 `tokens/sec` 数值（不要求特定阈值）。 <!-- v:manual:benchmark 人工运行 -->

---

## Golden 文件内容规格

### `basic_ident.ms`

```ms
x
hello_world
_priv
return
true
false
nil
```

### `basic_ident.expected`

```
  1:1   IDENT               x
  1:2   NEWLINE             <newline>
  2:1   IDENT               hello_world
  2:12  NEWLINE             <newline>
  3:1   IDENT               _priv
  3:6   NEWLINE             <newline>
  4:1   RETURN              return
  4:7   NEWLINE             <newline>
  5:1   TRUE                true
  5:5   NEWLINE             <newline>
  6:1   FALSE               false
  6:6   NEWLINE             <newline>
  7:1   NIL                 nil
  8:1   EOF                 <eof>
```

### `int_literals.ms`

```ms
0
42
0xFF
0b1010
0o17
1_000_000
9223372036854775807
```

### `int_literals.expected`

```
  1:1   INT                  0
  1:2   NEWLINE              <newline>
  2:1   INT                  42
  2:3   NEWLINE              <newline>
  3:1   INT                  255
  3:5   NEWLINE              <newline>
  4:1   INT                  10
  4:7   NEWLINE              <newline>
  5:1   INT                  15
  5:5   NEWLINE              <newline>
  6:1   INT                  1000000
  6:10  NEWLINE              <newline>
  7:1   INT                  9223372036854775807
  8:1   EOF                  <eof>
```

说明：
- 整数 repr 统一输出为十进制值（`"%" PRId64`），不保留原始前缀/分隔符。
- `0xFF` → `255`（十进制），`0b1010` → `10`，`0o17` → `15`，`1_000_000` → `1000000`。

### `errors.ms`

```ms
@
$
```

### `errors.expected`

```
  1:1   ERROR                <error: unexpected character '@'>
  1:2   NEWLINE              <newline>
  2:1   ERROR                <error: unexpected character '$'>
  3:1   EOF                  <eof>
```

说明：
- 错误行继续扫描（不中断），最终 `cliRunTokens` 返回非零。
- `<error: …>` 的具体消息来自 `lex.errBuf`（`ms_lexer.h` 中 `hasError`/`errBuf` 字段）。

### `fstring.ms`

```ms
$"hello {x} world"
```

### `fstring.expected`

```
  1:1   FSTRING_START        $"hello 
  1:9   FSTRING_EXPR_START   {
  1:10  IDENT                x
  1:11  FSTRING_EXPR_END     }
  1:12  FSTRING_END          " world"
  2:1   EOF                  <eof>
```

说明：f-string 拆为 5 类子 token，每段原始字节含引号（`FSTRING_START` 含开头 `$"` 及文本，
`FSTRING_END` 含结尾 `"`）；具体分段视 f-string 状态机实现（T011）而定，实现者应以实际
`msLexerNext` 输出为准并更新此 golden。

### `asi.ms`

```ms
x
+
y
```

### `asi.expected`

```
  1:1   IDENT               x
  1:2   NEWLINE             <newline>
  2:1   PLUS                +
  3:1   IDENT               y
  4:1   EOF                 <eof>
```

（`+` 后换行不触发 ASI，因为 `MS_TOK_PLUS` 不在 `asiShouldInsert` 触发列表中。）

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_token_print.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>
#include <stdio.h>

static void testTokenPrintIdent(void) {
  // 测试 msTokenPrint 输出格式（写入 char buf）
  const char* src = "hello";
  struct MsLexer lex;
  msLexerInit(&lex, src, 5, "<t>");
  struct MsToken t = msLexerNext(&lex);

  char buf[128];
  FILE* fp = fmemopen(buf, sizeof(buf), "w");
  msTokenPrint(&t, src, fp);
  fclose(fp);
  // 期望：包含 "IDENT" 和 "hello"
  MS_ASSERT_TRUE(strstr(buf, "IDENT") != NULL, "has IDENT");
  MS_ASSERT_TRUE(strstr(buf, "hello") != NULL, "has hello");
}

int main(void) {
  MS_RUN(testTokenPrintIdent);
  return msTestSummary();
}
```

（注：`fmemopen` 在 Windows 需要 `_fmemopen` 或临时文件替代；测试文件可用 `#ifdef _WIN32` 条件编译跳过此测试，待 T016 确认跨平台方案。）

---

## .ms 使用示例

```sh
# 打印 hello.ms 的 token 流
mslang tokens hello.ms

# 管道查看特定 token
mslang tokens program.ms | grep FSTRING

# 词法错误检测
mslang tokens broken.ms; echo "Exit: $?"
```

---

## Benchmark

### C microbench（`benchmarks/bench_lexer.c`）

基准文件 `benchmarks/data/bench_1k.ms` 由 T016 实现者手写，应包含：
- 各类字面量（整数/浮点/字符串/bytes/f-string）
- 各类运算符与括号
- 函数/类/控制流关键字
- 典型表达式（`a + b * c - d / e`）
- 注释行

目标指标（Release `-O2`）：

| 指标 | 目标值 |
|---|---|
| tokens/sec | > 50M |
| 内存（峰值） | < 1MB/次扫描（词法器无堆分配） |

---

## 风险与边界

- **Windows 跨平台 `fmemopen`**：MSVC 无 `fmemopen`，C 单测中涉及输出格式验证时，改用临时文件或字符串缓冲写法（`sprintf` + 断言）。Golden runner 基于 CLI 子命令，不受此影响。
- **列号对齐宽度**：超过 999 行或 9999 列的源文件会破坏固定宽度对齐（格式串 `"%3u:%-4u"`）；初版不处理（实际 `.ms` 文件极少超过此范围）。
- **Unicode 标识符**：含多字节 UTF-8 字符的标识符，列号按字节计（非字符计），与 `syntax.md §1.4` 注脚一致。
- **benchmark 数据文件不入 git**：`benchmarks/data/` 加入 `.gitignore`，由 T016 实现者生成（或在 README 中提供生成脚本）。benchmark 内存指标（`< 1MB`）无自动化手段，标注为 `<!-- v:manual:... -->`，需人工 ASAN 复核。
- **`msTokenPrint` 与 `msTokName` 分工**：`msTokName()`（`ms_lexer.h` 第 135 行）面向错误诊断，对关键字返回小写、运算符返回符号，与 `tokens` 子命令期望的全大写 KIND 列语义不同。`msTokenPrint` 内部应使用独立映射表（枚举名去 `MS_TOK_` 前缀），不能直接调用 `msTokName()`。
- **golden runner 子命令扩展**：`tests/golden_runner.py` 的 `--cmd` 参数已支持 `nargs="+"`，但 `ms_add_golden_test` CMake 包装仅传单个 `$<TARGET_FILE:${cmd}>`。T016 需新增 `ms_add_tokens_test` 宏（或修改现有包装），将 `tokens` 子命令注入 runner。
