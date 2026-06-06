# P4-T057 str 类型（UTF-8 / 索引 / 切片 / 迭代 / hash）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `str` 运行时类型（`MsStrObj`）：UTF-8 不可变字符串，支持索引（按 Unicode 码点）、切片、迭代、哈希、比较、拼接、格式化（`repr`/`str`）、常用方法（`len`/`upper`/`lower`/`strip`/`split`/`join`/`startsWith`/`endsWith`/`contains`/`replace`/`format`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T049 | `MsObject`/`MsType` |
| P4-T050 | `msGCAlloc` |
| P4-T056 | 比较协议（`tp_eq`/`tp_lt`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §8 str 类型 |
| `stdlib/strings.md` | 字符串方法清单（camelCase 命名） |

---

## 待实现（C 文件）

```
src/runtime/ms_str.c         # MsStrObj + 所有方法
include/mslang/ms_str.h      # msNewStr / msNewStrN / msStrIntern
```

---

## 实现要点

### 1. MsStrObj 结构

```c
typedef struct MsStrObj {
    MsObject  header;     // 必须是第一个成员
    uint32_t  len;        // UTF-8 字节数
    uint32_t  cpLen;      // Unicode 码点数（-1 = 未计算）
    uint32_t  hashVal;    // FNV-1a 哈希（0 = 未计算）
    char      data[];     // 内联存储（flexible array member）
} MsStrObj;
```

**字符串驻留（intern）**：短字符串（≤ 64 字节）驻留到全局 intern 表（开放寻址哈希），确保相同内容的短字符串共享同一 `MsStrObj*`（加速 dict key 查找，`==` 可退化为指针比较）。

### 2. 构造函数

```c
// 创建字符串（自动尝试 intern）
MsValue msNewStr(const char* data, uint32_t len);

// 创建字符串（不 intern，适用于大字符串）
MsValue msNewStrNoIntern(const char* data, uint32_t len);

// 字符串拼接（O(n)，生成新字符串）
MsValue msStrConcat(MsValue a, MsValue b);

// 格式化整数（快速路径，无 GC）
MsValue msStrFromInt(int64_t i);
```

### 3. 类型槽

```c
static MsValue strLen(MsValue v) {
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
    if (s->cpLen == UINT32_MAX) s->cpLen = msUtf8CodepointLen(s->data, s->len);
    return MS_INT_VAL((int64_t)s->cpLen);
}

static MsValue strEq(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msStrType)
        return MS_BOOL_VAL(false);
    MsStrObj* sa = (MsStrObj*)MS_AS_OBJ(a);
    MsStrObj* sb = (MsStrObj*)MS_AS_OBJ(b);
    if (sa == sb) return MS_BOOL_VAL(true);   // intern 命中
    if (sa->len != sb->len) return MS_BOOL_VAL(false);
    return MS_BOOL_VAL(memcmp(sa->data, sb->data, sa->len) == 0);
}

static MsValue strHash(MsValue v) {
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
    if (!s->hashVal) {
        s->hashVal = msFNV1a32(s->data, s->len);
        if (!s->hashVal) s->hashVal = 1;  // 避免 0（标记为"未计算"）
    }
    return MS_INT_VAL((int64_t)(uint32_t)s->hashVal);
}

static MsValue strLt(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msStrType)
        return MS_ERROR_VALUE;
    MsStrObj* sa = (MsStrObj*)MS_AS_OBJ(a);
    MsStrObj* sb = (MsStrObj*)MS_AS_OBJ(b);
    int cmp = memcmp(sa->data, sb->data, sa->len < sb->len ? sa->len : sb->len);
    return MS_BOOL_VAL(cmp < 0 || (cmp == 0 && sa->len < sb->len));
}

// str + str = concat
static MsValue strAdd(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msStrType)
        return MS_ERROR_VALUE;
    return msStrConcat(a, b);
}

// str * int = repeat
static MsValue strMul(MsValue a, MsValue b) {
    if (!MS_IS_INT(b)) return MS_ERROR_VALUE;
    int64_t n = MS_AS_INT(b);
    if (n <= 0) return msNewStr("", 0);
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(a);
    uint32_t newLen = (uint32_t)(s->len * n);
    char* buf = msAlloc(newLen + 1);
    for (int64_t i = 0; i < n; i++) memcpy(buf + i * s->len, s->data, s->len);
    buf[newLen] = '\0';
    MsValue r = msNewStrNoIntern(buf, newLen);
    msFree(buf);
    return r;
}

// str[i] → 按码点索引
static MsValue strGetItem(MsValue v, MsValue idx) {
    if (!MS_IS_INT(idx)) return MS_ERROR_VALUE;  // TypeError
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
    int64_t i = MS_AS_INT(idx);
    int64_t cpLen = (int64_t)strCpLen(s);
    if (i < 0) i += cpLen;
    if (i < 0 || i >= cpLen) return MS_ERROR_VALUE;  // IndexError
    // 线性扫描找第 i 个码点（初版，大字符串性能差）
    const char* p = s->data;
    for (int64_t j = 0; j < i; j++) p += msUtf8CharLen((uint8_t)*p);
    uint32_t charLen = msUtf8CharLen((uint8_t)*p);
    return msNewStr(p, charLen);
}

MsType msStrType = {
    .name = "str", .instanceSize = 0,  // 动态大小（flexible array）
    .tp_repr     = strRepr,   // 含引号：'"hello"'
    .tp_str      = strStr,    // 不含引号
    .tp_hash     = strHash,
    .tp_eq       = strEq,
    .tp_lt       = strLt,
    .tp_len      = strLen,
    .tp_add      = strAdd,
    .tp_mul      = strMul,
    .tp_getitem  = strGetItem,
    .tp_iter     = strIter,
    .tp_mark     = NULL,      // 不含子对象（内联存储）
    .tp_free     = NULL,      // msGCAlloc 分配，GC 直接 msFree
};
```

