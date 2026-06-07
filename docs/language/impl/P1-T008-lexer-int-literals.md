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

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.6 整数字面量（BNF + 值域说明） |
| `type-system.md` | §2.1 int（int64，溢出回绕） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 添加 scanInt() 内部函数
```

### 内部函数

```c
// 当前字节为数字时调用（已读入当前 lex->pos）
// 返回 TOK_INT，val.ival 已填充
// 若后跟 '.'/e/E 则本函数不负责，caller 转交 scanFloat（T009）
static MsToken scanNumber(MsLexer* lex);

// 解析无符号 64 位数字（带进位与溢出回绕）
static uint64_t parseDigits(MsLexer* lex, int base);
```

---

## 实现要点

1. **`0` 起始的前缀**：
   - `0x` / `0X` → 十六进制，有效数字 `[0-9a-fA-F_]`。
   - `0o` / `0O` → 八进制，有效数字 `[0-7_]`。
   - `0b` / `0B` → 二进制，有效数字 `[01_]`。
   - `0` 单独或 `0` 后跟数字 → 十进制（mslang 无 C 风格 `0777` 八进制，`syntax.md §1.6`）。
   - `0` 后跟 `.` / `e` / `E` → 浮点（转交 T009）。
2. **`_` 视觉分隔**：`_` 在数字之间跳过，`_` 不能作为首字符或末字符（`1_000_000` 合法，`_1` 为标识符，`1_` 为词法错误）。
3. **溢出语义**：`uint64_t` 累加，溢出时自然回绕（C 无符号运算）；最终存入 `int64_t val.ival`（位模式不变，即 `(int64_t)(uint64_t_value)`）。
4. **与浮点消歧**：`scanNumber` 在读完整数部分后，若后一字节为 `.`（且 `.` 后不是 `.`，即不是 `...`）或 `e`/`E`，则将已读内容退回，交给 `scanFloat`；或由 caller（`msLexNext`）在调用 `scanNumber` 前先前瞻决定路径。

---

## 验收标准（checklist）

- [ ] `42` → `TOK_INT, ival=42`。
- [ ] `0` → `TOK_INT, ival=0`。
- [ ] `0xFF` → `TOK_INT, ival=255`。
- [ ] `0o77` → `TOK_INT, ival=63`。
- [ ] `0b1010` → `TOK_INT, ival=10`。
- [ ] `1_000_000` → `TOK_INT, ival=1000000`。
- [ ] `9_223_372_036_854_775_807` → `INT64_MAX`（`int64_t` 最大值）。
- [ ] `9_223_372_036_854_775_808` → `-9223372036854775808`（溢出回绕，INT64_MIN 的位模式）。
- [ ] `1_` → `TOK_ERROR`（尾部 `_` 不合法）。
- [ ] `0x` → `TOK_ERROR`（前缀后无有效数字）。
- [ ] `0b2` → `TOK_INT(0)`（`2` 不是二进制数字，`0b0` 扫完后 `2` 为下一 token）或 `TOK_ERROR`（实现可选，以 C 编译器常见行为为准）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_int_literals.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"

struct IntCase { const char* src; int64_t expected; };

static void testDecimal(void) {
  struct IntCase cases[] = {
    {"0",   0}, {"42", 42}, {"1_000", 1000},
    {"9_223_372_036_854_775_807", INT64_MAX},
  };
  for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
    MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    MsToken t = msLexNext(&lex);
    MS_ASSERT_EQ(t.kind,     TOK_INT,           cases[i].src);
    MS_ASSERT_EQ(t.val.ival, cases[i].expected, cases[i].src);
  }
}

static void testHexOctBin(void) {
  struct { const char* src; int64_t v; } cases[] = {
    {"0xFF",   255}, {"0XFF",   255},
    {"0o77",   63},  {"0O77",   63},
    {"0b1010", 10},  {"0B1010", 10},
    {"0x1_0",  16},
  };
  for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
    MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    MsToken t = msLexNext(&lex);
    MS_ASSERT_EQ(t.kind,     TOK_INT,    cases[i].src);
    MS_ASSERT_EQ(t.val.ival, cases[i].v, cases[i].src);
  }
}

static void testOverflow(void) {
  // 9223372036854775808 = INT64_MAX + 1，位模式 = INT64_MIN
  const char* src = "9223372036854775808";
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  MsToken t = msLexNext(&lex);
  MS_ASSERT_EQ(t.kind, TOK_INT, "overflow is TOK_INT");
  MS_ASSERT_EQ(t.val.ival, INT64_MIN, "wraps to INT64_MIN");
}

int main(void) {
  MS_RUN(testDecimal);
  MS_RUN(testHexOctBin);
  MS_RUN(testOverflow);
  return msTestSummary();
}
```

### .ms 使用示例（T067 VM 可用后验证）

```ms
// 整数字面量使用示例
a := 42
b := 0xFF           // 255
c := 0o77           // 63
d := 0b1010         // 10
e := 1_000_000      // 1000000
f := 0x7FFF_FFFF_FFFF_FFFF  // INT64_MAX

print(a, b, c, d, e, f)
// 42 255 63 10 1000000 9223372036854775807
```

---

## Benchmark

N/A（整数扫描为词法整体性能的一部分，在 T016 的 bench_lexer 中统计）。

---

## 风险与边界

- **`0` 与浮点**：`0.5` 以 `0` 开头，`scanNumber` 读到 `0` 后发现下一字节为 `.`（且不是 `..`），转为浮点扫描（T009）。
- **`0x1p4` 科学计数**：mslang 不支持十六进制浮点（`syntax.md §1.7` 未提及），可以将 `p` 视为终止字符。
- **`_` 不能单独出现**：`1__2` 在两个 `_` 间合法（跳过两次），但 `_2` 作为标识符，不是整数。
