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
| P2-T017 | `ND_UNARY`/`ND_BINARY` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3 表达式优先级表（含结合性） |
| `syntax.md` | §2.3.1 一元运算（`-`, `~`, `not`） |
| `syntax.md` | §2.3.2 二元算术（`+ - * / // % **`） |
| `syntax.md` | §2.3.3 位运算（`& | ^ << >>`） |
| `syntax.md` | §2.3.4 比较（`== != < > <= >= is is not in not in`） |
| `syntax.md` | §2.3.5 逻辑（`and or not`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册以下 prefix/infix 函数
```

---

## 实现要点

### 1. 字面量前缀（注册到 `gParseRules`）

```c
// TOK_INT → ND_INT
static MsNode* parseIntLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = ND_INT;
  n->pos          = p->prev.pos;
  n->lit_int.ival = p->prev.val.ival;
  return n;
}
// 同理：parseFloatLit, parseStringLit, parseBytesLit, parseBoolLit(true/false), parseNilLit
// parseIdentLit → ND_IDENT

// 注册：
// gParseRules[TOK_INT]   = { parseIntLit,   NULL, PREC_NONE };
// gParseRules[TOK_FLOAT] = { parseFloatLit, NULL, PREC_NONE };
// …
```

### 2. 一元运算（前缀）

```c
// -x  ~x  not x  +x
static MsNode* parseUnary(MsParser* p) {
  MsTokKind op  = p->prev.kind;
  MsSrcPos  pos = p->prev.pos;
  Precedence prec = (op == TOK_NOT) ? PREC_NOT : PREC_UNARY;
  MsNode* operand = parsePrecedence(p, prec);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = ND_UNARY;
  n->pos          = pos;
  n->unary.op     = op;
  n->unary.operand = operand;
  return n;
}
// 注册：
// gParseRules[TOK_MINUS] = { parseUnary, parseBinary, PREC_TERM };
// gParseRules[TOK_TILDE] = { parseUnary, NULL,        PREC_NONE };
// gParseRules[TOK_NOT]   = { parseUnary, NULL,        PREC_NONE };
// gParseRules[TOK_PLUS]  = { parseUnary, parseBinary, PREC_TERM };
```

### 3. 二元运算（中缀）

```c
static MsNode* parseBinary(MsParser* p, MsNode* left) {
  MsTokKind  op  = p->prev.kind;
  MsSrcPos   pos = p->prev.pos;
  Precedence prec = gParseRules[op].prec;
  // 幂 ** 右结合：下一层 prec 不 +1
  bool rightAssoc = (op == TOK_STARSTAR);
  MsNode* right = parsePrecedence(p, rightAssoc ? prec : prec + 1);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind          = ND_BINARY;
  n->pos           = pos;
  n->binary.op     = op;
  n->binary.left   = left;
  n->binary.right  = right;
  return n;
}
// 注册（中缀部分）：
// gParseRules[TOK_PLUS]     = { parseUnary,  parseBinary, PREC_TERM   };
// gParseRules[TOK_MINUS]    = { parseUnary,  parseBinary, PREC_TERM   };
// gParseRules[TOK_STAR]     = { NULL,         parseBinary, PREC_FACTOR };
// gParseRules[TOK_SLASH]    = { NULL,         parseBinary, PREC_FACTOR };
// gParseRules[TOK_PERCENT]  = { NULL,         parseBinary, PREC_FACTOR };
// gParseRules[TOK_STARSTAR] = { NULL,         parseBinary, PREC_POWER  };（右结合）
// gParseRules[TOK_SHL]      = { NULL,         parseBinary, PREC_SHIFT  };
// gParseRules[TOK_SHR]      = { NULL,         parseBinary, PREC_SHIFT  };
// gParseRules[TOK_AMP]      = { NULL,         parseBinary, PREC_BITAND };
// gParseRules[TOK_PIPE]     = { NULL,         parseBinary, PREC_BITOR  };
// gParseRules[TOK_CARET]    = { NULL,         parseBinary, PREC_BITXOR };
```

### 4. 比较运算符

比较运算符全部优先级 `PREC_COMPARE`，左结合。

```c
// gParseRules[TOK_EQ]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[TOK_NEQ] = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[TOK_LT]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[TOK_GT]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[TOK_LE]  = { NULL, parseBinary, PREC_COMPARE };
// gParseRules[TOK_GE]  = { NULL, parseBinary, PREC_COMPARE };
```

### 5. `is [not]` / `[not] in`（混合关键字运算符）

```c
// 'is' 后可跟 'not'；'not' 后可跟 'in'——中缀函数中向前探测：
static MsNode* parseIsIn(MsParser* p, MsNode* left) {
  MsTokKind op  = p->prev.kind;  // TOK_IS or TOK_IN or TOK_NOT
  bool negated  = false;

  if (op == TOK_IS && match(p, TOK_NOT)) {
    negated = true;           // is not
  } else if (op == TOK_NOT) {
    expect(p, TOK_IN, "'in' expected after 'not'");
    op = TOK_IN;
    negated = true;           // not in
  }
  MsNode* right = parsePrecedence(p, PREC_COMPARE + 1);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind       = ND_BINARY;
  n->pos        = p->prev.pos;
  // 编码负号：使用 TOK_IS_NOT / TOK_NOT_IN 虚拟 token，或在 binary.op 高位设标志
  // 简单方案：在 op 上叠加偏移（负结合）
  n->binary.op    = op;
  n->binary.left  = left;
  n->binary.right = right;
  // 附加 negated 标志——可在 binary 子结构扩展一个 bool
  // 暂用：若 negated，将 op 换成独立 TOK_IS_NOT / TOK_NOT_IN 枚举值（需 T006 追加）
  return n;
}
// gParseRules[TOK_IS]  = { NULL, parseIsIn, PREC_COMPARE };
// gParseRules[TOK_IN]  = { NULL, parseIsIn, PREC_COMPARE };
// gParseRules[TOK_NOT] 前缀已注册，中缀（not in）:
// gParseRules[TOK_NOT].infix = parseIsIn;
// gParseRules[TOK_NOT].prec  = PREC_COMPARE;
```

**注**：`TOK_IS_NOT` 与 `TOK_NOT_IN` 作为虚拟复合 token 添加到 `MsTokKind` 枚举（仅在 AST 中使用，词法器不产生），编译器据此生成对应字节码。

### 6. `and` / `or`（短路逻辑运算）

```c
// and/or 产生 ND_BINARY，但编译器需要短路跳转
// gParseRules[TOK_AND] = { NULL, parseBinary, PREC_AND };
// gParseRules[TOK_OR]  = { NULL, parseBinary, PREC_OR  };
```

### 7. 整数地板除（`//`）

