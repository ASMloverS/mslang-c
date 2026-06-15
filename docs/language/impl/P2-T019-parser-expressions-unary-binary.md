# P2-T019 一元 / 二元 / 幂 / 位 / 比较 / 逻辑运算符解析

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在 T018 Pratt 框架上注册全部一元与二元运算符的前缀/中缀解析函数，覆盖算术、位运算、比较、逻辑与幂次，同时处理 mslang 特有运算符（`**`、`not`、`and`/`or`、`is [not]`、`[not] in`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架与 `ParseRule` 表 |
| P2-T017 | `MS_ND_UNARY`/`MS_ND_BINARY` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3 表达式优先级表（含结合性） |
| `syntax.md` | §2.3.1 一元运算（`-`, `~`, `not`） |
| `syntax.md` | §2.3.2 二元算术（`+ - * / % **`） |
| `syntax.md` | §2.3.3 位运算（`& | ^ << >>`） |
| `syntax.md` | §2.3.4 比较（`== != < > <= >= is is not in not in`） |
| `syntax.md` | §2.3.5 逻辑（`and or not`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册 prefix/infix 函数
include/mslang/ms_lexer.h    # 追加 MS_TOK_IS_NOT、MS_TOK_NOT_IN 枚举值
```

---

## 实现要点

### 1. 字面量前缀（注册到 `gParseRules`）

```c
// MS_TOK_INT → MS_ND_INT
static MsNode* parseIntLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = MS_ND_INT;
  n->pos          = p->prev.pos;
  n->litInt.ival = p->prev.val.ival;
  return n;
}
// 同理：parseFloatLit, parseStringLit, parseBytesLit, parseBoolLit(true/false), parseNilLit
// parseIdentLit → MS_ND_IDENT

