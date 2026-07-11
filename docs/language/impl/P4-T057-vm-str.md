# P4-T057 str 类型（UTF-8 / 索引 / 切片 / 迭代 / hash）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 `str` 运行时类型（`MsStrObj`）：UTF-8 不可变字符串，支持索引（按字节，返回 int）、切片、迭代（按 Unicode 码点）、哈希、比较、拼接、格式化（`repr`/`str`）、常用方法（`len`/`codepointCount`/`upper`/`lower`/`strip`/`split`/`join`/`hasPrefix`/`hasSuffix`/`contains`/`replace`/`format`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T049 | `MsObject`/`MsType` |
| P4-T050 | `msGCAlloc` |
| P4-T056 | 比较协议（`tpEq`/`tpLt`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §2.5 string、§1.3 MsType |
| `stdlib/strings.md` | 字符串方法清单（camelCase 命名） |
| `syntax.md` | §1.8/§1.8.1 字符串/f-string 字面量、§2.3 索引/切片/`in` |
| `errors.md` | `TypeError`/`IndexError`/`ValueError` 异常语义 |
| `gc.md` | intern 表与移动式 GC 交互 |

---

## 待实现（C 文件）

```
src/runtime/ms_str.c         # MsStrObj + 所有方法
include/mslang/ms_str.h      # msNewStr / msNewStrNoIntern / msStrConcat / msStrFromInt
```

---

## 实现要点

### 1. MsStrObj 结构

