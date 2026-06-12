# P1-T008 整数字面量（dec/hex/oct/bin/`_`分隔）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现整数字面量的完整扫描与解析：十进制、十六进制（`0x`/`0X`）、八进制（`0o`/`0O`）、二进制（`0b`/`0B`），以及可视分隔符 `_`。解析结果存入 `MsToken.val.ival`（int64_t），溢出时回绕（符合 C 标准无符号语义）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | Lexer 框架 |
| P1-T007 | 标识符/关键字扫描（保证 `_` 开头走标识符路径） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.6 整数字面量（BNF + 值域说明） |
| `syntax.md` | §1.7 浮点字面量（消歧依据） |
| `type-system.md` | §2.1 int（int64，溢出回绕） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 添加 lexScanNumber() 内部函数
```

### 内部函数

```c
// 当前字节为数字时由 lexScan 调用（首字节已确认为 [0-9]）
// 返回 MS_TOK_INT，val.ival 已填充
// 读完整数部分后前瞻 '.'/e/E 命中则按浮点继续（T009 实现）
static struct MsToken lexScanNumber(struct MsLexer* lex);

// 解析无符号 64 位数字（带进位与溢出回绕）
static uint64_t lexParseDigits(struct MsLexer* lex, int base);
```

---

## 实现要点

1. **`0` 起始的前缀**：
   - `0x` / `0X` → 十六进制，有效数字 `[0-9a-fA-F]`（`_` 仅可夹在数字之间）。
   - `0o` / `0O` → 八进制，有效数字 `[0-7]`（同上）。
   - `0b` / `0B` → 二进制，有效数字 `[01]`（同上）。
   - `0` 单独 → 十进制 0；`0` 后直接跟数字 → **词法错误**（`MS_TOK_ERROR`，
     消息 "leading zeros in decimal literal are not allowed"——
     `decimal_lit = '0' | ('1'..'9') { '_'? digit }`，`syntax.md §1.6` 不接受前导零）。
   - `0` 后跟 `.` / `e` / `E` → 浮点（转交 T009）。
   - `_` 不能紧跟进制前缀（`0x_FF` 非法，BNF 要求前缀后第一个字符为有效数字）。
2. **`_` 视觉分隔**：`_` 仅可夹在两个数字之间且**不可连续**（BNF `'_'? digit` 每个数字前至多一个 `_`）：`1_000_000` 合法；`_1` 为标识符；`1_`、`1__2` 为词法错误。实现时记录"上一字符是否为 `_`"以拒绝连续下划线。
3. **溢出语义**：`uint64_t` 累加，溢出时自然回绕（C 无符号运算）；最终存入 `int64_t val.ival`（位模式不变，即 `(int64_t)(uint64_t_value)`）。注：该转换在 C17 为实现定义行为，本项目依赖二补码平台（CI 全部满足）；如需严格可移植可用 `memcpy` 转换。
4. **与浮点消歧**：由 `lexScan` 在首字节为数字时调用 `lexScanNumber`；`lexScanNumber` 读完整数部分后前瞻 `.`（且后一字节不是 `.`，即不是 `...`）或 `e`/`E`，命中则继续按浮点扫描（T009 实现，本任务预留分支并先返回 `MS_TOK_ERROR` 占位）。

---

## 验收标准（checklist）

- [ ] `42` → `MS_TOK_INT, ival=42`。
- [ ] `0` → `MS_TOK_INT, ival=0`。
- [ ] `0xFF` → `MS_TOK_INT, ival=255`。
- [ ] `0o77` → `MS_TOK_INT, ival=63`。
- [ ] `0b1010` → `MS_TOK_INT, ival=10`。
- [ ] `1_000_000` → `MS_TOK_INT, ival=1000000`。
- [ ] `9_223_372_036_854_775_807` → `INT64_MAX`（`int64_t` 最大值）。
- [ ] `9_223_372_036_854_775_808` → `-9223372036854775808`（溢出回绕，INT64_MIN 的位模式）。
- [ ] `1_` → `MS_TOK_ERROR`（尾部 `_` 不合法）。
- [ ] `1__2` → `MS_TOK_ERROR`（连续 `_` 不合法，BNF `'_'? digit`）。
- [ ] `0777` → `MS_TOK_ERROR`（十进制不允许前导零，`syntax.md §1.6`）。
- [ ] `0x` → `MS_TOK_ERROR`（前缀后无有效数字）。
- [ ] `0x_FF` → `MS_TOK_ERROR`（`_` 不能紧跟进制前缀）。
- [ ] `0b2` → `MS_TOK_ERROR`（前缀后无有效数字，与 `0x` 同规则）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_int_literals.c`）

```c
#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

struct IntCase { const char* src; int64_t expected; };

static void testDecimal(void) {
  struct IntCase cases[] = {
    {"0",   0}, {"42", 42}, {"1_000", 1000},
    {"9_223_372_036_854_775_807", INT64_MAX},
  };
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind,     MS_TOK_INT,        cases[i].src);
    MS_ASSERT_EQ(t.val.ival, cases[i].expected, cases[i].src);
  }
}

static void testHexOctBin(void) {
  struct IntCase cases[] = {
    {"0xFF",   255}, {"0XFF",   255},
    {"0o77",   63},  {"0O77",   63},
    {"0b1010", 10},  {"0B1010", 10},
    {"0x1_0",  16},
  };
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind,     MS_TOK_INT,        cases[i].src);
    MS_ASSERT_EQ(t.val.ival, cases[i].expected, cases[i].src);
  }
}

static void testOverflow(void) {
  // 9223372036854775808 = INT64_MAX + 1，位模式 = INT64_MIN
  const char* src = "9223372036854775808";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_INT, "overflow is MS_TOK_INT");
  MS_ASSERT_EQ(t.val.ival, INT64_MIN, "wraps to INT64_MIN");
}

static void testInvalidLiterals(void) {
  const char* cases[] = {"1_", "1__2", "0777", "0x", "0x_FF", "0b2"};
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i], (uint32_t)strlen(cases[i]), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, cases[i]);
  }
}

int main(void) {
  MS_RUN(testDecimal);
  MS_RUN(testHexOctBin);
  MS_RUN(testOverflow);
  MS_RUN(testInvalidLiterals);
  return msTestSummary();
}
```

### .ms 使用示例（T067 VM 可用后验证）

```ms
// 整数字面量使用示例
answer := 42
mask := 0xFF            // 255
perm := 0o77            // 63
flags := 0b1010         // 10
million := 1_000_000    // 1000000
maxInt := 0x7FFF_FFFF_FFFF_FFFF  // INT64_MAX

print(answer, mask, perm, flags, million, maxInt)
// 42 255 63 10 1000000 9223372036854775807
```

---

## Benchmark

N/A（整数扫描为词法整体性能的一部分，在 T016 的 bench_lexer 中统计）。

---

## 风险与边界

- **`0` 与浮点**：`0.5` 以 `0` 开头，`lexScanNumber` 读到 `0` 后发现下一字节为 `.`（且不是 `..`），转为浮点扫描（T009）。
- **`0x1p4` 科学计数**：mslang 不支持十六进制浮点（`syntax.md §1.7` 未提及），可以将 `p` 视为终止字符。
- **`_` 不能单独/连续出现**：`1__2` 非法（BNF `'_'? digit` 中 `_` 在数字间至多一次），产出 `MS_TOK_ERROR`；`_2` 作为标识符，不是整数。
