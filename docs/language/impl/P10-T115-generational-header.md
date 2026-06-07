# P10-T115 对象头分代位 + 年轻代 bump 分配 / TLAB

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

扩展 `MsObject` 头部，加入分代标记位（Young/Middle/Old）；实现**年轻代 bump pointer 分配器**（Thread-Local Allocation Buffer，TLAB）：小对象在线程本地缓冲区中顺序分配，极快；满时触发 Minor GC（T116）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T050 | 简易 GC（基线，被此任务替换） |
| P9-T112 | M:N 调度器（每个 Worker 有 TLAB） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `gc.md` | §3 分代设计 / §4 年轻代 |

---

## 实现要点

### 1. 扩展对象头分代标志

```c
// 原 MsObject.gcFlags（uint8_t）：
// bit 0-1: GC 颜色（WHITE/GRAY/BLACK），T050 已有
// 新增分代标志：
#define GC_GEN_YOUNG  0x00  // 年轻代（默认）
#define GC_GEN_MIDDLE 0x04  // 中代（经历 2 次 Minor GC）
#define GC_GEN_OLD    0x08  // 老年代（经历 N 次 Minor GC）
#define GC_GEN_LARGE  0x0C  // 大对象区（T122）

#define OBJ_GEN(obj)  ((obj)->gcFlags & 0x0C)
#define OBJ_SET_GEN(obj, gen) ((obj)->gcFlags = ((obj)->gcFlags & ~0x0C) | (gen))

// 晋升计数（可以用 gcFlags 高位或单独字段）
// 简单方案：gcFlags bit 4-5 存晋升次数（0-3，>= 2 晋升到 Old）
```

### 2. Semi-space 年轻代布局

```c
// 年轻代：两个等大的 semi-space（from / to）
// 每次 Minor GC：从 from 复制存活对象到 to，然后 swap
typedef struct MsYoungGen {
  uint8_t* fromStart;
  uint8_t* fromEnd;
  uint8_t* toStart;
  uint8_t* toEnd;
  size_t   semiSize;   // 默认 4MB
} MsYoungGen;

MsYoungGen gYoung;
```

### 3. TLAB 分配

```c
// 每个 Worker 有自己的 TLAB（避免分配时加锁）
typedef struct MsTLAB {
  uint8_t* cur;    // 当前分配指针
  uint8_t* end;    // TLAB 结束地址
} MsTLAB;

// 从 TLAB 分配（inline 快路径）
static inline MsObject* tlabAlloc(MsTLAB* tlab, size_t size) {
  size = ALIGN_UP(size, 8);
  uint8_t* p = tlab->cur;
  uint8_t* next = p + size;
  if (__builtin_expect(next <= tlab->end, 1)) {
    tlab->cur = next;
    return (MsObject*)p;
  }
  return NULL;  // TLAB 不足，走慢路径
}

// 慢路径：申请新 TLAB（从 Young 的 from-space 批量取）
MsObject* msGCAllocSlow(size_t size, MsType* type) {
  MsWorker* w = msCurrentWorker();
  // 申请新 TLAB（默认 256KB）
  if (!refillTLAB(&w->tlab)) {
    // from-space 满 → 触发 Minor GC
    msMinorGC();
    if (!refillTLAB(&w->tlab))
      return msAllocLarge(size, type);  // 大对象直接到大对象区
  }
  return tlabAlloc(&w->tlab, size);
}

// 对外接口（替换 T050 的 msGCAlloc）
MsObject* msGCAlloc(size_t size, MsType* type) {
  MsWorker* w = msCurrentWorker();
  MsObject* obj = tlabAlloc(&w->tlab, size);
  if (__builtin_expect(obj != NULL, 1)) {
    obj->type    = type;
    obj->gcFlags = GC_GEN_YOUNG;
    obj->gcNext  = NULL;
    return obj;
  }
  return msGCAllocSlow(size, type);
}
```

---

## 验收标准（checklist）

- [ ] 小对象分配在 TLAB 中，无锁，速度 > 100M alloc/s。
- [ ] TLAB 耗尽时申请新 TLAB（从 Young 的 from-space 批量取）。
- [ ] Young 的 from-space 满时触发 Minor GC（T116）。
- [ ] 大对象（> 阈值，初版 128KB）直接分配到大对象区（T122）。
- [ ] 新对象的 `gcFlags` 默认为 `GC_GEN_YOUNG`。

---

## 测试用例（C 单测）

```c
// tests/test_tlab.c
void testTlabAllocSpeed(void) {
  uint64_t t0 = msNow();
  for (int i = 0; i < 1000000; i++) {
    MsObject* obj = msGCAlloc(64, &msStrType);
    MS_ASSERT(obj != NULL);
    MS_ASSERT(OBJ_GEN(obj) == GC_GEN_YOUNG);
  }
  uint64_t t1 = msNow();
  // 1M 小对象分配应 < 50ms
  MS_ASSERT(t1 - t0 < 50);
}
```

---

## Benchmark

```ms
// benchmarks/bench_alloc.ms
n := 10_000_000
t0 := time.now()
for i in range(n) {
    x := [1, 2, 3]   // 每次分配一个 list
}
t1 := time.now()
print("10M list allocs:", t1-t0, "ms")
// 目标 < 2s（含 GC 开销）
```

---

## 风险与边界

- **单线程回退**：T112 之前（单线程调度器），只有主线程有 TLAB；`msCurrentWorker()` 返回 `&gWorkers[0]`（主 Worker）。
- **TLAB 大小**：默认 256KB（可调）；太小导致频繁申请；太大浪费内存。