```c
struct MsStrObj {
  struct MsObject head;   // 必须是第一个成员
  uint32_t        len;    // UTF-8 字节数
  uint32_t        cpLen;  // Unicode 码点数缓存（UINT32_MAX = 未计算）
  uint32_t        hash;   // FNV-1a 哈希（0 = 未计算）
  char            data[]; // 内联存储（flexible array member）
};
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
static MsValue strLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsStrObj* s = (struct MsStrObj*)MS_AS_OBJ(v);
  return MS_INT_VAL((int64_t)s->len);
}

static MsValue strEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msStrType) {
    return MS_BOOL_VAL(false);
  }
  struct MsStrObj* sa = (struct MsStrObj*)MS_AS_OBJ(a);
  struct MsStrObj* sb = (struct MsStrObj*)MS_AS_OBJ(b);
  if (sa == sb) {
    return MS_BOOL_VAL(true);  // intern 命中
  }
  if (sa->len != sb->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(memcmp(sa->data, sb->data, sa->len) == 0);
}

static MsValue strHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsStrObj* s = (struct MsStrObj*)MS_AS_OBJ(v);
  if (!s->hash) {
    s->hash = msFNV1a32(s->data, s->len);
    if (!s->hash) {
      s->hash = 1;  // 避免 0（标记为"未计算"）
    }
  }
  return MS_INT_VAL((int64_t)(uint32_t)s->hash);
}

static MsValue strLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msStrType) {
    return MS_ERROR_VALUE;  // TypeError（errors.md）
  }
  struct MsStrObj* sa = (struct MsStrObj*)MS_AS_OBJ(a);
  struct MsStrObj* sb = (struct MsStrObj*)MS_AS_OBJ(b);
  int cmp = memcmp(sa->data, sb->data, sa->len < sb->len ? sa->len : sb->len);
  return MS_BOOL_VAL(cmp < 0 || (cmp == 0 && sa->len < sb->len));
}

// str + str = concat
static MsValue strAdd(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msStrType) {
    return MS_ERROR_VALUE;  // TypeError（errors.md）
  }
  return msStrConcat(a, b);
}

// str * int = repeat
static MsValue strMul(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_INT(b)) {
    return MS_ERROR_VALUE;  // TypeError（errors.md）
  }
  int64_t n = MS_AS_INT(b);
  if (n <= 0) {
    return msNewStr("", 0);
  }
  struct MsStrObj* s = (struct MsStrObj*)MS_AS_OBJ(a);
  if (s->len != 0 && (uint64_t)n > UINT32_MAX / s->len) {
    return MS_ERROR_VALUE;  // OverflowError（errors.md），避免 newLen 截断溢出
  }
  uint32_t newLen = (uint32_t)(s->len * (uint32_t)n);
  char* buf = msAlloc(newLen + 1);
  for (int64_t i = 0; i < n; i++) {
    memcpy(buf + i * s->len, s->data, s->len);
  }
  buf[newLen] = '\0';
  MsValue r = msNewStrNoIntern(buf, newLen);
  msFree(buf);
  return r;
}

// str[i] → 按字节索引，返回 int（type-system.md §2.5）
static MsValue strGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  (void) vm;
  if (!MS_IS_INT(idx)) {
    return MS_ERROR_VALUE;  // TypeError（errors.md）
  }
  struct MsStrObj* s = (struct MsStrObj*)MS_AS_OBJ(v);
  int64_t i = MS_AS_INT(idx);
  if (i < 0) {
    i += s->len;
  }
  if (i < 0 || i >= s->len) {
    return MS_ERROR_VALUE;  // IndexError（errors.md）
  }
  return MS_INT_VAL((uint8_t)s->data[i]);
}

// 变长对象实际分配大小：头部 + 字节数据 + NUL 终止符
static size_t strVarSize(const struct MsObject* obj) {
  const struct MsStrObj* s = (const struct MsStrObj*)obj;
  return sizeof(struct MsStrObj) + s->len + 1;
}

// item in s（子串查找，空串恒真，见 stdlib/strings.md）
static MsValue strContains(struct MsVM* vm, MsValue v, MsValue item) {
  (void) vm;
  if (!MS_IS_OBJ(item) || MS_AS_OBJ(item)->type != &msStrType) {
    return MS_ERROR_VALUE;  // TypeError（errors.md）
  }
  struct MsStrObj* s = (struct MsStrObj*)MS_AS_OBJ(v);
  struct MsStrObj* sub = (struct MsStrObj*)MS_AS_OBJ(item);
  if (sub->len == 0) {
    return MS_BOOL_VAL(true);
  }
  if (sub->len > s->len) {
    return MS_BOOL_VAL(false);
  }
  for (uint32_t i = 0; i + sub->len <= s->len; i++) {
    if (memcmp(s->data + i, sub->data, sub->len) == 0) {
      return MS_BOOL_VAL(true);
    }
  }
  return MS_BOOL_VAL(false);
}

struct MsType msStrType = {
  .name       = "str",
  .objSize    = sizeof(struct MsStrObj),  // 头部大小，flexible array 不计入
  .varSize    = strVarSize,               // 变长对象：按实际字节数计算分配大小
  .traverse   = NULL,                     // 不含子对象（内联存储）
  .destroy    = NULL,                     // msGCAlloc 分配，GC 直接回收
  .tpRepr     = strRepr,   // 含引号：'"hello"'
  .tpStr      = strStr,    // 不含引号
  .tpHash     = strHash,
  .tpEq       = strEq,
  .tpLt       = strLt,
  .tpLen      = strLen,
  .tpAdd      = strAdd,
  .tpMul      = strMul,
  .tpGetitem  = strGetItem,
  .tpContains = strContains,
  .tpIter     = strIter,
};
```

### 4. 常用方法

方法作为 `MsCFunction` 注册到 `msStrType.methods`（T073 前直接在 `tpGetattr` 中查找）：

| 方法 | 签名 | 说明 |
|---|---|---|
| `len()` | `() → int` | 字节数（等同 `tpLen`） |
| `codepointCount()` | `() → int` | Unicode 码点数 |
| `upper()` | `() → str` | ASCII 大写（v1 只处理 ASCII） |
| `lower()` | `() → str` | ASCII 小写 |
| `strip()` | `(chars=" \t\n\r") → str` | 去除两端空白或指定字符 |
| `lstrip()` | `(chars=" \t\n\r") → str` | 去除左侧 |
| `rstrip()` | `(chars=" \t\n\r") → str` | 去除右侧 |
| `split()` | `(sep=nil, maxsplit=-1) → list` | 分割字符串 |
| `join()` | `(iterable) → str` | 连接序列 |
| `hasPrefix()` | `(prefix) → bool` | 前缀判断 |
| `hasSuffix()` | `(suffix) → bool` | 后缀判断 |
| `contains()` | `(sub) → bool` | 子串查找 |
| `index()` | `(sub, start=0, end=-1) → int` | 返回索引（-1 未找到） |
| `replace()` | `(old, new, count=-1) → str` | 替换子串 |
| `format()` | `(*args, **kwargs) → str` | `"{}".format(1)` 风格 |
| `encode()` | `(encoding="utf-8") → bytes` | 字符串编码（依赖 T058 bytes） |

