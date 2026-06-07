# P1-T006 Token 定义与 lexer 框架（位置/行号/错误恢复）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

建立词法分析器的骨架：`Token` 类型定义（含所有 token 种类枚举）、源位置信息（文件名/行号/列号）、`MsLexer` 状态机结构、`msLexerInit`/`msLexNext` 接口，以及错误恢复策略。后续 T007–T015 在此基础上添加各类 token 的具体扫描规则。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T005 | 公共头与前置类型就绪 |
| P0-T002 | `MsVec`（用于 lexer 内部缓冲） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1 词法规则（完整） |
| `syntax.md` | §1.1 字符集与编码 |
| `syntax.md` | §1.3 自动分号插入（ASI） |
| `syntax.md` | §1.4 关键字 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_lexer.h
src/lexer/ms_lexer.c
src/lexer/ms_token.h      # Token 种类枚举（仅内部使用，lexer 私有）
```

### Token 种类枚举（`src/lexer/ms_token.h`）

```c
typedef enum MsTokKind {
  // 字面量
  TOK_INT,        // 整数字面量
  TOK_FLOAT,      // 浮点字面量
  TOK_STRING,     // 字符串字面量
  TOK_FSTRING,    // f-string（$"…"）
  TOK_BYTES,      // bytes 字面量（b"…"）
  TOK_TRUE,       TOK_FALSE,  TOK_NIL,

  // 标识符
  TOK_IDENT,

  // 关键字（与 syntax.md §1.4 一一对应）
  TOK_IF, TOK_ELSE, TOK_FOR, TOK_BREAK, TOK_CONTINUE, TOK_RETURN,
  TOK_FUNC, TOK_CLASS, TOK_EXTENDS, TOK_IMPORT, TOK_AS, TOK_VAR,
  TOK_AND, TOK_OR, TOK_NOT, TOK_IN, TOK_IS,
  TOK_TRY, TOK_CATCH, TOK_FINALLY, TOK_RAISE, TOK_GO, TOK_CHAN,
  TOK_SELECT, TOK_ASYNC, TOK_AWAIT, TOK_MAKE, TOK_PASS,
  TOK_SWITCH, TOK_CASE, TOK_DEFAULT, TOK_FALLTHROUGH, TOK_WITH, TOK_DEL,

  // 运算符
  TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_STARSTAR,
  TOK_AMP, TOK_PIPE, TOK_CARET, TOK_SHL, TOK_SHR, TOK_TILDE,
  TOK_EQ, TOK_NEQ, TOK_LT, TOK_LE, TOK_GT, TOK_GE,
  TOK_ASSIGN, TOK_COLON_ASSIGN,
  TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_STAR_ASSIGN, TOK_SLASH_ASSIGN,
  TOK_PERCENT_ASSIGN, TOK_AMP_ASSIGN, TOK_PIPE_ASSIGN, TOK_CARET_ASSIGN,
  TOK_SHL_ASSIGN, TOK_SHR_ASSIGN,
  TOK_ARROW_LEFT,   // <-
  TOK_ARROW_RIGHT,  // -> (保留，未来扩展)
  TOK_DOTDOTDOT,    // ...
  TOK_STARSTAR_KWARG, // ** 在参数位（与 TOK_STARSTAR 同字符，由 parser 消歧）
  TOK_INC, TOK_DEC, // ++ --

  // 界符
  TOK_DOT, TOK_COMMA, TOK_SEMICOLON, TOK_COLON,
  TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
  TOK_LBRACE, TOK_RBRACE,

  // 特殊
  TOK_NEWLINE,    // 虚拟 `;`（ASI 插入）
  TOK_EOF,
  TOK_ERROR,      // 词法错误（携带错误消息）

  TOK_COUNT_,     // 枚举计数（内部用）
} MsTokKind;
```

### Source 位置（`include/mslang/ms_lexer.h`）

```c
struct MsSrcPos {
  const char* file;   // 文件名（不拥有内存，指向 MsStr 或字面量）
  uint32_t    line;   // 从 1 开始
  uint32_t    col;    // 从 1 开始（字节偏移，非 Unicode 列）
};

struct MsToken {
  MsTokKind  kind;
  struct MsSrcPos   pos;       // token 起始位置
  const char* start;   // 指向源码字节的开始（不拷贝）
  uint32_t   len;       // 字节长度
  // 对于字面量，附加解析后的值（避免 parser 重复解析）
  union {
    int64_t  ival;   // TOK_INT
    double   fval;   // TOK_FLOAT
    // TOK_STRING/BYTES/FSTRING：start/len 指向原始 token（含引号），
    // 解析由 compiler 在 const pool 时处理
  };
};

// Lexer 状态机
struct MsLexer {
  const char*  src;        // 源码全文（UTF-8，不可变）
  uint32_t     srcLen;
  uint32_t     pos;        // 当前读取字节偏移
  uint32_t     line;       // 当前行号
  uint32_t     lineStart;  // 当前行首字节偏移
  const char*  fileName;   // 文件名（调试用）

  struct MsToken      peek;       // 预读 token（peek-ahead）
  bool         hasPeek;

  // 错误收集（词法错误不立即 abort，由 parser 决定恢复策略）
  char         errBuf[256];
  bool         hasError;
};
```

### 关键函数签名

```c
// 初始化 lexer（src 必须在 lexer 生命周期内保持有效）
void     msLexerInit(struct MsLexer* lex, const char* src, uint32_t len,
                     const char* fileName);

