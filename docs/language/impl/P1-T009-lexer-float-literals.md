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
| P1-T008 | 整数扫描（共用 `lexParseDigits` 辅助函数） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.7 浮点字面量（BNF） |
| `syntax.md` | §1.10 运算符与界符（`.` 消歧依据） |
| `type-system.md` | §2.2 float（float64，IEEE 754） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 添加 lexScanFloat() 内部函数；
                       # 移除 lexScanNumber 中两处 "float literals not yet supported"
                       # 占位 MS_TOK_ERROR 分支；
                       # 修改 lexScan 中 '.' 起始分支（'.' 后紧跟数字时转入浮点）
```

### 内部函数

```c
// 从当前 lex->pos（已扫描整数部分或 '.'）开始扫描浮点
// 调用 strtod 完成实际转换，存入 val.fval
static struct MsToken lexScanFloat(struct MsLexer* lex, uint32_t start, struct MsSrcPos pos);
```

---

## 实现要点

1. **与整数的分工**：`msLexerNext` 在遇到数字字节时先调用 `lexScanNumber`；若扫描整数部分后发现：
   - 下一字节为 `.`（且 `.` 后不是第二个 `.`）→ 转为浮点，继续扫描小数部分。
   - 下一字节为 `e`/`E` → 转为浮点，继续扫描指数部分。
   - 否则返回 `MS_TOK_INT`。

   以 `.` 开头（token 起始字节为 `.`）：若 `.` 后紧跟数字，则为浮点字面量；`...` 为分隔符
   token（`MS_TOK_DOTDOTDOT`）；连续两个 `.` 按两个 `MS_TOK_DOT` 处理（`syntax.md §1.10`
   界符表中无 `..` token，import 相对路径 `..mod` 即两个 `MS_TOK_DOT`）。
2. **`_` 与浮点互斥**：浮点字面量中的 `_` 不在 `syntax.md §1.7` 的 BNF 中，故**不支持**
   （与整数字面量不同）。`lexScanNumber` 须记录整数部分是否出现过 `_`；若出现且后续命中
   `.`/`e`/`E`，返回 `MS_TOK_ERROR`（消息 `'_' is not allowed in float literals`），避免
   `strtod("1_000.0")` 在 `_` 处停止、静默返回 1.0 的错值陷阱。
   （注：§1.7 中 `decimal_digits` 为 BNF 悬空非终结符，本任务按 `digit { digit }` 解释，
   不含 `_`；如设计组确认浮点支持 `_` 需先修订 syntax.md。）
3. **转换**：词法器先按 BNF 扫描确定 token 终点，再将该字节段拷贝到栈上定长缓冲
   （超长则 `MS_TOK_ERROR`）并补 `\0`，然后调用 `strtod(buf, &end)` 完成 string→double
   转换（最精确，处理所有边界情况）；须校验 `end` 等于缓冲末尾，不一致即 `MS_TOK_ERROR`。
   不得对源缓冲直接调用 `strtod`——`msLexerInit` 未承诺 `src` 以 `\0` 结尾，直接调用存在
   越界读取风险。指数部分至少一位数字（`exponent = ('e'|'E') ('+'|'-')? decimal_digits`），
   `1e`/`1e+`/`1.5e-` 均为 `MS_TOK_ERROR`。
4. **`NaN`/`Inf`**：字面量不直接产生这些值（通过 `math.nan`/`math.inf` 访问，
   `type-system.md §2.2`）；`1e999` 解析为 `+inf`（`strtod` 行为），不报错。忽略 `strtod`
   设置的 `ERANGE`（`c-style.md §8.4` 禁止以 errno 作错误传递机制），按返回值
   （±inf/0.0）存入 token。

---

## 验收标准（checklist）

- [ ] `3.14` → `MS_TOK_FLOAT, fval≈3.14`。
- [ ] `1.` → `MS_TOK_FLOAT, fval=1.0`。
- [ ] `.5` → `MS_TOK_FLOAT, fval=0.5`。
- [ ] `1e10` → `MS_TOK_FLOAT, fval=1e10`。
- [ ] `2e+5` → `MS_TOK_FLOAT, fval=2e5`。
- [ ] `1.5E-3` → `MS_TOK_FLOAT, fval=0.0015`。
- [ ] `0.0` → `MS_TOK_FLOAT, fval=0.0`。
- [ ] `1e999` → `MS_TOK_FLOAT, fval=+inf`（不报词法错误）。
- [ ] `1_000.0` → `MS_TOK_ERROR`（`_` 在浮点字面量中不合法，见实现要点 2）。
- [ ] `1e` → `MS_TOK_ERROR`（指数缺数字）。
- [ ] `1e+` → `MS_TOK_ERROR`（指数缺数字）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_float_literals.c`）

```c
#include <math.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

static void testBasicFloat(void) {
  struct { const char* src; double expected; } cases[] = {
    {"3.14",  3.14},  {"1.",   1.0},  {".5",   0.5},
    {"1e10",  1e10},  {"2e+5", 2e5},  {"1.5E-3", 1.5e-3},
    {"0.0",   0.0},   {"1.0",  1.0},
  };
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind, MS_TOK_FLOAT, cases[i].src);
    // 用 fabs 比较，允许极小误差
    MS_ASSERT_TRUE(fabs(t.val.fval - cases[i].expected) < 1e-9, cases[i].src);
  }
}

static void testInfFromLargeExponent(void) {
  struct MsLexer lex;
  msLexerInit(&lex, "1e999", 5, "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FLOAT, "1e999 is float");
  MS_ASSERT_TRUE(isinf(t.val.fval), "1e999 is inf");
}

static void testInvalidFloat(void) {
  const char* cases[] = {"1e", "1e+", "1.5e-", "1_000.0"};
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i], (uint32_t)strlen(cases[i]), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, cases[i]);
  }
}

int main(void) {
  MS_RUN(testBasicFloat);
  MS_RUN(testInfFromLargeExponent);
  MS_RUN(testInvalidFloat);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
import math

piVal := 3.14
bigNum := 1e10
half := .5
tiny := 1.5E-3
print(piVal, bigNum, half, tiny)
// 输出 4 个浮点值（具体格式以 stdlib str(float) 设计为准）

print(math.inf, math.nan)
// inf nan
```

---

## Benchmark

N/A（浮点扫描归入 T016 词法整体 bench）。

---

## 风险与边界

- **`.` 消歧**：`a.b` 中的 `.` 是属性访问符（`MS_TOK_DOT`），不是浮点字面量起始。只有**在表达式上下文中、`.` 后紧跟数字**才是浮点字面量；但词法器本身无法知道上下文，因此需要一个规则：**以 `.` 开头时，仅当 `.` 不在标识符/右括号之后**才考虑浮点——但这是语法层判断。实际上词法器可以简单地：`msLexerNext` 在见到 `.` 时，若下一字节为数字，则产生浮点；否则产生 `MS_TOK_DOT`（或 `MS_TOK_DOTDOTDOT`）。
- **`strtod` locale**：某些 locale 将 `,` 作为小数点。C 程序默认 locale 即 `"C"`，本任务单测不经过 VM 初始化，无需额外处理。长期风险：嵌入场景下宿主程序已切换 `LC_NUMERIC` 时 `strtod` 行为偏差，建议后续以 `strtod_l` 或自实现转换解决；**不得**在 `msNew()` 中调用 `setlocale`——其修改进程全局状态、会篡改宿主 locale 且非线程安全。
