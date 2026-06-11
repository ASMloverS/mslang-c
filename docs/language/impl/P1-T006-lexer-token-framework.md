# P1-T006 Token 定义与 lexer 框架（位置/行号/错误恢复）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

建立词法分析器的骨架：`Token` 类型定义（含所有 token 种类枚举）、源位置信息（文件名/行号/列号）、`MsLexer` 状态机结构、`msLexerInit`/`msLexerNext` 接口，以及错误恢复策略。后续 T007–T015 在此基础上添加各类 token 的具体扫描规则。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T005 | 公共头与前置类型就绪 |
| P0-T002 | 错误码枚举 `MsErrCode` / 内存分配封装 |
| P0-T003 | 测试框架（`ms_test.h`，C 单测使用） |

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
```

> `MsTokKind` 枚举直接定义在公共头 `include/mslang/ms_lexer.h` 中：`MsToken.kind`
> 字段与公共 API `msTokName()` 都依赖它，公共头必须自包含（c-style.md §2.3）。

### Token 种类枚举（`include/mslang/ms_lexer.h`）

```c
typedef enum MsTokKind {
  // 字面量
  MS_TOK_INT,        // 整数字面量
  MS_TOK_FLOAT,      // 浮点字面量
  MS_TOK_STRING,     // 字符串字面量
  MS_TOK_FSTRING,    // f-string（$"…"）
  MS_TOK_BYTES,      // bytes 字面量（b"…"）
  MS_TOK_TRUE,       MS_TOK_FALSE,  MS_TOK_NIL,

  // 标识符
  MS_TOK_IDENT,

  // 关键字（与 syntax.md §1.4 的 38 个保留字一一对应，
  // true/false/nil 归入上方字面量组）
  MS_TOK_IF, MS_TOK_ELSE, MS_TOK_FOR, MS_TOK_BREAK, MS_TOK_CONTINUE,
  MS_TOK_RETURN, MS_TOK_FUNC, MS_TOK_CLASS, MS_TOK_EXTENDS, MS_TOK_IMPORT,
  MS_TOK_AS, MS_TOK_VAR,
  MS_TOK_AND, MS_TOK_OR, MS_TOK_NOT, MS_TOK_IN, MS_TOK_IS,
  MS_TOK_TRY, MS_TOK_CATCH, MS_TOK_FINALLY, MS_TOK_RAISE, MS_TOK_GO,
  MS_TOK_CHAN, MS_TOK_SELECT, MS_TOK_ASYNC, MS_TOK_AWAIT, MS_TOK_MAKE,
  MS_TOK_PASS, MS_TOK_SWITCH, MS_TOK_CASE, MS_TOK_DEFAULT,
  MS_TOK_FALLTHROUGH, MS_TOK_WITH, MS_TOK_DEL, MS_TOK_ASSERT,

  // 运算符
  MS_TOK_PLUS, MS_TOK_MINUS, MS_TOK_STAR, MS_TOK_SLASH, MS_TOK_PERCENT,
  MS_TOK_STARSTAR,  // 幂运算；kwarg 展开 ** 由 parser 按语法位置消歧
  MS_TOK_AMP, MS_TOK_PIPE, MS_TOK_CARET, MS_TOK_SHL, MS_TOK_SHR, MS_TOK_TILDE,
  MS_TOK_EQ, MS_TOK_NEQ, MS_TOK_LT, MS_TOK_LE, MS_TOK_GT, MS_TOK_GE,
  MS_TOK_ASSIGN, MS_TOK_COLON_ASSIGN,
  MS_TOK_PLUS_ASSIGN, MS_TOK_MINUS_ASSIGN, MS_TOK_STAR_ASSIGN,
  MS_TOK_SLASH_ASSIGN, MS_TOK_PERCENT_ASSIGN, MS_TOK_AMP_ASSIGN,
  MS_TOK_PIPE_ASSIGN, MS_TOK_CARET_ASSIGN,
  MS_TOK_SHL_ASSIGN, MS_TOK_SHR_ASSIGN,
  MS_TOK_ARROW_LEFT,   // <-
  MS_TOK_DOTDOTDOT,    // ...
  MS_TOK_INC, MS_TOK_DEC, // ++ --

  // 界符
  MS_TOK_DOT, MS_TOK_COMMA, MS_TOK_SEMICOLON, MS_TOK_COLON,
  MS_TOK_LPAREN, MS_TOK_RPAREN, MS_TOK_LBRACKET, MS_TOK_RBRACKET,
  MS_TOK_LBRACE, MS_TOK_RBRACE,

  // 特殊
  MS_TOK_NEWLINE,    // 虚拟 `;`（ASI 插入）
  MS_TOK_EOF,
  MS_TOK_ERROR,      // 词法错误（消息存于 lex->errBuf，见「实现要点」3）

  MS_TOK_COUNT,      // 枚举计数（内部用）
} MsTokKind;
```

### Source 位置（`include/mslang/ms_lexer.h`）

```c
// 词法错误消息缓冲容量（字节，含 NUL）
#define MS_LEXER_ERR_MAX 256

