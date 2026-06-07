# P2-T018 Pratt 表达式解析框架（优先级表）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

搭建 Pratt（Top-Down Operator Precedence）解析框架：定义运算符优先级表、前缀/中缀解析函数表，以及核心函数 `parsePrecedence(parser, minPrec)`。T019–T025 将在此框架上注册具体运算符的解析器。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T017 | AST 节点类型与 arena |
| P1-T006 ~ T016 | 全部词法器 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3 表达式（完整优先级表） |
| `syntax.md` | §1.3 ASI（`TOK_NEWLINE` 作为语句分隔符） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_parser.h   # 公共 parser API
src/parser/ms_parser.c       # MsParser 结构体 + Pratt 框架主体
src/parser/ms_parse_expr.c   # 表达式级 parse 函数（T018：框架，T019–T025：具体运算符）
```

---

## 实现要点

### 1. MsParser 结构体

```c
// include/mslang/ms_parser.h
typedef struct MsParser {
  MsLexer   lex;          // 嵌入词法器（非指针，owns the lexer）
  MsToken   cur;          // 当前 token
  MsToken   prev;         // 上一个 token
  MsArena*  arena;        // AST 内存（由调用方传入）
  bool      hadError;     // 是否发生解析错误
  bool      panicMode;    // 错误恢复状态（抑制后续错误）
  char      errBuf[256];  // 最近一条错误消息
} MsParser;
```

### 2. 优先级枚举

```c
typedef enum Precedence {
  PREC_NONE      = 0,   // 无结合性（语句层）
  PREC_ASSIGN    = 1,   // = += -= …（右结合，但在 Pratt 框架内作为语句处理）
  PREC_OR        = 2,   // or
  PREC_AND       = 3,   // and
  PREC_NOT       = 4,   // not（一元，前缀）
  PREC_COMPARE   = 5,   // == != < > <= >= is in not-in is-not
  PREC_BITOR     = 6,   // |
  PREC_BITXOR    = 7,   // ^
  PREC_BITAND    = 8,   // &
  PREC_SHIFT     = 9,   // << >>
  PREC_TERM      = 10,  // + -
  PREC_FACTOR    = 11,  // * / // %
  PREC_UNARY     = 12,  // - ~ +（一元，前缀）
  PREC_POWER     = 13,  // **（右结合）
  PREC_CALL      = 14,  // () [] .（后缀）
  PREC_PRIMARY   = 15,  // 字面量/标识符
} Precedence;
```

### 3. Pratt 解析表

```c
typedef MsNode* (*PrefixFn)(MsParser*);
typedef MsNode* (*InfixFn) (MsParser*, MsNode* left);

struct ParseRule {
  PrefixFn   prefix;   // 前缀（以此 token 开头的表达式）
  InfixFn    infix;    // 中缀（此 token 出现在左侧操作数之后）
  Precedence prec;     // 中缀绑定优先级
};

// 全局表，索引为 MsTokKind
extern struct ParseRule gParseRules[TOK_COUNT];
```

规则注册由各子任务在文件顶层调用 `parserRegisterRule(kind, prefix, infix, prec)` 完成（或直接在 `gParseRules` 初始化列表中填充）。

### 4. 核心函数

```c
// 解析优先级 >= minPrec 的表达式
MsNode* parsePrecedence(MsParser* p, Precedence minPrec);

// parser 公开 API
void     msParserInit(MsParser* p, const char* src, uint32_t srcLen,
                      const char* fileName, MsArena* arena);
MsNode*  msParseExpr(MsParser* p);
MsNode*  msParseStmt(MsParser* p);
MsNode*  msParseProgram(MsParser* p);

// 内部辅助
static MsToken advance(MsParser* p);
static bool    check(MsParser* p, MsTokKind kind);
static bool    match(MsParser* p, MsTokKind kind);  // 若 check 则 advance + return true
static void    expect(MsParser* p, MsTokKind kind, const char* msg);
static void    syncError(MsParser* p);              // 错误恢复：跳到下一分号/换行
```

### 5. `parsePrecedence` 实现骨架

```c
MsNode* parsePrecedence(MsParser* p, Precedence minPrec) {
  // 前缀
  advance(p);
  struct ParseRule* rule = &gParseRules[p->prev.kind];
  if (rule->prefix == NULL) {
    parserError(p, "expected expression");
    return NULL;
  }
  MsNode* left = rule->prefix(p);

  // 中缀（循环）
  while (!p->hadError) {
    struct ParseRule* cur = &gParseRules[p->cur.kind];
    if (cur->prec < minPrec) break;
    advance(p);
    left = cur->infix(p, left);
  }
  return left;
}

MsNode* msParseExpr(MsParser* p) {
  return parsePrecedence(p, PREC_ASSIGN + 1);  // 赋值不走 Pratt，单独处理
}
```

### 6. 错误恢复

使用 **panic mode** 模式：发生第一个语法错误后设 `panicMode = true`，抑制后续错误打印；调用 `syncError` 跳到安全同步点（下一个 `TOK_NEWLINE`/`TOK_SEMICOLON`/`TOK_EOF` 或块关键字 `if`/`func`/`class` 等）。

---

## 验收标准（checklist）

- [ ] `msParserInit` + `msParseExpr("42")` 返回 `ND_INT` 节点（仅字面量，T019 未接入时需先注册 `TOK_INT` 的 prefix）。
- [ ] `parsePrecedence` 在空输入（只有 `TOK_EOF`）时返回 `NULL` + 设 `hadError`。
- [ ] `gParseRules` 表大小恰好为 `TOK_COUNT`（编译期 `static_assert`）。
- [ ] `match(p, TOK_NEWLINE)` 与 `match(p, TOK_SEMICOLON)` 都作语句分隔符处理（等价）。
- [ ] panic mode 恢复后能正确解析后续语句（错误不会级联）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_pratt_framework.c`）

```c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "ms_arena.h"

static MsNode* parseExprStr(MsArena* arena, const char* src) {
  MsParser p;
  msParserInit(&p, src, (uint32_t)strlen(src), "<t>", arena);
  return msParseExpr(&p);
}

static void testEmptyExpr(void) {
  MsArena arena; msArenaInit(&arena);
  MsParser p;
  msParserInit(&p, "", 0, "<t>", &arena);
  MsNode* n = msParseExpr(&p);
  MS_ASSERT_TRUE(p.hadError || n == NULL, "empty input => error or null");
  msArenaFree(&arena);
}

int main(void) {
  MS_RUN(testEmptyExpr);
  return msTestSummary();
}
```

（完整的表达式解析测试在 T019 提供，T018 只验证框架本身）

---

## .ms 使用示例

N/A（parser 框架通过 `mslang parse` 子命令（T036）验证）。

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`TOK_NEWLINE` 与 Pratt**：中缀循环遇 `TOK_NEWLINE` 应停止（不进入 infix），因为 NEWLINE 是语句分隔符。`gParseRules[TOK_NEWLINE].prec = PREC_NONE`，自然中断。
- **赋值右结合**：赋值 `=`/`:=`/`+=`/… 不走 Pratt infix，而在 `msParseStmt` 中显式处理（以左侧表达式为目标，检查是否跟赋值运算符，再解析右侧）。
- **逗号优先级**：逗号在函数调用参数列表和 tuple 中有不同语义；不注册为 Pratt infix，由调用/tuple 专用函数（T021/T023）手动处理。