// 返回下一个 token，调用方可重复调用直到 TOK_EOF
struct MsToken  msLexNext(struct MsLexer* lex);

// 返回当前 peek token 但不前进（用于 parser 的单 token 前瞻）
struct MsToken  msLexPeek(struct MsLexer* lex);

// 跳过 TOK_NEWLINE（parser 通常按需跳过）
struct MsToken  msLexNextSkipNewline(struct MsLexer* lex);

// 工具：token 种类名称（用于错误消息与 disasm/tokens 输出）
const char* msTokName(MsTokKind kind);
```

---

## 实现要点

1. **字节驱动**：lexer 始终以**字节**为单位读取（`uint8_t`），标识符/字符串中的 UTF-8 多字节序列透明传递；只有 f-string `{expr}` 展开（T011）和 Unicode 转义（T010）需要解码码点。
2. **peek 缓存**：`msLexPeek` 第一次调用 `msLexNext` 填充 `peek`，后续返回缓存；`msLexNext` 先检查缓存。这样 parser 可以安全地多次 peek 而不重复扫描。
3. **错误恢复**：遇到无法识别字符时，填充 `TOK_ERROR` token，设置 `hasError=true`，但不 abort——lexer 继续向前跳过问题字节，允许 parser 收集多个错误后再报告。
4. **行号追踪**：每遇到 `'\n'` 时 `line++`，`lineStart = pos`；`col = pos - lineStart + 1`。`'\r\n'` 视为一个换行（Windows 兼容）。
5. **token 的 `start`/`len`**：指向原始 `src` 字节，不拷贝；string/ident 值由调用方（parser/compiler）按需处理。

---

## 验收标准（checklist）

- [ ] `msLexerInit` 不崩溃，初始化后 `msLexNext` 第一次调用返回第一个有效 token。
- [ ] 对空字符串 `""` 调用后首个 token 为 `TOK_EOF`。
- [ ] `msLexPeek` 不消耗 token（连续两次 `peek` 返回相同 token）。
- [ ] 对 `"hello"` 字符串源码（含 `#include` 等关键字作标识符）能正确分词。
- [ ] `msTokName(TOK_PLUS)` 返回 `"+"`（或合理字符串），用于错误打印。
- [ ] 编译含本任务源文件后，`cmake --build build` 无警告。
- [ ] 行号从 1 开始，遇 `\n` 后下一 token 的 `pos.line` 递增。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_lexer_framework.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void testEmptySource(void) {
  struct MsLexer lex;
  msLexerInit(&lex, "", 0, "<test>");
  struct MsToken t = msLexNext(&lex);
  MS_ASSERT_EQ(t.kind, TOK_EOF, "empty → EOF");
}

static void testPeekDoesNotConsume(void) {
  struct MsLexer lex;
  const char* src = "x";
  msLexerInit(&lex, src, 1, "<test>");
  struct MsToken p1 = msLexPeek(&lex);
  struct MsToken p2 = msLexPeek(&lex);
  struct MsToken n  = msLexNext(&lex);
  MS_ASSERT_EQ(p1.kind, n.kind, "peek1 == next");
  MS_ASSERT_EQ(p2.kind, n.kind, "peek2 == next");
}

static void testLineTracking(void) {
  struct MsLexer lex;
  const char* src = "a\nb";
  msLexerInit(&lex, src, 3, "<test>");
  struct MsToken t1 = msLexNext(&lex);  // a（line 1）
  // 跳过 ASI 虚拟分号（T015 后才真正插入，此处 lexer 框架不处理）
  struct MsToken t2 = msLexNext(&lex);  // \n 或 b，取决于 ASI 实现
  MS_ASSERT_EQ(t1.pos.line, 1, "a on line 1");
  (void)t2;
}

int main(void) {
  MS_RUN(testEmptySource);
  MS_RUN(testPeekDoesNotConsume);
  MS_RUN(testLineTracking);
  return msTestSummary();
}
```

---

## .ms 使用示例

```
# CLI tokens 子命令（T016 实现后）：
$ mslang tokens examples/hello.ms
# 输出示例：
# 1:1  IDENT    "print"
# 1:6  LPAREN   "("
# 1:7  STRING   "\"hello\""
# 1:14 RPAREN   ")"
# 1:15 NEWLINE  ";"
# 2:1  EOF
```

---

## Benchmark

```c
// benchmarks/lexer/bench_lexer.c
// 目标：≥ 100 MB/s 词法分析吞吐（含 token 存储）
// 方法：将大型 .ms 文件反复词法分析 1000 次，统计 tokens/sec
// N/A（目标：≥ 10M tokens/sec 在现代 CPU 上，-O2）
```

---

## 风险与边界

- **`**` 的消歧**：`TOK_STARSTAR` 与 kwarg 展开 `**kwargs` 同字符；词法器始终产生 `TOK_STARSTAR`，parser（T021/T034）在语法位置消歧。
- **`<-` 的 ASI 影响**：`<-` 为整体 token，不会被 ASI 在中间插入分号；词法器需检查 `<` 后紧跟 `-` 时合并为 `TOK_ARROW_LEFT`。
- **`$"` 的识别**：`$` 字符在字面量外为非法；词法器在 `$` 后检查是否紧跟 `"`，是则进入 f-string 扫描（T011），否则产生 `TOK_ERROR`。
- **未覆盖**：具体字面量扫描逻辑（整数/浮点/字符串等）在 T007–T012 中分别实现；本任务只提供框架骨架，`msLexNext` 对未知字节产生 `TOK_ERROR`。