struct MsSrcPos {
  const char* file;   // 文件名；指向调用方保证在 lexer 生命周期内有效的
                      // NUL 结尾 C 字符串（不拥有内存）
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
    int64_t  ival;   // MS_TOK_INT
    double   fval;   // MS_TOK_FLOAT
    // MS_TOK_STRING/BYTES/FSTRING：start/len 指向原始 token（含引号），
    // 解析由 compiler 在 const pool 时处理
  } val;
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
  char         errBuf[MS_LEXER_ERR_MAX];
  bool         hasError;
};
```

### 关键函数签名

```c
// 初始化 lexer（src 必须在 lexer 生命周期内保持有效）
void     msLexerInit(struct MsLexer* lex, const char* src, uint32_t len,
                     const char* fileName);

// 返回下一个 token，调用方可重复调用直到 MS_TOK_EOF
struct MsToken  msLexerNext(struct MsLexer* lex);

// 返回当前 peek token 但不前进（用于 parser 的单 token 前瞻）
struct MsToken  msLexerPeek(struct MsLexer* lex);

// 跳过 MS_TOK_NEWLINE（parser 通常按需跳过）
struct MsToken  msLexerNextSkipNewline(struct MsLexer* lex);

// 工具：token 种类名称（用于错误消息与 disasm/tokens 输出）
// 返回值固定：运算符/界符返回符号字面量（如 "+"、"("），
// 关键字返回小写名（如 "if"），其余返回大写种类名（如 "INT"、"EOF"）
const char* msTokName(MsTokKind kind);
```

---

## 实现要点

1. **字节驱动**：lexer 始终以**字节**为单位读取（`uint8_t`），标识符/字符串中的 UTF-8 多字节序列透明传递；只有 f-string `{expr}` 展开（T011）和 Unicode 转义（T010）需要解码码点。
2. **peek 缓存**：`msLexerPeek` 第一次调用 `msLexerNext` 填充 `peek`，后续返回缓存；`msLexerNext` 先检查缓存。这样 parser 可以安全地多次 peek 而不重复扫描。
3. **错误恢复**：遇到无法识别字符时，填充 `MS_TOK_ERROR` token，设置 `hasError=true`，但不 abort——lexer 继续向前跳过问题字节。错误消息存于 `lex->errBuf`，**仅对最近一个 `MS_TOK_ERROR` 有效**（后续错误会覆盖）；parser 收到 `MS_TOK_ERROR` 后必须立即拷贝消息再继续，方可收集多个错误后统一报告。
4. **行号追踪**：消费 `'\n'` 并使 `pos` 前进越过它**之后**执行 `line++; lineStart = pos;`，从而 `col = pos - lineStart + 1` 对下一行首字符得 1。`'\r\n'` 视为一个换行（Windows 兼容）；孤立 `'\r'` 按普通空白处理（syntax.md §1.2），仅 `'\n'` 触发行号递增。
5. **token 的 `start`/`len`**：指向原始 `src` 字节，不拷贝；string/ident 值由调用方（parser/compiler）按需处理。
6. **长度/偏移用 `uint32_t`**：偏离 c-style.md §4.1 的 `size_t` 约定，理由是与 `MsChunk`/`MsVec` 的 uint32_t 宽度保持一致（vm.md §2），并将 token 结构压缩到缓存友好的尺寸；隐含约束为单个源文件 ≤ 4GB。

---

## 验收标准（checklist）

- [ ] `msLexerInit` 不崩溃，初始化后 `msLexerNext` 第一次调用返回首个 token（框架阶段允许为 `MS_TOK_ERROR`，标识符/关键字扫描在 T007 实现）。
- [ ] 对空字符串 `""` 调用后首个 token 为 `MS_TOK_EOF`。
- [ ] `msLexerPeek` 不消耗 token（连续两次 `peek` 返回相同 token）。
- [ ] 对单字符未知字节（如 `@`）产生 `MS_TOK_ERROR` 且 `hasError == true`，随后 lexer 继续前进直至 `MS_TOK_EOF`。
- [ ] `msTokName(MS_TOK_PLUS)` 返回 `"+"`，`msTokName(MS_TOK_IF)` 返回 `"if"`，`msTokName(MS_TOK_EOF)` 返回 `"EOF"`（按签名注释的固定返回值表）。
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
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_EOF, "empty → EOF");
}

static void testPeekDoesNotConsume(void) {
  struct MsLexer lex;
  const char* src = "x";
  msLexerInit(&lex, src, 1, "<test>");
  struct MsToken p1 = msLexerPeek(&lex);
  struct MsToken p2 = msLexerPeek(&lex);
  struct MsToken n  = msLexerNext(&lex);
  MS_ASSERT_EQ(p1.kind, n.kind, "peek1 == next");
  MS_ASSERT_EQ(p2.kind, n.kind, "peek2 == next");
}

static void testLineTracking(void) {
  struct MsLexer lex;
  const char* src = "a\nb";
  msLexerInit(&lex, src, 3, "<test>");
  // 框架阶段标识符扫描未实现（T007），首 token 允许为 MS_TOK_ERROR；
  // 本测试只验证位置信息
  struct MsToken t1 = msLexerNext(&lex);  // 源码 "a" 处的 token（line 1）
  struct MsToken t2 = msLexerNext(&lex);  // \n 或 b，取决于 ASI（T015）实现
  MS_ASSERT_EQ(t1.pos.line, 1, "first token on line 1");
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

## CLI 演示（T016 后可用）

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
// 目标：≥ 100 MB/s 词法分析吞吐（含 token 存储，-O2，现代 CPU）
// 方法：将大型 .ms 文件反复词法分析 1000 次，统计 MB/s
```

---

## 风险与边界

- **`**` 的消歧**：幂运算与 kwarg 展开 `**kwargs` 同字符；词法器始终产生 `MS_TOK_STARSTAR`（不设独立 kind），parser（T021/T034）在语法位置消歧。
- **`<-` 的 ASI 影响**：`<-` 为整体 token，不会被 ASI 在中间插入分号；词法器需检查 `<` 后紧跟 `-` 时合并为 `MS_TOK_ARROW_LEFT`。
- **`$"` 的识别**：`$` 字符在字面量外为非法；词法器在 `$` 后检查是否紧跟 `"`，是则进入 f-string 扫描（T011），否则产生 `MS_TOK_ERROR`。
- **未覆盖**：标识符与关键字识别在 **T007** 实现；具体字面量扫描逻辑（整数/浮点/字符串等）在 T008–T012 中分别实现。本任务只提供框架骨架，`msLexerNext` 对一切尚无扫描规则的字节产生 `MS_TOK_ERROR`。
