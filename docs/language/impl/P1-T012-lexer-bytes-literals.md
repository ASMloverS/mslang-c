# P1-T012 bytes 字面量 `b"…"`

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 bytes 字面量 `b"…"` 的词法扫描。bytes 字面量产生可变字节数组对象（非字符串类型）。词法处理与字符串类似，但 token 种类为 `TOK_BYTES`；转义规则相同，解码在 compiler 阶段（`msUnescapeBytes`）。

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
src/lexer/ms_lexer.c   # 添加 bytes 字面量分支（复用 scanString 逻辑）
```

---

## 实现要点

1. **识别**：`msLexNext` 见到 `b`，检查下一字节是否为 `"`；若是，进入 bytes 扫描（与 `scanString` 逻辑完全相同，但产生 `TOK_BYTES`）。若 `b` 后跟其他字符，则 `b` 作为标识符首字符（标识符 `b` 或以 `b` 开头的标识符）。
2. **转义**：与普通字符串相同的转义序列集合；`\u{H}` 在 bytes 上下文下编码为 UTF-8 字节序列（与字符串一致）。
3. **非 ASCII 限制**：bytes 字面量中直接写入非 ASCII 字节（`>= 0x80`）是否合法？参考 Python 3：bytes 字面量不允许非 ASCII 字节，必须用 `\xHH` 转义。初版按此规则：bytes 字面量内非 ASCII 字节（非转义形式）产生 `TOK_ERROR`。

---

## 验收标准（checklist）

- [ ] `b"hello"` → `TOK_BYTES`，`start`/`len` 指向 `b"hello"`（6 字节，含 `b` 和两个引号共 7 字节）。
- [ ] `b"\x41\x42\x43"` → `TOK_BYTES`，解码后为 `ABC`（在 compiler 层验证）。
- [ ] `b"\n\t\r"` → `TOK_BYTES`（转义序列合法）。
- [ ] `b"abc\xFF"` → `TOK_BYTES`（`\xFF` 合法，编码为字节 255）。
- [ ] `b"abc` → `TOK_ERROR`（未终止）。
- [ ] `b` 单独 → `TOK_IDENT("b")`（`b` 后非 `"` 时为标识符）。
- [ ] `bytes_var` → `TOK_IDENT`（以 `b` 开头的普通标识符）。

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
    MsToken t = msLexNext(&lex);
    MS_ASSERT_EQ(t.kind, TOK_BYTES, "bytes token");
    MS_ASSERT_EQ(t.len, 8, "raw len (b + 2 quotes + 5)");
}

static void testBAloneIsIdent(void) {
    const char* src = "b ";
    MsLexer lex;
    msLexerInit(&lex, src, 2, "<t>");
    MsToken t = msLexNext(&lex);
    MS_ASSERT_EQ(t.kind, TOK_IDENT, "b alone is ident");
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
- **`b` 前缀与大写 `B`**：mslang `syntax.md §1.9` 仅指定 `b"…"`（小写 `b`）；`B"…"` 不合法（产生 `TOK_ERROR`），与 Python 不同（Python 允许 `B"…"`）。
