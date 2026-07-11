# P4-T058 bytes 类型（可变字节序列）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `bytes` 运行时类型（`struct MsBytes`）：可变字节序列，类似 Python `bytearray`（mslang 的 `bytes` 是可变的，没有 `bytearray` 区分）。支持索引（返回 int）、切片、迭代、比较、拼接和常用方法。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | `msStrType`（`bytes.decode()` 返回 str） |
| P4-T050 | `msGCAlloc` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §2.6 bytes、§1.3 MsType |
| `syntax.md` | §1.9 bytes 字面量 (`b"..."`) |
| `errors.md` | `TypeError`/`IndexError`/`ValueError` 异常语义 |
| `gc.md` | 对象析构（`destroy`）与遍历（`traverse`）约定 |

---

## 待实现（C 文件）

```
src/runtime/ms_bytes.c     # MsBytes + 类型槽 + 方法
include/mslang/ms_bytes.h
```

---

## 实现要点

### 1. MsBytes 结构

```c
struct MsBytes {
  struct MsObject head;   // 必须是第一个成员（type-system.md §2.6）
  uint32_t        len;    // 当前字节数
  uint32_t        cap;    // 已分配容量
  uint8_t*        data;   // 字节数组（GC 非托管，手动 realloc）
};
```

注：`data` 是额外分配的（非内联），GC `destroy` 时负责 `msFree(data)`。

### 2. 构造与 GC

```c
// 创建 bytes（从字节数组复制）
MsValue msNewBytes(const uint8_t* data, uint32_t len);

// destroy：释放 data 缓冲区
static void bytesDestroy(struct MsObject* obj) {
  struct MsBytes* b = (struct MsBytes*)obj;
  msFree(b->data);
}

// traverse：data 不含 MsValue 子引用，不需要 traverse
```

### 3. 类型槽

```c
static MsValue bytesLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL(((struct MsBytes*)MS_AS_OBJ(v))->len);
}

// bytes[i] → int（0-255，按字节索引）；切片键（slice）延后到 T065 切片语义落地
static MsValue bytesGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  (void) vm;
  if (!MS_IS_INT(idx)) return MS_ERROR_VALUE;  // TypeError（errors.md）
  struct MsBytes* b = (struct MsBytes*)MS_AS_OBJ(v);
  int64_t i = MS_AS_INT(idx);
  if (i < 0) i += (int64_t)b->len;
  if (i < 0 || i >= (int64_t)b->len) return MS_ERROR_VALUE;  // IndexError（errors.md）
  return MS_INT_VAL((int64_t)b->data[i]);
}

static MsValue bytesSetItem(struct MsVM* vm, MsValue v, MsValue key, MsValue val) {
  (void) vm;
  struct MsBytes* b = (struct MsBytes*)MS_AS_OBJ(v);
  if (!MS_IS_INT(key) || !MS_IS_INT(val)) return MS_ERROR_VALUE;  // TypeError（errors.md）
  int64_t i = MS_AS_INT(key);
  if (i < 0) i += b->len;
  if (i < 0 || i >= (int64_t)b->len) return MS_ERROR_VALUE;  // IndexError（errors.md）
  int64_t n = MS_AS_INT(val);
  if (n < 0 || n > 255) return MS_ERROR_VALUE;  // ValueError（errors.md）
  b->data[i] = (uint8_t)n;
  return MS_NIL_VAL;
}

static MsValue bytesEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msBytesType) return MS_BOOL_VAL(false);
  struct MsBytes* ba = (struct MsBytes*)MS_AS_OBJ(a);
  struct MsBytes* bb = (struct MsBytes*)MS_AS_OBJ(b);
  if (ba->len != bb->len) return MS_BOOL_VAL(false);
  return MS_BOOL_VAL(memcmp(ba->data, bb->data, ba->len) == 0);
}

// 字典序比较（同 strLt，逐字节 memcmp）
static MsValue bytesLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msBytesType) return MS_ERROR_VALUE;  // TypeError（errors.md）
  struct MsBytes* ba = (struct MsBytes*)MS_AS_OBJ(a);
  struct MsBytes* bb = (struct MsBytes*)MS_AS_OBJ(b);
  int cmp = memcmp(ba->data, bb->data, ba->len < bb->len ? ba->len : bb->len);
  return MS_BOOL_VAL(cmp < 0 || (cmp == 0 && ba->len < bb->len));
}

static MsValue bytesAdd(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msBytesType) return MS_ERROR_VALUE;  // TypeError（errors.md）
  struct MsBytes* ba = (struct MsBytes*)MS_AS_OBJ(a);
  struct MsBytes* bb = (struct MsBytes*)MS_AS_OBJ(b);
  uint32_t newLen = ba->len + bb->len;
  uint8_t* buf = msAlloc(newLen);
  memcpy(buf, ba->data, ba->len);
  memcpy(buf + ba->len, bb->data, bb->len);
  MsValue r = msNewBytes(buf, newLen);
  msFree(buf);
  return r;
}

// tpIter/tpNext 延后：StopIteration 哨兵与迭代协议在 T065 前未落定
// （同 ms_str.c msStrType 的处理，见 ms_vm.c msContains 注释）
struct MsType msBytesType = {
  .name       = "bytes",
  .objSize    = sizeof(struct MsBytes),
  .traverse   = NULL,        // data 不含 MsValue 子引用
  .destroy    = bytesDestroy,
  .tpLen      = bytesLen,
  .tpEq       = bytesEq,
  .tpLt       = bytesLt,
  .tpAdd      = bytesAdd,
  .tpGetitem  = bytesGetItem,
  .tpSetitem  = bytesSetItem,
};
```

