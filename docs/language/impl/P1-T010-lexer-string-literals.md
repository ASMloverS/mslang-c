# P1-T010 字符串字面量与转义序列

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `"…"` 字符串字面量的扫描，包括所有转义序列（`\n`/`\t`/`\r`/`\\`/`\"`/`\xHH`/`\u{H…}`）。Token 携带原始字节范围（含引号），解码由 compiler 在构建常量池时执行（与 Python 字节码编译器的惯用方式一致）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | Lexer 框架 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.8 字符串字面量（双引号/转义序列） |
| `type-system.md` | §2.5 string（UTF-8，不可变） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c         # 添加 scanString() 内部函数
src/lexer/ms_lexer_unescape.c # 转义解码（compiler 调用）
include/mslang/ms_lexer.h    # 导出 msUnescapeString
```

### 内部函数（lexer 阶段）

```c
// 扫描 "…"（已消耗开引号 "）
// 产生 TOK_STRING；start/len 覆盖含引号的原始字节
// 不解码转义（延迟到 compiler）
static MsToken scanString(MsLexer* lex);
```

### 公共函数（compiler 调用）

```c
// 解码字符串字面量 raw（含引号）到目标 buf（不含引号，已展开转义）
// 返回解码后字节数；发生错误时返回 -1 并填充 errBuf
// outBuf 由调用方分配（保守估计：len 字节足够）
int msUnescapeString(const char* raw, uint32_t rawLen,
                     char* outBuf, uint32_t outBufLen,
                     char* errBuf, uint32_t errBufLen);
```

---

## 实现要点

1. **扫描阶段（lexer）**：不解码转义，直接推进 `pos` 直到遇到闭合 `"`（转义的 `\"` 跳过），记录 `start`/`len` 指向原始字节（含 `"…"` 引号）。未闭合的字符串（遇 `\0` 或 `\n` 前无 `"`）产生 `TOK_ERROR`。
2. **解码阶段（`msUnescapeString`）**：
   - `\n` → LF, `\t` → TAB, `\r` → CR, `\\` → `\`, `\"` → `"`。
   - `\xHH`（2 个十六进制数字）→ 单字节，值 `0x00`–`0xFF`。
   - `\u{H…}`（1~6 个十六进制数字）→ Unicode 码点，以 UTF-8 编码输出（最多 4 字节）。
   - 无效转义（如 `\q`）→ 返回 -1，填充错误消息。
3. **多行字符串**：`syntax.md §1.8` 不支持（无反引号原始字符串）；字符串内 `\n` 为字面换行（产生词法错误："未终止的字符串"），只能用 `\n` 转义表示换行。

---

## 验收标准（checklist）

- [ ] `"hello"` → `TOK_STRING`，`start` 指向 `"`，`len=7`。
- [ ] `"a\nb"` → `TOK_STRING`（`\n` 在引号内作为转义序列，不触发行号变化）。
- [ ] `"unterminated` → `TOK_ERROR`（无闭合引号）。
- [ ] `msUnescapeString("\"\\n\"", 4, ...)` → 输出 `"\n"`（一个 LF 字节），返回 1。
- [ ] `msUnescapeString("\"\\x41\"", 6, ...)` → 输出 `"A"`，返回 1。
- [ ] `msUnescapeString("\"\\u{1F600}\"", ...)` → UTF-8 编码 😀（4 字节 `0xF0 0x9F 0x98 0x80`），返回 4。
- [ ] `msUnescapeString("\"\\q\"", ...)` → 返回 -1，错误消息含 "invalid escape"。
- [ ] 连续两个字符串 `"a" "b"` 产生两个 `TOK_STRING`（无自动拼接，拼接在 parser/compiler 层）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_string_literals.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

static void testBasicString(void) {
  MsLexer lex;
  const char* src = "\"hello\"";
  msLexerInit(&lex, src, 7, "<t>");
  MsToken t = msLexNext(&lex);
  MS_ASSERT_EQ(t.kind, TOK_STRING, "string token");
  MS_ASSERT_EQ(t.len, 7, "raw len (with quotes)");
}

static void testUnterminatedString(void) {
  MsLexer lex;
  const char* src = "\"no close";
  msLexerInit(&lex, src, 9, "<t>");
  MsToken t = msLexNext(&lex);
  MS_ASSERT_EQ(t.kind, TOK_ERROR, "unterminated");
}

static void testUnescape(void) {
  char out[64]; char err[64];
  // "\n" → LF
  int n = msUnescapeString("\"\\n\"", 4, out, sizeof(out), err, sizeof(err));
  MS_ASSERT_EQ(n, 1, "unescape \\n len");
  MS_ASSERT_EQ((unsigned char)out[0], 10, "unescape \\n value");

  // "\x41" → 'A'
  n = msUnescapeString("\"\\x41\"", 6, out, sizeof(out), err, sizeof(err));
  MS_ASSERT_EQ(n, 1, "unescape \\x41 len");
  MS_ASSERT_EQ((unsigned char)out[0], 65, "unescape \\x41 value");
}

static void testUnescapeUnicode(void) {
  char out[16]; char err[64];
  // "\u{41}" → 'A' (U+0041 → 0x41 in UTF-8)
  int n = msUnescapeString("\"\\u{41}\"", 8, out, sizeof(out), err, sizeof(err));
  MS_ASSERT_EQ(n, 1, "U+0041 len");
  MS_ASSERT_EQ((unsigned char)out[0], 0x41, "U+0041 byte");
}

int main(void) {
  MS_RUN(testBasicString);
  MS_RUN(testUnterminatedString);
  MS_RUN(testUnescape);
  MS_RUN(testUnescapeUnicode);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
a := "hello\nworld"
b := "tab:\there"
c := "unicode: \u{1F600}"
d := "hex: \x41\x42\x43"

print(a)
// hello
// world
print(b)
// tab:	here
print(c)
// unicode: 😀
print(d)
// hex: ABC
```

---

## Benchmark

N/A（字符串扫描归入 T016 词法整体 bench）。

---

## 风险与边界

- **`\u{H}` 上限**：Unicode 码点最大为 `U+10FFFF`（21 位），`\u{10FFFF}` 合法；`\u{110000}` 及以上报错。
- **代理对**：`U+D800`–`U+DFFF` 为代理对，不是合法 Unicode 码点；`\u{D800}` 报错。
- **字符串内嵌 NUL**：`\x00` 产生字节 `0x00`；mslang 字符串使用 `len` 字段而非 NUL 终止，支持内嵌 NUL（`MsStr.data` + `MsStr.len`）。
