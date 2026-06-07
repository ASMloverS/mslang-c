# P12-T184 stdlib: json（编码器）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `json` 模块的编码器（`json.dumps`）：将 mslang 值序列化为 JSON 字符串，支持自定义编码器、格式化输出。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj |
| P4-T059 | MsListObj |
| P4-T060 | MsMapObj |
| P12-T141 | strings.Builder（性能） |

---

## API 清单

```ms
// 基础编码
json.dumps(obj, *, indent=nil, separators=nil, sort_keys=false,
           ensure_ascii=true, allow_nan=false, default=nil) → str

// indent=nil → 紧凑格式（无换行）
// indent=2 → 2空格缩进的美化格式
// indent="\t" → tab 缩进
// separators=(", ", ": ") → 自定义分隔符（紧凑时默认(",",":") ）
// sort_keys=true → 字典键排序
// ensure_ascii=true → 非 ASCII 字符用 \uXXXX 转义
// allow_nan=true → 允许 Inf/NaN（严格 JSON 不允许）
// default=func → 不可序列化对象调用 default(obj)，返回可序列化值

// 写入文件
json.dump(obj, fp, **kwargs)   // fp 是可写文件对象

// JSONEncoder 类（可继承自定义）
class MyEncoder(json.JSONEncoder):
    func default(self, obj) {
        if isinstance(obj, datetime) { return obj.isoformat() }
        return super().default(obj)
    }

enc := MyEncoder(indent=2)
enc.encode(obj) → str
enc.iterencode(obj)  // → 迭代器，逐块生成（流式编码）

// 类型映射
// nil → null
// bool → true/false
// int → 整数  float → 浮点（NaN/Inf 按 allow_nan 处理）
// str → JSON 字符串（转义特殊字符）
// list/tuple → JSON 数组
// dict → JSON 对象（键必须为 str，否则转 str 或报错）
// set/frozenset → JSON 数组（元素不保证顺序）
// 其他 → 调用 default，若无则抛 TypeError
```

---

## 实现要点

```c
// 编码器核心：递归遍历 MsValue，写到 MsWriter 缓冲
// 检查循环引用（用 set 跟踪访问过的 container id）

typedef struct JsonEncCtx {
  MsWriter  buf;
  int       indent;      // -1=no indent
  char      item_sep;    // ','
  char*     key_sep;     // ': ' or ':'
  bool      sort_keys;
  bool      ensure_ascii;
  bool      allow_nan;
  MsValue   default_fn;  // callable or nil
  MsSetObj* seen;        // 循环引用检测（存 object id）
  int       level;       // 当前嵌套层级
  MsThread* thread;
} JsonEncCtx;

// str 编码：
// 遍历 UTF-8 字节，输出 " ... "
// 转义：\ " \n \r \t \b \f → \\x
// 控制字符：\u00xx
// ensure_ascii=true：非 ASCII codepoint → \uXXXX 或 𐀀（代理对 for BMP+）

// float 编码：
// NaN → "NaN" （allow_nan）或抛 ValueError
// Inf → "Infinity" （allow_nan）或抛 ValueError
// 使用 snprintf(buf, "%.17g", f)，去除尾随 0（但保留至少一位小数点后数字）

// dict 编码：
// sort_keys=true：对键列表排序（全为 str 时）
// 非 str 键：若 int/float/bool/nil 则转 str，否则抛 TypeError

// 循环引用：
// encode container 前：检查 obj id 在 seen 中 → 抛 ValueError
// encode 后：从 seen 移除
```

---

## 验收标准（checklist）

- [ ] `json.dumps(nil)` → `"null"`。
- [ ] `json.dumps({"b":2,"a":1}, sort_keys=true)` → `'{"a": 1, "b": 2}'`。
- [ ] `json.dumps([1,2,3], indent=2)` 格式正确（多行，2空格缩进）。
- [ ] `json.dumps("中文", ensure_ascii=true)` → `'"\\u4e2d\\u6587"'`。
- [ ] `json.dumps("中文", ensure_ascii=false)` → `'"中文"'`。
- [ ] 循环引用抛 ValueError（而非崩溃）。
- [ ] `default` 函数处理自定义类型。

---

## 测试用例（.ms）

```ms
import json

// 基础类型
print(json.dumps(nil))         // null
print(json.dumps(true))        // true
print(json.dumps(42))          // 42
print(json.dumps(3.14))        // 3.14
print(json.dumps("hello"))     // "hello"
print(json.dumps([1,"two",nil]))  // [1, "two", null]

// 嵌套对象
data := {"name":"Alice","scores":[95,87,92],"active":true}
print(json.dumps(data, sort_keys=true))
// {"active": true, "name": "Alice", "scores": [95, 87, 92]}

// 美化输出
print(json.dumps({"a":1,"b":[2,3]}, indent=2))
// {
//   "a": 1,
//   "b": [
//     2,
//     3
//   ]
// }

// 自定义编码器
import datetime
class DateEncoder(json.JSONEncoder):
    func default(self, obj) {
        if isinstance(obj, datetime.date) { return obj.isoformat() }
        return super().default(obj)
    }
enc := DateEncoder()
print(enc.encode({"date": datetime.date.today()}))
// {"date": "2024-01-15"}

// 循环引用
a := []
a.append(a)
try:
    json.dumps(a)
catch e as ValueError:
    print("Circular reference detected")
```

---

## Benchmark

```ms
import json, time

// 1M 次序列化小对象
obj := {"x": 1, "y": 2, "label": "point"}
t0 := time.now()
for _ in range(1_000_000) { json.dumps(obj) }
t1 := time.now()
print("1M dumps:", t1-t0, "ms")  // 目标 < 2s

// 大对象序列化
big := [{"id": i, "name": "user"+str(i)} for i in range(10000)]
t0 = time.now()
s := json.dumps(big)
t1 = time.now()
print("10K objects dump:", t1-t0, "ms", "length:", len(s))  // 目标 < 100ms
```
