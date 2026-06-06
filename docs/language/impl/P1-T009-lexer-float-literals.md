# P1-T009 浮点字面量（IEEE 754，指数形式）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现浮点字面量的扫描与解析。浮点字面量有三种形式（`syntax.md §1.7`）：`digits.digits?exp?`、`digits exp`、`.digits exp?`。解析后存入 `MsToken.val.fval`（double）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | Lexer 框架 |
| P1-T008 | 整数扫描（共用 `parseDigits` 辅助函数） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.7 浮点字面量（BNF） |
| `type-system.md` | §2.2 float（float64，IEEE 754） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 添加 scanFloat() 内部函数
```

### 内部函数

```c
// 从当前 lex->pos（已扫描整数部分或 '.'）开始扫描浮点
// 调用 strtod 完成实际转换，存入 val.fval
static MsToken scanFloat(MsLexer* lex, uint32_t numStart);
```

---

## 实现要点

1. **与整数的分工**：`msLexNext` 在遇到数字字节时先调用 `scanNumber`；若扫描整数部分后发现：
   - 下一字节为 `.`（且不是 `..`）→ 转为浮点，继续扫描小数部分。
   - 下一字节为 `e`/`E` → 转为浮点，继续扫描指数部分。
   - 否则返回 `TOK_INT`。
   
   以 `.` 开头（`token` 起始字节为 `.`）：若 `.` 后紧跟数字，则为浮点字面量；`..` 或 `...` 为界符。
2. **转换**：扫描得到原始字节段后，使用 `strtod(start, &end)` 完成 string→double 转换（最精确，处理所有边界情况）。浮点字面量中的 `_` 不在 `syntax.md §1.7` 的 BNF 中，故**不支持**（与整数字面量不同）。
3. **`NaN`/`Inf`**：字面量不直接产生这些值（通过 `math.nan`/`math.inf` 访问，`type-system.md §2.2`）；`1e999` 解析为 `+inf`（`strtod` 行为），不报错。

---

## 验收标准（checklist）

- [ ] `3.14` → `TOK_FLOAT, fval≈3.14`。
- [ ] `1.` → `TOK_FLOAT, fval=1.0`。
- [ ] `.5` → `TOK_FLOAT, fval=0.5`。
- [ ] `1e10` → `TOK_FLOAT, fval=1e10`。
- [ ] `1.5E-3` → `TOK_FLOAT, fval=0.0015`。
- [ ] `0.0` → `TOK_FLOAT, fval=0.0`。
- [ ] `1e999` → `TOK_FLOAT, fval=+inf`（不报词法错误）。
- [ ] `1_000.0` → 词法错误（`_` 在浮点中不合法）或 `TOK_INT(1000)` 后跟 `.0`（取决于实现，文档明确选其一）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_float_literals.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <math.h>

static void testBasicFloat(void) {
    struct { const char* src; double v; } cases[] = {
        {"3.14",  3.14},  {"1.",   1.0},  {".5",   0.5},
        {"1e10",  1e10},  {"1.5E-3", 1.5e-3},
        {"0.0",   0.0},   {"1.0",  1.0},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        MsLexer lex;
        msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
        MsToken t = msLexNext(&lex);
        MS_ASSERT_EQ(t.kind, TOK_FLOAT, cases[i].src);
        // 用 fabs 比较，允许极小误差
        MS_ASSERT_TRUE(fabs(t.val.fval - cases[i].v) < 1e-9, cases[i].src);
    }
}

static void testInfFromLargeExponent(void) {
    MsLexer lex;
    msLexerInit(&lex, "1e999", 5, "<t>");
    MsToken t = msLexNext(&lex);
    MS_ASSERT_EQ(t.kind, TOK_FLOAT, "1e999 is float");
    MS_ASSERT_TRUE(isinf(t.val.fval), "1e999 is inf");
}

int main(void) {
    MS_RUN(testBasicFloat);
    MS_RUN(testInfFromLargeExponent);
    return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
a := 3.14
b := 1e10
c := .5
d := 1.5E-3
print(a, b, c, d)
// 3.14 10000000000.0 0.5 0.0015

import math
print(math.inf, math.nan)
// inf nan
```

---

## Benchmark

N/A（浮点扫描归入 T016 词法整体 bench）。

---

## 风险与边界

- **`.` 消歧**：`a.b` 中的 `.` 是属性访问符（`TOK_DOT`），不是浮点字面量起始。只有**在表达式上下文中、`.` 后紧跟数字**才是浮点字面量；但词法器本身无法知道上下文，因此需要一个规则：**以 `.` 开头时，仅当 `.` 不在标识符/右括号之后**才考虑浮点——但这是语法层判断。实际上词法器可以简单地：`msLexNext` 在见到 `.` 时，若下一字节为数字，则产生浮点；否则产生 `TOK_DOT`（或 `...`）。
- **`strtod` locale**：某些 locale 将 `,` 作为小数点；需在初始化时确保 `locale = "C"`（`setlocale(LC_NUMERIC, "C")`，在 `msNew()` 时调用）。
