# json — JSON 序列化与反序列化

```ms
import json
```

## 概述

在 mslang 值与 JSON 字符串之间转换。参考 Python `json` 模块设计。支持紧凑输出、
缩进美化、键排序以及文件级读写。自定义类可通过 `__json__` 魔术方法参与序列化流程。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `json.JSONDecodeError` | class | JSON 解析失败异常，继承自 `ValueError` |

**JSONDecodeError 属性**

| 属性 | 类型 | 说明 |
|---|---|---|
| `e.msg` | `str` | 错误描述文本 |
| `e.doc` | `str` | 被解析的原始字符串 |
| `e.pos` | `int` | 出错位置的字节偏移量（从 0 开始） |

**mslang ↔ JSON 类型映射**

| JSON 类型 | mslang 类型 |
|---|---|
| object | map |
| array | list |
| string | str |
| number（整数） | int |
| number（浮点） | float |
| true / false | true / false |
| null | nil |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `encode` | `encode(obj, ensureAscii=true, indent=nil, sortKeys=false) → str` | 序列化为 JSON 字符串 |
| `encodePretty` | `encodePretty(obj, indent=2) → str` | 缩进美化序列化（便捷别名） |
| `decode` | `decode(s) → any` | 解析 JSON 字符串 |
| `encodeFile` | `encodeFile(path, obj, *, indent=nil, sortKeys=false)` | 序列化并写入文件 |
| `decodeFile` | `decodeFile(path) → any` | 读取文件并解析 |

## 详细语义

### json.encode

```
json.encode(obj, ensureAscii=true, indent=nil, sortKeys=false) → str
```

将 mslang 值 `obj` 序列化为 JSON 字符串。

- `ensureAscii=true`：非 ASCII 字符以 `\uXXXX` 转义；设为 `false` 时允许输出原始
  Unicode 字符（文件须为 UTF-8 编码）。
- `indent`：可为 `int`（每级缩进空格数）或 `str`（每级缩进字符串）；非 `nil` 时启用
  多行美化输出；`nil`（默认）产生紧凑单行输出。
- `sortKeys=true`：map 的键按字典序排序后输出，便于产生确定性结果。

**可序列化类型**：`nil`、`bool`、`int`、`float`、`str`、`list`、`tuple`（序列化为
array）、`map`。`set`、`frozenset` 不直接可序列化——请实现 `__json__` 或先转为 `list`。

`map` 的键必须是 `str`；其他类型的键会引发 `TypeError`。

自定义类若实现了 `__json__(self) → SerializableValue` 魔术方法，`encode` 调用时会
自动调用该方法，将返回值递归序列化。

---

### json.encodePretty

```
json.encodePretty(obj, indent=2) → str
```

等价于 `json.encode(obj, indent=indent, ensureAscii=false)`，用于快速生成人类可读的
格式化输出。`indent` 默认为 `2`（2 个空格缩进）。

---

### json.decode

```
json.decode(s) → any
```

将 JSON 字符串 `s` 解析为 mslang 值，类型映射见上表。

- `s` 必须为 `str`；传入 `bytes` 时抛 `TypeError`（请先 `s.decode("utf-8")`）。
- JSON 顶层值可为任意合法 JSON 类型（对象、数组、字符串、数值、true/false/null）。
- 解析失败时抛 `json.JSONDecodeError`，通过 `.pos` 定位出错位置。

**注意**：`decode` 不自动调用 `__from_json__`。若需将 map 还原为自定义类，需显式调用
`MyClass.__from_json__(data)` 或在解析后手动转换。

---

### json.encodeFile

```
json.encodeFile(path, obj, *, indent=nil, sortKeys=false)
```

将 `obj` 序列化后以 UTF-8 编码写入 `path` 指定的文件（覆盖写）。参数语义与
`encode` 相同（`ensureAscii` 固定为 `false`）。文件不存在时自动创建；写入失败时
抛 `OSError`。

---

### json.decodeFile

```
json.decodeFile(path) → any
```

以 UTF-8 读取 `path` 文件的全部内容，等价于 `json.decode(open(path).read())`。
文件不存在或不可读时抛 `OSError`；内容非合法 JSON 时抛 `json.JSONDecodeError`。

---

### 自定义序列化协议

**序列化**（encode 方向）：

```ms
class Point {
    func __init__(self, x, y) {
        self.x = x
        self.y = y
    }
    func __json__(self) {
        return {"x": self.x, "y": self.y}
    }
}
```

`encode` 遇到不认识的对象时，若对象有 `__json__` 方法则调用之，将返回值再次递归
序列化。若对象既无 `__json__` 也不属于内置可序列化类型，则抛 `TypeError`。

**反序列化**（decode 方向）：

```ms
class Point {
    class func __from_json__(cls, data) {
        return cls(data["x"], data["y"])
    }
}

raw := json.decode('{"x": 1, "y": 2}')
p := Point.__from_json__(raw)
```

`decode` 本身不自动还原类型，`__from_json__` 需手动调用。

## 示例

```ms
import json

// 1. 基础序列化与反序列化
data := {"name": "alice", "age": 30, "scores": [98, 87, 95]}
s := json.encode(data)
fmt.println(s)
// {"age":30,"name":"alice","scores":[98,87,95]}

parsed := json.decode(s)
fmt.println(parsed["name"])  // alice

// 2. 美化输出
fmt.println(json.encodePretty(data))
// {
//   "age": 30,
//   "name": "alice",
//   "scores": [
//     98,
//     87,
//     95
//   ]
// }

// 3. 自定义类序列化
class Color {
    func __init__(self, r, g, b) {
        self.r = r
        self.g = g
        self.b = b
    }
    func __json__(self) {
        return [self.r, self.g, self.b]
    }
    class func __from_json__(cls, data) {
        return cls(data[0], data[1], data[2])
    }
}

c := Color(255, 128, 0)
s2 := json.encode(c)
fmt.println(s2)  // [255,128,0]

c2 := Color.__from_json__(json.decode(s2))
fmt.println($"{c2.r},{c2.g},{c2.b}")  // 255,128,0

// 4. 文件读写
json.encodeFile("config.json", {"debug": false, "port": 8080}, indent=2)
cfg := json.decodeFile("config.json")
fmt.println(cfg["port"])  // 8080

// 5. 处理解析错误
try {
    json.decode("{bad json}")
} catch json.JSONDecodeError as e {
    fmt.println($"解析失败：{e.msg}，位置：{e.pos}")
}

// 6. set 不可序列化，转 list 后再编码
tags := set(["go", "python", "mslang"])
s3 := json.encode(list(tags))
fmt.println(s3)  // ["go","mslang","python"]  (顺序不确定)
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `json.JSONDecodeError` | 输入不是合法 JSON；继承自 `ValueError` |
| `TypeError` | 传入不可序列化的类型（如 `set`、无 `__json__` 的自定义类）；map 键非 `str`；`decode` 传入 `bytes` |
| `OSError` | `encodeFile`/`decodeFile` 文件读写失败 |
