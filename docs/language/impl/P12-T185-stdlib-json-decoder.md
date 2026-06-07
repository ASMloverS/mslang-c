# P12-T185 stdlib: json（解码器）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `json` 模块的解码器（`json.loads`）：将 JSON 字符串解析为 mslang 值，完成 json 模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T184 | json 编码器（共享模块框架） |

---

## API 清单

```ms
// 基础解码
json.loads(s, *, object_hook=nil, parse_float=nil, parse_int=nil,
           parse_constant=nil, object_pairs_hook=nil) → value

// object_hook：dict 每次构建后调用（允许替换为自定义类型）
// object_pairs_hook：接收 [(k,v)] 有序对列表（替代 object_hook）
// parse_float：str → float 的自定义转换（默认 float(s)）
// parse_int：str → int 的自定义转换
// parse_constant："-Infinity"/"Infinity"/"NaN" 的处理函数

// 读取文件
json.load(fp, **kwargs) → value

// JSONDecoder 类
dec := json.JSONDecoder(**kwargs)
dec.decode(s) → value
dec.raw_decode(s, idx=0) → (value, end_idx)  // 从 idx 开始解析，返回结束位置

// 错误
json.JSONDecodeError(msg, doc, pos)  // 解析错误，含行列信息
e.msg  e.doc  e.pos  e.lineno  e.colno
```

---

## 实现要点

```c
// 递归下降解析器（手写，高性能）
// 不使用 re 模块（避免依赖），纯手写 FSM

typedef struct JsonParser {
  const char* src;
  size_t      pos;
  size_t      len;
  MsThread*   thread;
  // options
  MsValue     object_hook;
  MsValue     parse_float;
  MsValue     parse_int;
} JsonParser;

// 顶层分发：skipWhitespace → 按首字符分派
// '{' → parse_object
// '[' → parse_array
// '"' → parse_string
// 't' → parse_true（检查 "rue"）
// 'f' → parse_false（检查 "alse"）
// 'n' → parse_null（检查 "ull"）
// '-'/digit → parse_number

// parse_string：
// 处理转义：\\ \" \/ \b \f \n \r \t \uXXXX
// \uXXXX 代理对：D800-DBFF 后接 DC00-DFFF → UTF-32 → UTF-8
// 构建 UTF-8 字节缓冲

// parse_number：
// 整数：strtoll（无 . 和 e/E）
// 浮点：strtod（含 . 或 e/E）
// parse_int/parse_float 钩子：传入数字字符串，调用用户函数

// parse_object：
// 解析 "key": value 对，构建 MsMapObj
// 检查：key 必须为字符串
// object_pairs_hook：构建 [(k,v)] 列表传入钩子

// 错误处理：
// 每次读字符前检查 pos < len（否则 JSONDecodeError）
// 错误信息包含位置（行号/列号）

// 性能优化：
// 长字符串复用 MsWriter 缓冲（避免多次 malloc）
// 数字解析：不调用 strtoll/strtod（太慢）而是手写积累
```

---

## 验收标准（checklist）

- [ ] `json.loads("null")` → `nil`。
- [ ] `json.loads('{"a":1,"b":[2,3]}')` → `{"a":1,"b":[2,3]}`。
- [ ] Unicode 转义：`json.loads('"\\u4e2d\\u6587"')` → `"中文"`。
- [ ] 代理对：`😀` → 😀（U+1F600）。
- [ ] `json.loads("invalid{}")` 抛 JSONDecodeError，含行列信息。
- [ ] `object_hook` 将每个 dict 转为自定义对象。
- [ ] round-trip：`json.loads(json.dumps(obj)) == obj`（标准类型）。

---

## 测试用例（.ms）

```ms
import json

// 基础
print(json.loads("null"))      // nil
print(json.loads("true"))      // true
print(json.loads("42"))        // 42
print(json.loads("3.14"))      // 3.14
print(json.loads('"hello"'))   // "hello"

// 复杂结构
data := json.loads('{"name":"Alice","scores":[95,87,92],"active":true}')
print(data["name"])     // Alice
print(data["scores"])   // [95,87,92]

// Unicode
print(json.loads('"\\u4e2d\\u6587"'))  // 中文
print(json.loads('"\\uD83D\\uDE00"'))  // 😀

// object_hook
class Point:
    func __init__(self, d) {
        self.x = d["x"]
        self.y = d["y"]
    }
p := json.loads('{"x":1,"y":2}', object_hook=lambda d: Point(d))
print(p.x, p.y)  // 1 2

// 错误处理
try:
    json.loads("{bad}")
catch e as json.JSONDecodeError:
    print("Error at line:", e.lineno, "col:", e.colno)

// round-trip
original := {"name":"Bob","tags":["a","b"],"score":3.14}
print(json.loads(json.dumps(original)) == original)  // true
```

---

## Benchmark

```ms
import json, time

// 解析大型 JSON
big_json := json.dumps([{"id":i,"name":"user"+str(i),"score":float(i)/10}
                        for i in range(10000)])
t0 := time.now()
data := json.loads(big_json)
t1 := time.now()
print("parse 10K objects:", t1-t0, "ms")  // 目标 < 100ms

// 1M 次解析小 JSON
s := '{"x":1,"y":2}'
t0 = time.now()
for _ in range(1_000_000) { json.loads(s) }
t1 = time.now()
print("1M loads:", t1-t0, "ms")  // 目标 < 2s
```