// 注册：
// gParseRules[MS_TOK_INT]   = { parseIntLit,   NULL, PREC_NONE };
// gParseRules[MS_TOK_FLOAT] = { parseFloatLit, NULL, PREC_NONE };
// …
```

### 2. 一元运算（前缀）

```c
// -x  ~x  not x  +x
static MsNode* parseUnary(MsParser* p) {
  MsTokKind op  = p->prev.kind;
  MsSrcPos  pos = p->prev.pos;
  Precedence prec = (op == MS_TOK_NOT) ? PREC_NOT : PREC_POWER;
  MsNode* operand = parsePrecedence(p, prec);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = MS_ND_UNARY;
  n->pos          = pos;
  n->unary.op     = op;
  n->unary.operand = operand;
  return n;
}
// 注册：
// gParseRules[MS_TOK_MINUS] = { parseUnary, parseBinary, PREC_TERM };
// gParseRules[MS_TOK_TILDE] = { parseUnary, NULL,        PREC_NONE };
// gParseRules[MS_TOK_NOT]   = { parseUnary, parseIsIn,   PREC_COMPARE };  // unified: prefix + infix
// gParseRules[MS_TOK_PLUS]  = { parseUnary, parseBinary, PREC_TERM };
```

### 3. 二元运算（中缀）

```c
static MsNode* parseBinary(MsParser* p, MsNode* left) {
  MsTokKind  op  = p->prev.kind;
  MsSrcPos   pos = p->prev.pos;
  Precedence prec = gParseRules[op].prec;
  // 幂 ** 右结合：下一层 prec 不 +1
  bool rightAssoc = (op == MS_TOK_STARSTAR);
  MsNode* right = parsePrecedence(p, rightAssoc ? prec : prec + 1);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind          = MS_ND_BINARY;
  n->pos           = pos;
  n->binary.op     = op;
  n->binary.left   = left;
  n->binary.right  = right;
  return n;
}
// 注册（中缀部分）：
// gParseRules[MS_TOK_PLUS]     = { parseUnary,  parseBinary, PREC_TERM   };
// gParseRules[MS_TOK_MINUS]    = { parseUnary,  parseBinary, PREC_TERM   };
// gParseRules[MS_TOK_STAR]     = { NULL,         parseBinary, PREC_FACTOR };
// gParseRules[MS_TOK_SLASH]    = { NULL,         parseBinary, PREC_FACTOR };
// gParseRules[MS_TOK_PERCENT]  = { NULL,         parseBinary, PREC_FACTOR };
// gParseRules[MS_TOK_STARSTAR] = { NULL,         parseBinary, PREC_POWER  };（右结合）
// gParseRules[MS_TOK_SHL]      = { NULL,         parseBinary, PREC_SHIFT  };
// gParseRules[MS_TOK_SHR]      = { NULL,         parseBinary, PREC_SHIFT  };
// gParseRules[MS_TOK_AMP]      = { NULL,         parseBinary, PREC_BITAND };
// gParseRules[MS_TOK_PIPE]     = { NULL,         parseBinary, PREC_BITOR  };
// gParseRules[MS_TOK_CARET]    = { NULL,         parseBinary, PREC_BITXOR };
```

### 4. 比较运算符

比较运算符全部优先级 `PREC_COMPARE`，左结合。

```c
// gParseRules[MS_TOK_EQ]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[MS_TOK_NEQ] = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[MS_TOK_LT]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[MS_TOK_GT]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[MS_TOK_LE]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[MS_TOK_GE]  = { NULL, parseBinary, PREC_COMPARE };
```

### 5. `is [not]` / `[not] in`（混合关键字运算符）

```c
// 'is' 后可跟 'not'；'not' 后可跟 'in'——中缀函数中向前探测：
static MsNode* parseIsIn(MsParser* p, MsNode* left) {
  MsTokKind op  = p->prev.kind;  // MS_TOK_IS、MS_TOK_IN 或 MS_TOK_NOT
  MsSrcPos  pos = p->prev.pos;   // 保存运算符起点

  if (op == MS_TOK_IS && msParserMatch(p, MS_TOK_NOT)) {
    op = MS_TOK_IS_NOT;           // is not
  } else if (op == MS_TOK_NOT) {
    msParserExpect(p, MS_TOK_IN, "'in' expected after 'not'");
    op = MS_TOK_NOT_IN;           // not in
  }
  MsNode* right = parsePrecedence(p, PREC_COMPARE + 1);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = MS_ND_BINARY;
  n->pos          = pos;
  n->binary.op    = op;
  n->binary.left  = left;
  n->binary.right = right;
  return n;
}
// MS_TOK_IS_NOT 与 MS_TOK_NOT_IN：parser 内部虚拟 token，添加至 ms_lexer.h
// MsTokKind 枚举末尾；词法器不产生，仅 AST binary.op 中使用。
//
// 注册（MS_TOK_NOT 前缀与中缀在同一处赋值，见 §2）：
// gParseRules[MS_TOK_IS]  = { NULL,       parseIsIn, PREC_COMPARE };
// gParseRules[MS_TOK_IN]  = { NULL,       parseIsIn, PREC_COMPARE };
```

### 6. `and` / `or`（短路逻辑运算）

```c
// and/or 产生 MS_ND_BINARY，但编译器需要短路跳转
// gParseRules[MS_TOK_AND] = { NULL, parseBinary, PREC_AND };
// gParseRules[MS_TOK_OR]  = { NULL, parseBinary, PREC_OR  };
```

### 7. 整数地板除说明

mslang **无 `//` 运算符**（`syntax.md §1.2`：`//` 恒为行注释，词法器直接跳过到行尾）。整数除法用 `/`，结果截断为零（与 C 语义一致）。本任务无需为 `//` 注册任何 ParseRule。

---

## 验收标准（checklist）

- [ ] `"1 + 2"` → `MS_ND_BINARY(MS_TOK_PLUS, MS_ND_INT(1), MS_ND_INT(2))`。
- [ ] `"1 + 2 * 3"` → `+` 在根，`*` 在右（优先级正确）。
- [ ] `"2 ** 3 ** 2"` → `**` 右结合，等价 `2 ** (3 ** 2) = 512`，AST 根为 `**`，右子为 `**`。
- [ ] `"-2 ** 2"` → 根为 `MS_ND_UNARY(MS_TOK_MINUS, MS_ND_BINARY(MS_TOK_STARSTAR, MS_ND_INT(2), MS_ND_INT(2)))`（`**` 优先级高于一元 `-`，等价 `-(2**2)`）。
- [ ] `"-1"` → `MS_ND_UNARY(MS_TOK_MINUS, MS_ND_INT(1))`。
- [ ] `"+5"` → `MS_ND_UNARY(MS_TOK_PLUS, MS_ND_INT(5))`（一元正号，语义为恒等）。
- [ ] `"not true"` → `MS_ND_UNARY(MS_TOK_NOT, MS_ND_BOOL(true))`。
- [ ] `"a is not b"` → `MS_ND_BINARY(MS_TOK_IS_NOT, MS_ND_IDENT(a), MS_ND_IDENT(b))`。
- [ ] `"a not in b"` → `MS_ND_BINARY(MS_TOK_NOT_IN, MS_ND_IDENT(a), MS_ND_IDENT(b))`。
- [ ] `"a or b and c"` → `or` 在根（`and` 优先级高于 `or`），`and(b,c)` 在右。
- [ ] `"a & b | c"` → `|` 在根（`&` 优先级高于 `|`），`&(a,b)` 在左。
- [ ] `"1 < 2 < 3"` → 根为 `MS_ND_BINARY(MS_TOK_LT, MS_ND_BINARY(MS_TOK_LT, MS_ND_INT(1), MS_ND_INT(2)), MS_ND_INT(3))`（左结合，不支持链式比较）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_expr_binary.c`）

```c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "parser/ms_arena.h"

