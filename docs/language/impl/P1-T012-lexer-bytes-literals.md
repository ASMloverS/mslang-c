# P1-T012 bytes 字面量 `b"…"`

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 bytes 字面量 `b"…"` 的词法扫描。bytes 字面量产生可变字节数组对象（非字符串类型）。词法处理与字符串类似，但 token 种类为 `MS_TOK_BYTES`；转义规则相同，解码函数 `msUnescapeBytes` 须在 `src/lexer/ms_lexer_unescape.c` 中新增（复用 `msUnescapeString` 框架，额外校验非 ASCII 限制）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T010 | 字符串字面量（共用转义框架） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.9 bytes 字面量（`b"…"`） |
| `type-system.md` | §2.6 bytes（可变字节数组） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c          # 添加 bytes 字面量分支（参考 lexScanString，产生 MS_TOK_BYTES）
src/lexer/ms_lexer_unescape.c # 新增 msUnescapeBytes（复用 msUnescapeString 框架 + ASCII 限制）
```

---

## 实现要点

1. **识别**：`msLexerNext` 见到 `b`，检查下一字节是否为 `"`；若是，进入 bytes 扫描（参考 `lexScanString`（`ms_lexer.c:399`），但需参数化 token 种类为 `MS_TOK_BYTES` 并额外插入非 ASCII 字节检查）。若 `b` 后跟其他字符，则 `b` 作为标识符首字符（标识符 `b` 或以 `b` 开头的标识符）。
2. **转义**：与普通字符串相同的转义序列集合；`\u{H}` 在 bytes 上下文下编码为 UTF-8 字节序列（与字符串一致）。
3. **非 ASCII 限制**：bytes 字面量中直接写入裸非 ASCII 字节（`>= 0x80`，非转义形式）产生 `MS_TOK_ERROR`；通过 `\xHH`、`\u{H}` 等转义序列产生的 `>= 0x80` 字节视为合法（转义由 `msUnescapeBytes` 在后续阶段解码）。
4. **ASI**：`MS_TOK_BYTES` 须纳入行尾自动分号插入（ASI）触发集合（`syntax.md §1.3`），与字符串 token 处理保持一致。

---

## 验收标准（checklist）

- [ ] `b"hello"` → `MS_TOK_BYTES`，`start`/`len` 指向 `b"hello"`（原始 token 长度 8 字节：`b` + 两个引号 + 5 字节内容）。 <!-- v:ctest:test_bytes_literal -->
- [ ] `b"\x41\x42\x43"` → `MS_TOK_BYTES`，解码后为 `ABC`（`msUnescapeBytes` 验证）。 <!-- v:ctest:test_bytes_literal -->
- [ ] `b"\n\t\r"` → `MS_TOK_BYTES`（转义序列合法）。 <!-- v:ctest:test_bytes_literal -->
- [ ] `b"abc\xFF"` → `MS_TOK_BYTES`（`\xFF` 合法，编码为字节 255）。 <!-- v:ctest:test_bytes_literal -->
- [ ] `b"abc` → `MS_TOK_ERROR`（未终止）。 <!-- v:ctest:test_bytes_literal -->
- [ ] `b` 单独 → `MS_TOK_IDENT("b")`（`b` 后非 `"` 时为标识符）。 <!-- v:ctest:test_b_alone_is_ident -->
- [ ] `bytes_var` → `MS_TOK_IDENT`（以 `b` 开头的普通标识符）。 <!-- v:ctest:test_bytes_literal -->
- [ ] `B"hello"` → `MS_TOK_IDENT("B")` + `MS_TOK_STRING`（大写前缀不触发 bytes 路径）。 <!-- v:ctest:test_bytes_literal -->
- [ ] `b"hello"` 位于行尾 → 触发 ASI（插入虚拟 `;`）（`syntax.md §1.3`）。 <!-- v:ctest:test_bytes_literal -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_bytes_literals.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void testBytesLiteral(void) {
  const char* src = "b\"hello\"";
  MsLexer lex;
  msLexerInit(&lex, src, 8, "<t>");
  MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_BYTES, "bytes token");
  MS_ASSERT_EQ(t.len, 8, "raw len (b + 2 quotes + 5)");
}

static void testBAloneIsIdent(void) {
  const char* src = "b ";
  MsLexer lex;
  msLexerInit(&lex, src, 2, "<t>");
  MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "b alone is ident");
}

int main(void) {
  MS_RUN(testBytesLiteral);
  MS_RUN(testBAloneIsIdent);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
buf := b"\x48\x65\x6C\x6C\x6F"   // "Hello" in bytes
print(len(buf))   // 5
buf[0] = 104      // 'h'
print(buf)        // b"hello"

// bytes 与 str 不同类型
print(type(buf) == bytes)    // true
print(type("hello") == str)  // true
```

---

## Benchmark

N/A（归入 T016 词法整体 bench）。

---

## 风险与边界

- **`bytearray` vs `bytes`**：`bytes` 字面量在脚本层产生可变字节数组（`type-system.md §2.6`），不同于 Python 3 中 `bytes` 不可变、`bytearray` 可变的区分。mslang 中只有 `bytes`（可变），无 `bytearray` 专用类型（`stdlib.md §1` 有 `bytearray` 构造函数作为别名）。
- **`b` 前缀与大写 `B`**：mslang `syntax.md §1.9` 仅指定 `b"…"`（小写 `b`）。`B"…"` 不触发 bytes 路径：`B` 按既有标识符规则扫描为 `MS_TOK_IDENT("B")`，随后 `"hello"` 独立产生 `MS_TOK_STRING`，不产生 `MS_TOK_ERROR`。
- **`\u{H}` 与非 ASCII 限制**：非 ASCII 限制仅针对 bytes 字面量中直接写入的裸字节（源码中 `>= 0x80` 的未转义字节）。通过 `\xHH`、`\u{H}` 等转义序列产生的 `>= 0x80` 字节不在词法层校验，由 `msUnescapeBytes` 在后续阶段解码并校验（与 `syntax.md §1.9` 转义文法一致）。