`//` 词法器已产生 `TOK_FLOOR_DIV`（在 T014 注释跳过之后，若当前字符是 `/` 且下一字符也是 `/`，则注释跳过逻辑优先；但若 `//` 出现在表达式中，词法器应产生 `TOK_FLOOR_DIV`，而非注释）。

**澄清策略**：词法器在 `/` 之后看到 `/` 时，先看是否处于行首（空白后第一个 token）——不，这样不对。正确逻辑：

- `//` 始终产生注释（跳过到行尾），这是 mslang 语法设计（`syntax.md §1.2`）。
- **mslang 无地板除 `//` 运算符**（不同于 Python）。整数除法即 `/`，结果截断为零（与 C 语义一致）；地板除语义通过 `math.floor(a/b)` 或 `__floordiv__` 魔术方法实现（`type-system.md`）。
- `TOK_FLOOR_DIV` 枚举值保留但词法器不产生（避免与注释冲突）。

---

## 验收标准（checklist）

- [ ] `"1 + 2"` → `ND_BINARY(TOK_PLUS, ND_INT(1), ND_INT(2))`。
- [ ] `"1 + 2 * 3"` → `+` 在根，`*` 在右（优先级正确）。
- [ ] `"2 ** 3 ** 2"` → `**` 右结合，等价 `2 ** (3 ** 2) = 512`，AST 根为 `**`，右子为 `**`。
- [ ] `"-1"` → `ND_UNARY(TOK_MINUS, ND_INT(1))`。
- [ ] `"not True"` → `ND_UNARY(TOK_NOT, ND_BOOL(true))`。
- [ ] `"a is not b"` → `ND_BINARY(IS_NOT, ND_IDENT(a), ND_IDENT(b))`。
- [ ] `"a not in b"` → `ND_BINARY(NOT_IN, ND_IDENT(a), ND_IDENT(b))`。
- [ ] `"a or b and c"` → `or` 在根（`or` < `and` 优先级），`and(b,c)` 在右。
- [ ] `"a & b | c"` → `|` 在根（`|` < `&`），`&(a,b)` 在左。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_expr_binary.c`）

```c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "ms_arena.h"

static MsNode* parseStr(MsArena* a, const char* src) {
  MsParser p;
  msParserInit(&p, src, (uint32_t)strlen(src), "<t>", a);
  return msParseExpr(&p);
}

static void testAddMul(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = parseStr(&a, "1 + 2 * 3");
  MS_ASSERT_EQ(n->kind,          ND_BINARY,  "root is binary");
  MS_ASSERT_EQ(n->binary.op,     TOK_PLUS,   "root op is +");
  MS_ASSERT_EQ(n->binary.left->kind,  ND_INT, "left is int");
  MS_ASSERT_EQ(n->binary.right->kind, ND_BINARY, "right is binary");
  MS_ASSERT_EQ(n->binary.right->binary.op, TOK_STAR, "right op is *");
  msArenaFree(&a);
}

static void testPowerRightAssoc(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = parseStr(&a, "2 ** 3 ** 2");
  MS_ASSERT_EQ(n->kind, ND_BINARY, "root **");
  MS_ASSERT_EQ(n->binary.op, TOK_STARSTAR, "root is **");
  MS_ASSERT_EQ(n->binary.right->kind, ND_BINARY, "right is also **");
  msArenaFree(&a);
}

static void testUnaryNeg(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = parseStr(&a, "-42");
  MS_ASSERT_EQ(n->kind,        ND_UNARY,   "unary neg");
  MS_ASSERT_EQ(n->unary.op,    TOK_MINUS,  "op is -");
  MS_ASSERT_EQ(n->unary.operand->kind, ND_INT, "operand int");
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

- **`not` 的双重角色**：`TOK_NOT` 既是前缀一元运算符（`not x`），也是中缀 `not in` 的一部分。Pratt 框架通过同时注册 `prefix` 和 `infix` 处理；在 `infix` 上下文中 `not` 后面必须紧跟 `in`，否则报错。
- **链式比较**：mslang 不支持 Python 风格的链式比较（`1 < x < 10`）；`<` 左结合，`1 < x < 10` 解析为 `(1 < x) < 10`（bool 与 int 比较）。文档在 `syntax.md` 中需明确。
- **无 `!`**：`TOK_EXCL`（单独 `!`）产生词法错误（T013 已处理），parser 无需额外处理。
