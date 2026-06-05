# csv — CSV 读写

```ms
import csv
```

## 概述

以逗号分隔值（CSV）格式读写表格数据。行为参考 Python `csv` 标准库，支持自定义分隔符、
引号字符和换行策略。`Reader`/`Writer` 操作原始 `list[str]`，`DictReader`/`DictWriter`
操作 `map[str]str`，适合带表头的结构化数据。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `csv.QUOTE_MINIMAL` | int | 仅在必要时引用字段（默认） |
| `csv.QUOTE_ALL` | int | 始终引用所有字段 |
| `csv.QUOTE_NONNUMERIC` | int | 引用所有非数值字段 |
| `csv.QUOTE_NONE` | int | 从不引用；特殊字符以 `escapechar` 转义 |
| `csv.Error` | class | CSV 解析错误基类（继承自 `ValueError`） |
| `csv.Reader` | class | 行迭代器（list 模式） |
| `csv.Writer` | class | 行写入器（list 模式） |
| `csv.DictReader` | class | 行迭代器（map 模式） |
| `csv.DictWriter` | class | 行写入器（map 模式） |

## 函数签名速查

| 函数/方法 | 签名 | 说明 |
|---|---|---|
| `csv.reader` | `reader(fileOrIter, delimiter=",", quotechar='"') → Reader` | 构造 Reader |
| `csv.writer` | `writer(file, delimiter=",", quotechar='"', quoting=QUOTE_MINIMAL) → Writer` | 构造 Writer |
| `csv.DictReader` | `DictReader(file, fieldnames=nil, delimiter=",") → DictReader` | 构造 DictReader |
| `csv.DictWriter` | `DictWriter(file, fieldnames, delimiter=",") → DictWriter` | 构造 DictWriter |
| `r.__iter__` | `for row in r` | Reader 迭代，每行为 `list[str]` |
| `w.writerow` | `w.writerow(row: list)` | 写一行 |
| `w.writerows` | `w.writerows(rows: list[list])` | 写多行 |
| `dr.__iter__` | `for row in dr` | DictReader 迭代，每行为 `map[str]str` |
| `dw.writeheader` | `dw.writeheader()` | 写表头行 |
| `dw.writerow` | `dw.writerow(mapping: map)` | 写一行（map） |
| `dw.writerows` | `dw.writerows(mappings: list[map])` | 写多行（map 列表） |

## 详细语义

### csv.reader

```
csv.reader(fileOrIter, delimiter=",", quotechar='"') → Reader
```

接受文件对象或任意可迭代的字符串序列，返回 `Reader` 迭代器。每次迭代产出一个 `list[str]`，
表示该行解析后的各字段值。字段中的引号字符由 `quotechar` 指定，引号内的 `delimiter` 和
换行均视为字段内容。遇到格式错误时抛 `csv.Error`。

### csv.writer

```
csv.writer(file, delimiter=",", quotechar='"', quoting=QUOTE_MINIMAL) → Writer
```

返回 `Writer` 对象，将行数据格式化并写入 `file`（须实现 `write(s)`）。
`quoting` 参数控制何时对字段加引号：
- `QUOTE_MINIMAL`：仅当字段含 `delimiter`、`quotechar` 或换行时加引号。
- `QUOTE_ALL`：始终对所有字段加引号。
- `QUOTE_NONNUMERIC`：对非 int/float 字段加引号。
- `QUOTE_NONE`：从不加引号（若字段含特殊字符且无 `escapechar`，抛 `csv.Error`）。

### csv.DictReader

```
csv.DictReader(file, fieldnames=nil, delimiter=",") → DictReader
```

若 `fieldnames=nil`，读取第一行作为表头，后续行映射为 `map[str]str`。若提供 `fieldnames`，
从第一行数据行开始解析（不跳过任何行）。行字段数多于表头时，多余字段存入键 `"_extra"` 的
列表；少于表头时，缺失字段值为 `nil`。

### csv.DictWriter

```
csv.DictWriter(file, fieldnames, delimiter=",") → DictWriter
```

`fieldnames` 为有序的字段名列表，决定列顺序。`writerow` 接受 `map`，按 `fieldnames` 顺序
提取值。若 map 中某字段缺失，写入空字符串；若有多余键，忽略（不报错）。建议先调用
`writeheader()` 写入表头行。

## 示例

```ms
import csv

// 读取 CSV 文件
f := open("data.csv", "r")
r := csv.reader(f)
for row in r {
    fmt.println(row)  // ["alice", "30", "engineer"]
}
f.close()

// 写入 CSV 文件
out := open("out.csv", "w")
w := csv.writer(out)
w.writerow(["name", "age", "role"])
w.writerows([
    ["alice", "30", "engineer"],
    ["bob",   "25", "designer"],
])
out.close()

// DictReader：按表头名访问字段
f2 := open("data.csv", "r")
for row in csv.DictReader(f2) {
    fmt.println($"{row['name']} ({row['age']})")
}
f2.close()

// DictWriter：按字段名写入
out2 := open("out2.csv", "w")
fields := ["name", "score"]
dw := csv.DictWriter(out2, fields)
dw.writeheader()
dw.writerows([
    {"name": "alice", "score": "95"},
    {"name": "bob",   "score": "87"},
])
out2.close()

// 自定义分隔符（TSV）
tsvData := "a\tb\tc\n1\t2\t3"
for row in csv.reader(tsvData.splitlines(), delimiter="\t") {
    fmt.println(row)
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `csv.Error` | CSV 格式解析错误（如引号未闭合、`QUOTE_NONE` 下含特殊字符） |
| `IOError` | 文件读写失败 |
| `ValueError` | `QUOTE_NONE` 且字段含特殊字符但未设 `escapechar`（通过 `csv.Error` 子类报告） |