### 4. 常用方法

| 方法 | 签名 | 说明 |
|---|---|---|
| `len()` | `() → int` | 字节数 |
| `decode()` | `(encoding="utf-8") → str` | 解码为字符串 |
| `hex()` | `() → str` | 返回十六进制字符串 `"deadbeef"` |
| `fromHex()` | `(s) → bytes` | 从十六进制字符串构造 |
| `append()` | `(b: int) → nil` | 追加单字节 |
| `extend()` | `(iterable) → nil` | 追加多字节 |
| `copy()` | `() → bytes` | 返回副本 |
| `split()` | `(sep) → list[bytes]` | 分割（T059 后） |
| `find()` | `(sub, start=0) → int` | 子字节序列查找 |
| `startsWith()` | `(prefix) → bool` | 前缀 |
| `endsWith()` | `(suffix) → bool` | 后缀 |

---

## 验收标准（checklist）

- [ ] `b"hello"[0]` → 104（`'h'` 的 ASCII 码）。
- [ ] `b"hello"[-1]` → 111（`'o'`）。
- [ ] `b"\x41\x42" + b"\x43"` → `b"\x41\x42\x43"`（`"ABC"`）。
- [ ] `len(b"hello")` → 5（字节数，非码点数）。
- [ ] `b"hello" == b"hello"` → true；`b"hello" == b"world"` → false。
- [ ] `b"hello"[0] = 72` → 将 `'h'` 改为 `'H'`（可变性）。
- [ ] `b"hello"[1:3]` → `b"el"`（T065 切片语义，切片仍返回新 bytes）。
- [ ] `b"hello".decode()` → `"hello"`（str）。
- [ ] `b"\x00\xFF".hex()` → `"00ff"`。
- [ ] `bytes.fromHex("deadbeef")` → `b"\xde\xad\xbe\xef"`（T097 构造函数）。

---

## 测试用例（C 单测）

### `tests/vm/test_bytes.c`

```c
#include "ms_test.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_compiler.h"

static MsValue run(const char* src) {
  MsCompileResult r = msCompile(src, strlen(src), "<t>");
  msVMInit();
  MsValue v = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static void testBytesIndex(void) {
  MsValue v = run("b\"hello\"[0]");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 104, "b[0]='h'=104");
}

static void testBytesConcat(void) {
  MsValue v = run("b\"ab\" + b\"cd\"");
  struct MsBytes* b = (struct MsBytes*)MS_AS_OBJ(v);
  MS_ASSERT_TRUE(b->len == 4 && b->data[2] == 'c', "concat ok");
}

int main(void) {
  MS_RUN(testBytesIndex);
  MS_RUN(testBytesConcat);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// bytes 字面量
b := b"\x48\x65\x6c\x6c\x6f"
print(b.decode())    // Hello

// 可变性
b[0] = 0x68          // 'h'
print(b.decode())    // hello

// 十六进制
print(b"\xde\xad".hex())  // dead

// 拼接
msg := b"Hello" + b", " + b"World"
print(msg.decode())   // Hello, World

// 与 str 转换
s := "你好"
encoded := s.encode()   // bytes（UTF-8）
print(len(encoded))     // 6（两个汉字各 3 字节）
print(encoded.decode()) // 你好
```

---

## Benchmark

```ms
// benchmarks/bench_bytes.ms
n := 1_000_000
b := b"\x00"
for i in range(10) {
    b = b + b   // 1 → 1024 字节（tpAdd 加倍，避免依赖未定义的 tpMul）
}
for i in range(n) {
    _ = len(b)
    _ = b[0]
}
// 目标：> 50M read ops/sec
```

---

## 风险与边界

- **`data` 的 GC 管理**：`struct MsBytes` 的 `data` 是通过 `msAlloc` 分配的非 GC 内存，必须在 `destroy` 中手动释放。若忘记，valgrind 会报告泄漏。
- **append 触发 realloc**：`append()` 扩容时 `msRealloc(b->data, newCap)`；realloc 不移动 `struct MsBytes` 本身（GC 安全）。
- **`bytes` 可变性与并发**：P9 并发后，多协程共享同一 bytes 对象会有数据竞争；v1 不加锁（文档提示）。