static MsNode* parseStr(MsArena* a, const char* src) {
  MsParser p;
  msParserInit(&p, src, (uint32_t)strlen(src), "<t>", a);
  return msParseExpr(&p);
}

static void testAddMul(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = parseStr(&a, "1 + 2 * 3");
  MS_ASSERT_EQ(n->kind,          MS_ND_BINARY,  "root is binary");
  MS_ASSERT_EQ(n->binary.op,     MS_TOK_PLUS,   "root op is +");
  MS_ASSERT_EQ(n->binary.left->kind,  MS_ND_INT, "left is int");
  MS_ASSERT_EQ(n->binary.right->kind, MS_ND_BINARY, "right is binary");
  MS_ASSERT_EQ(n->binary.right->binary.op, MS_TOK_STAR, "right op is *");
  msArenaFree(&a);
}

static void testPowerRightAssoc(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = parseStr(&a, "2 ** 3 ** 2");
  MS_ASSERT_EQ(n->kind, MS_ND_BINARY, "root **");
  MS_ASSERT_EQ(n->binary.op, MS_TOK_STARSTAR, "root is **");
  MS_ASSERT_EQ(n->binary.right->kind, MS_ND_BINARY, "right is also **");
  msArenaFree(&a);
}

static void testUnaryNeg(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = parseStr(&a, "-42");
  MS_ASSERT_EQ(n->kind,        MS_ND_UNARY,   "unary neg");
  MS_ASSERT_EQ(n->unary.op,    MS_TOK_MINUS,  "op is -");
  MS_ASSERT_EQ(n->unary.operand->kind, MS_ND_INT, "operand int");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testAddMul);
  MS_RUN(testPowerRightAssoc);
  MS_RUN(testUnaryNeg);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 算术
print(1 + 2 * 3)     // 7
print(2 ** 10)       // 1024
print(2 ** 3 ** 2)   // 512（右结合）
print(-5 + 3)        // -2

// 位运算
print(0xFF & 0x0F)   // 15
print(1 << 8)        // 256

// 比较
print(1 < 2)         // true
print(2 is not nil)  // true
print(3 in [1,2,3])  // true
print(4 not in [1,2,3]) // true

// 逻辑短路
print(false and (1/0))  // false（不求值 1/0）
print(true or (1/0))    // true（不求值 1/0）
```

---

## Benchmark

（归入 T036 整体 parse bench）

---

## 风险与边界

- **`not` 的双重角色**：`MS_TOK_NOT` 在 `gParseRules` 中须同时设置 `prefix=parseUnary` 和 `infix=parseIsIn`（`prec=PREC_COMPARE`），两个字段在 §2 中统一赋值，避免分散覆盖导致顺序依赖。中缀上下文中 `not` 后若不跟 `in`，`msParserExpect` 报错 `"'in' expected after 'not'"`。
- **`-2 ** 2` 语义**：`parseUnary` 对 `-/+/~` 调用 `parsePrecedence(p, PREC_POWER)`（而非 `PREC_UNARY`），使幂运算先于一元符结合，即 `-2 ** 2 == -(2 ** 2) == -4`（与 Python 一致）。
- **一元 `+`**：`MS_TOK_PLUS` 作前缀注册 `parseUnary`，产生 `MS_ND_UNARY(MS_TOK_PLUS, operand)`，语义为恒等。
- **`await` / `<-` 前缀**：归属 T024/T025，本任务不注册。
- **链式比较**：mslang 不支持 Python 风格链式比较；`<` 左结合，`1 < x < 10` 解析为 `(1 < x) < 10`（bool 与 int 比较），行为已在验收标准中覆盖。
- **无 `!`**：`MS_TOK_EXCL`（单独 `!`）产生词法错误（T013 已处理），parser 无需额外处理。