---

## 验收标准（checklist）

- [ ] `"hello" + " world"` → `"hello world"`。
- [ ] `"ab" * 3` → `"ababab"`。
- [ ] `"hello"[1]` → `101`（字节值 `'e'`，索引按字节）。
- [ ] `"hello"[-1]` → `111`（字节值 `'o'`，负索引）。
- [ ] `"hello"[1:3]` → `"el"`（T065 切片语义，切片仍返回子字符串）。
- [ ] `len("hello")` → 5；`len("你好")` → 6（按字节，`你`/`好` 各占 3 字节）。
- [ ] `"你好".codepointCount()` → 2（按码点）。
- [ ] `"abc" < "abd"` → true。
- [ ] `"hello" == "hello"` → true（驻留时指针相等）。
- [ ] `hash("abc") == hash("abc")` → true（同值同 hash）。
- [ ] `"HeLLo".lower()` → `"hello"`；`.upper()` → `"HELLO"`。
- [ ] `"a,b,c".split(",")` → `["a","b","c"]`（T059 后）。
- [ ] `" ab ".strip()` → `"ab"`。
- [ ] `"ell" in "hello"` → true（`tpContains`）。

---

## 测试用例（C 单测）

### `tests/vm/test_str.c`

```c
#include "ms_test.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_str.h"

// run() 不提前 shutdown：调用方需在断言完成后自行 shutdown，
// 避免对已释放的 GC 堆对象读取（use-after-free）。
static MsValue run(const char* src, MsCompileResult* outResult) {
  *outResult = msCompile(src, strlen(src), "<t>");
  msVMInit();
  return msVMRun(outResult->chunk);
}

static void testConcat(void) {
  MsCompileResult r;
  MsValue v = run("\"hello\" + \" world\"", &r);
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "is obj");
  struct MsStrObj* s = (struct MsStrObj*)MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 11 && memcmp(s->data, "hello world", 11) == 0, "concat");
  msVMShutdown();
  msCompileResultFree(&r);
}

static void testIndex(void) {
  MsCompileResult r;
  MsValue v = run("\"abc\"[1]", &r);
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 'b', "index 1 = byte 'b'");
  msVMShutdown();
  msCompileResultFree(&r);
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
print(len(s))              // 13（字节数，UTF-8 编码后长度）
print(s.codepointCount())  // 9（Unicode 码点数）
print(s[0])                // 72（'H' 的字节值，索引按字节）
print(s.upper())           // HELLO, 世界（非ASCII保持原样）

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

- **`codepointCount()`/按码点迭代 O(n)**：索引 `s[i]` 按字节，O(1)；但 `codepointCount()` 与 `for ch in s` 仍需扫描 UTF-8 序列计数/取字符，v1 接受此代价，大字符串该类操作较慢；后续可缓存码点偏移表。
- **Intern 表 GC**：intern 表中的字符串是弱引用，GC 扫描时需将不可达的 intern 条目移除；T050 的 STW GC 在 sweep 阶段遍历 intern 表清除死亡条目。移动式 GC（P10 分代/半区复制）下，存活对象会被搬移到新地址，intern 表必须在 GC 后按新地址重新哈希或就地改写条目指针，否则将产生悬空指针（见 `gc.md`）。
- **字符串 > 64 字节不 intern**：大字符串不 intern，`==` 走内容比较（`memcmp`）。
- **方法实现顺序**：`split`/`join` 依赖 list（T059），`encode` 依赖 bytes（T058）；这些方法可以在对应任务完成后再实装，T057 先实现其他方法。
