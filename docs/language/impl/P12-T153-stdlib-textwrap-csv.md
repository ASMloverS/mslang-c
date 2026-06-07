# P12-T153 stdlib: textwrap / csv

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `textwrap`（文本自动换行/对齐）和 `csv`（CSV 文件读写）模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T134 | io 模块（csv 使用 file/StringIO） |
| P12-T141 | strings.Builder |

---

## API 清单

```ms
// textwrap
textwrap.wrap(text, width=70, **options) → list[str]
// 将 text 折行，每行不超过 width 字符，返回行列表（不含换行符）

textwrap.fill(text, width=70, **options) → str
// 同 wrap 但连接为单个字符串（join with "\n"）

textwrap.shorten(text, width, **options) → str
// 截断并添加省略号（末尾 "..."），保证 <= width

textwrap.dedent(text) → str
// 删除每行公共前导空白（去缩进）

textwrap.indent(text, prefix, predicate=nil) → str
// 给每行（或满足 predicate 的行）加前缀

textwrap.TextWrapper(width=70, **options)   // 类接口

// 选项：
// initial_indent, subsequent_indent, expand_tabs, tabsize
// fix_sentence_endings, break_long_words, break_on_hyphens

// csv
csv.reader(file_or_iterable, dialect="excel", **fmtparams)
// 返回迭代器，每次产出一个 list[str]

csv.writer(file, dialect="excel", **fmtparams)
// .writerow(row)  .writerows(rows)

csv.DictReader(f, fieldnames=nil, **fmtparams)
// 产出 dict（键来自首行 header 或 fieldnames）

csv.DictWriter(f, fieldnames, **fmtparams)
// .writeheader()  .writerow(dict)  .writerows(dicts)

csv.register_dialect(name, **fmtparams)
csv.Dialect                // 基类
// fmtparams: delimiter=",", quotechar='"', quoting=QUOTE_MINIMAL
// lineterminator="\r\n", skipinitialspace=false, strict=false
csv.QUOTE_MINIMAL, QUOTE_ALL, QUOTE_NONNUMERIC, QUOTE_NONE
```

---

## 实现要点

```c
// textwrap.wrap：
// 1. 规范化空白（expand_tabs，合并多空格）
// 2. 用 wordsep_simple_re 拆分词块（保留空白 token）
// 3. 贪心装行：当前行 + 词块不超 width 则追加，否则换行

// textwrap.dedent：
// 找所有非空行的最长公共空白前缀
// 然后 strip 每行的该前缀

// csv 解析 FSM：
// state: FIELD_START → IN_FIELD / IN_QUOTED / AFTER_QUOTE
// 支持 RFC 4180：引号字段中双引号转义
// 分隔符、换行符均可配置

typedef struct MsCsvReaderObj {
  MsObject header;
  MsValue  srcIter;
  char     delimiter;
  char     quotechar;
  int      quoting;
  bool     strict;
} MsCsvReaderObj;
```

---

## 验收标准（checklist）

- [ ] `textwrap.wrap("hello world foo bar", width=10)` 正确折行。
- [ ] `textwrap.dedent("  a\n  b\n  c")` → `"a\nb\nc"`。
- [ ] `textwrap.indent("a\nb", ">> ")` → `">> a\n>> b"`。
- [ ] csv.reader 正确解析带引号字段（含逗号、换行的字段）。
- [ ] csv.writer 写入后 reader 读回结果一致（round-trip）。
- [ ] DictReader 用首行作为 fieldnames。

---

## 测试用例（.ms）

```ms
import textwrap, csv, io

// textwrap
text := "The quick brown fox jumps over the lazy dog."
lines := textwrap.wrap(text, width=20)
for l in lines { print(l) }

// dedent
code := """
    def hello():
        pass
"""
print(textwrap.dedent(code))

// csv round-trip
buf := io.StringIO()
w := csv.writer(buf)
w.writerow(["name","age","city"])
w.writerow(["Alice","30","New York"])
w.writerow(["Bob","25","San, Francisco"])  // 含逗号

buf.seek(0)
r := csv.reader(buf)
for row in r { print(row) }

// DictReader
buf2 := io.StringIO("name,score\nAlice,95\nBob,87")
for row in csv.DictReader(buf2) {
    print(row["name"], row["score"])
}
```
