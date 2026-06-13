# P1-T010 字符串字面量与转义序列

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 `"…"` 字符串字面量的扫描，包括所有转义序列（`\n`/`\t`/`\r`/`\\`/`\"`/`\xHH`/`\u{H…}`）。Token 携带原始字节范围（含引号），解码由 compiler 在构建常量池时执行（与 Python 字节码编译器的惯用方式一致）。

**范围边界**：本任务仅处理裸 `"` 开头的普通字符串字面量；f-string（`$"…"`，`syntax.md §1.8.1`）由 P1-T011 实现，bytes（`b"…"`，`syntax.md §1.9`）由 P1-T012 实现。`msUnescapeString` 的 `raw` 约定为首尾各一个 `"` 字节——`$`/`b` 前缀由调用方剥离后再传入（三类字面量共享同一 `escape_seq` 解码逻辑）。

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
src/lexer/ms_lexer.c         # 添加 lexScanString() 内部函数
src/lexer/ms_lexer_unescape.c # 转义解码（compiler 调用）
include/mslang/ms_lexer.h    # 导出 msUnescapeString
```

### 内部函数（lexer 阶段）

```c
// 扫描 "…"（已消耗开引号 "）
// 产生 MS_TOK_STRING；start/len 覆盖含引号的原始字节
// 不解码转义（延迟到 compiler）
static struct MsToken lexScanString(struct MsLexer* lex, uint32_t start, struct MsSrcPos pos);
```

### 公共函数（compiler 调用）

```c
// 解码字符串字面量 raw（含引号，首尾各一个 '"' 字节）到 outBuf
// （不含引号，已展开转义）
// 成功返回 0 并写 *outLen（解码后字节数）；失败返回 -1 并填充 errBuf，
// errBuf 消息中包含出错转义在 raw 中的字节偏移（供 compiler 生成列号诊断）
// outBuf 由调用方分配：容量 >= rawLen 即保证足够
// （解码结果 <= rawLen - 2 字节，所有转义序列解码后均不长于原始形式）；
// 若解码字节数将超过 outBufLen，返回 -1，errBuf 填 "output buffer too small"
int msUnescapeString(const char* raw, uint32_t rawLen,
                     char* outBuf, uint32_t outBufLen, uint32_t* outLen,
                     char* errBuf, uint32_t errBufLen);
```

---

## 实现要点

1. **扫描阶段（lexer）**：不解码转义，直接推进 `pos` 直到遇到闭合 `"`（转义的 `\"` 跳过），记录 `start`/`len` 指向原始字节（含 `"…"` 引号）。未终止的字符串——在到达输入末尾（`pos >= srcLen`）或遇到换行符（`\n` 或 `\r`）前未见闭合 `"`——产生 `MS_TOK_ERROR`。遇 `\` 时若已到输入末尾（`\` 为最后一个字节），不得跳读下一字节，按未终止字符串处理。
2. **解码阶段（`msUnescapeString`）**：
   - `\n` → LF, `\t` → TAB, `\r` → CR, `\\` → `\`, `\"` → `"`。
   - `\xHH`（2 个十六进制数字）→ 单字节，值 `0x00`–`0xFF`。
   - `\u{H…}`（1~6 个十六进制数字）→ Unicode 码点，以 UTF-8 编码输出（最多 4 字节）。
   - 无效转义（如 `\q`）→ 返回 -1，填充错误消息。
3. **多行字符串**：`syntax.md §1.8` 不支持（无反引号原始字符串）；字符串内 `\n` 为字面换行（产生词法错误："未终止的字符串"），只能用 `\n` 转义表示换行。

---

## 验收标准（checklist）

- [ ] `"hello"` → `MS_TOK_STRING`，`start` 指向 `"`，`len=7`。
- [ ] `"a\nb"` → `MS_TOK_STRING`（`\n` 在引号内作为转义序列，不触发行号变化）。
- [ ] `"unterminated` → `MS_TOK_ERROR`（无闭合引号）。
- [ ] 字符串内出现字面换行（`"abc` + LF 或 CR）→ `MS_TOK_ERROR`（未终止）。
- [ ] `"abc\`（`\` 为输入最后一个字节）→ `MS_TOK_ERROR`（不越界读取）。
- [ ] `msUnescapeString("\"\\n\"", 4, ...)` → 返回 0，输出 `"\n"`（一个 LF 字节），`*outLen=1`。
- [ ] `msUnescapeString("\"\\x41\"", 6, ...)` → 返回 0，输出 `"A"`，`*outLen=1`。
- [ ] `msUnescapeString("\"\\u{1F600}\"", ...)` → 返回 0，UTF-8 编码 😀（4 字节 `0xF0 0x9F 0x98 0x80`），`*outLen=4`。
- [ ] `msUnescapeString("\"\\q\"", ...)` → 返回 -1，错误消息含 "invalid escape"。
- [ ] `outBufLen` 不足以容纳解码结果 → 返回 -1，错误消息含 "output buffer too small"。
- [ ] 连续两个字符串 `"a" "b"` 产生两个 `MS_TOK_STRING`（无自动拼接，拼接在 parser/compiler 层）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_string_literals.c`）

```c
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void testBasicString(void) {
  struct MsLexer lex;
  const char* src = "\"hello\"";
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_STRING, "string token");
  MS_ASSERT_EQ(t.len, 7, "raw len (with quotes)");
}

static void testUnterminatedString(void) {
  struct MsLexer lex;
  const char* src = "\"no close";
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "unterminated");
}

static void testUnescape(void) {
  char out[64]; char err[64];
  uint32_t outLen = 0;
  // "\n" → LF
  int rc = msUnescapeString("\"\\n\"", 4, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "unescape \\n ok");
  MS_ASSERT_EQ(outLen, 1, "unescape \\n len");
  MS_ASSERT_EQ((unsigned char)out[0], 10, "unescape \\n value");

  // "\x41" → 'A'
  rc = msUnescapeString("\"\\x41\"", 6, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "unescape \\x41 ok");
  MS_ASSERT_EQ(outLen, 1, "unescape \\x41 len");
  MS_ASSERT_EQ((unsigned char)out[0], 65, "unescape \\x41 value");
}

static void testUnescapeUnicode(void) {
  char out[16]; char err[64];
  uint32_t outLen = 0;
  // "\u{41}" → 'A' (U+0041 → 0x41 in UTF-8)
  int rc = msUnescapeString("\"\\u{41}\"", 8, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "U+0041 ok");
  MS_ASSERT_EQ(outLen, 1, "U+0041 len");
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
greeting := "hello\nworld"
tabbed := "tab:\there"
emoji := "unicode: \u{1F600}"
hexAbc := "hex: \x41\x42\x43"

print(greeting)
// hello
// world
print(tabbed)
// tab:<TAB>here（\t 展开为制表符）
print(emoji)
// unicode: 😀
print(hexAbc)
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