### 4. 常用方法

方法作为 `MsCFunction` 注册到 `msStrType.methods`（T073 前直接在 `tp_getattr` 中查找）：

| 方法 | 签名 | 说明 |
|---|---|---|
| `len()` | `() → int` | 码点数（等同 `tp_len`） |
| `upper()` | `() → str` | ASCII 大写（v1 只处理 ASCII） |
| `lower()` | `() → str` | ASCII 小写 |
| `strip()` | `(chars=nil) → str` | 去除两端空白或指定字符 |
| `lstrip()` | `(chars=nil) → str` | 去除左侧 |
| `rstrip()` | `(chars=nil) → str` | 去除右侧 |
| `split()` | `(sep=nil, maxsplit=-1) → list` | 分割字符串 |
| `join()` | `(iterable) → str` | 连接序列 |
| `startsWith()` | `(prefix) → bool` | 前缀判断 |
| `endsWith()` | `(suffix) → bool` | 后缀判断 |
| `contains()` | `(sub) → bool` | 子串查找 |
| `find()` | `(sub, start=0, end=-1) → int` | 返回索引（-1 未找到） |
| `replace()` | `(old, new, count=-1) → str` | 替换子串 |
| `format()` | `(*args, **kwargs) → str` | `"{}".format(1)` 风格 |
| `encode()` | `(encoding="utf-8") → bytes` | 字符串编码 |

---

## 验收标准（checklist）

- [ ] `"hello" + " world"` → `"hello world"`。
- [ ] `"ab" * 3` → `"ababab"`。
- [ ] `"hello"[1]` → `"e"`。
- [ ] `"hello"[-1]` → `"o"`（负索引）。
- [ ] `"hello"[1:3]` → `"el"`（T065 切片语义）。
- [ ] `len("hello")` → 5；`len("你好")` → 2（按码点）。
- [ ] `"abc" < "abd"` → true。
- [ ] `"hello" == "hello"` → true（驻留时指针相等）。
- [ ] `hash("abc") == hash("abc")` → true（同值同 hash）。
- [ ] `"HeLLo".lower()` → `"hello"`；`.upper()` → `"HELLO"`。
- [ ] `"a,b,c".split(",")` → `["a","b","c"]`（T059 后）。
- [ ] `" ab ".strip()` → `"ab"`。

---

## 测试用例（C 单测）

### `tests/vm/test_str.c`

```c
#include "ms_test.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_str.h"

static MsValue run(const char* src) {
    MsCompileResult r = msCompile(src, strlen(src), "<t>");
    msVMInit();
    MsValue v = msVMRun(r.chunk);
    msVMShutdown();
    msCompileResultFree(&r);
    return v;
}

static void testConcat(void) {
    MsValue v = run("\"hello\" + \" world\"");
    MS_ASSERT_TRUE(MS_IS_OBJ(v), "is obj");
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
    MS_ASSERT_TRUE(s->len == 11 && memcmp(s->data, "hello world", 11) == 0, "concat");
}

static void testIndex(void) {
    MsValue v = run("\"abc\"[1]");
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
    MS_ASSERT_TRUE(s->len == 1 && s->data[0] == 'b', "index 1 = 'b'");
}

int main(void) {
    MS_RUN(testConcat);
    MS_RUN(testIndex);
    return msTestSummary();
}
```

### .ms 使用示例

```ms
s := "Hello, 世界"
print(len(s))         // 9（码点数）
print(s[7])           // 世
print(s[-1])          // 界
print(s.upper())      // HELLO, 世界（非ASCII保持原样）

// 字符串格式化
name := "mslang"
print($"Hello, {name}!")   // Hello, mslang!（f-string，T011）
print("Value: {}".format(42))  // Value: 42

// 分割与连接
parts := "a,b,c".split(",")
print(parts)              // ["a", "b", "c"]
print(",".join(parts))    // a,b,c

// 成员检查
print("ell" in "hello")   // true
print("xyz" not in "hello")  // true
```

---

## Benchmark

```ms
// benchmarks/bench_str.ms
n := 1_000_000
s := "hello"
for i in range(n) {
    _ = s + " world"  // 每次分配新字符串
}
// 目标：> 5M string concat/sec（受 GC 影响）

// 更轻量：只做 len/hash
for i in range(n) {
    _ = len(s)
    _ = hash(s)
}
// 目标：> 50M ops/sec
```

---

## 风险与边界

- **UTF-8 码点索引 O(n)**：字符串按码点索引需要线性扫描（除非维护 O(n) 码点偏移表）。v1 接受此代价，大字符串索引较慢（文档说明）；后续可缓存偏移表。
- **Intern 表 GC**：intern 表中的字符串是弱引用，GC 扫描时需将不可达的 intern 条目移除；T050 的 STW GC 在 sweep 阶段遍历 intern 表清除死亡条目。
- **字符串 > 64 字节不 intern**：大字符串不 intern，`==` 走内容比较（`memcmp`）。
- **方法实现顺序**：`split`/`join` 依赖 list（T059），`encode` 依赖 bytes（T058）；这些方法可以在对应任务完成后再实装，T057 先实现其他方法。
