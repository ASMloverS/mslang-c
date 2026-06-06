# P10-T122 大对象区（mmap / VirtualAlloc）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现**大对象区**（Large Object Space）：超过阈值（默认 128KB）的对象直接通过 `mmap`（POSIX）或 `VirtualAlloc`（Windows）分配，独立于 semi-space 和 free list，简化内存管理。大对象直接归入老代，GC 时只做标记-清除（不复制）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T115 | 对象头分代位（GC_GEN_LARGE） |
| P10-T120 | 老代 GC（大对象在标记阶段被枚举） |

---

## 实现要点

### 1. 大对象分配

```c
#define LARGE_OBJ_THRESHOLD (128 * 1024)  // 128KB

typedef struct MsLargeObj {
    MsObject* obj;      // 分配的大对象
    size_t    size;     // 分配大小（含头部）
    bool      marked;   // GC 标记
    struct MsLargeObj* next;
} MsLargeObjEntry;

MsLargeObjEntry* gLargeObjList = NULL;

MsObject* msAllocLarge(size_t size, MsType* type) {
    // 对齐到页（4KB）
    size_t allocSize = ALIGN_UP(size, 4096);

#ifdef _WIN32
    void* mem = VirtualAlloc(NULL, allocSize,
                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void* mem = mmap(NULL, allocSize, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NULL;
#endif

    MsObject* obj = (MsObject*)mem;
    obj->type    = type;
    obj->gcFlags = GC_GEN_LARGE;  // 大对象标记

    // 登记到全局大对象链表
    MsLargeObjEntry* entry = msAlloc(sizeof(MsLargeObjEntry));
    entry->obj    = obj;
    entry->size   = allocSize;
    entry->marked = false;
    entry->next   = gLargeObjList;
    gLargeObjList = entry;

    gVM.gc.bytesAlloc += allocSize;
    return obj;
}
```

### 2. 大对象释放

```c
void msFreeLarge(MsObject* obj, size_t size) {
    size_t allocSize = ALIGN_UP(size, 4096);
#ifdef _WIN32
    VirtualFree(obj, 0, MEM_RELEASE);
#else
    munmap(obj, allocSize);
#endif
    gVM.gc.bytesAlloc -= allocSize;
}
```

### 3. GC 时大对象的处理

```c
// 在 msMajorGC（T119/T120）中，除扫描中/老代链表外，还扫描大对象链表：
void sweepLargeObjects(void) {
    MsLargeObjEntry** ep = &gLargeObjList;
    while (*ep) {
        MsLargeObjEntry* e = *ep;
        MsObject* obj = e->obj;
        if ((obj->gcFlags & 0x03) == GC_WHITE) {
            // 不可达：释放
            *ep = e->next;
            if (obj->type->tp_free) obj->type->tp_free(obj);
            msFreeLarge(obj, e->size);
            msFree(e);
        } else {
            // 存活：重置颜色
            obj->gcFlags &= ~0x03;
            ep = &e->next;
        }
    }
}
```

### 4. 大对象阈值调整

```c
// 可通过环境变量调整：
//   MSLANG_LARGE_THRESHOLD=65536  (64KB)
void msInitLargeObjThreshold(void) {
    const char* env = getenv("MSLANG_LARGE_THRESHOLD");
    if (env) gLargeObjThreshold = (size_t)atol(env);
    else     gLargeObjThreshold = LARGE_OBJ_THRESHOLD;
}
```

---

## 验收标准（checklist）

- [ ] 分配 ≥ 128KB 的对象 → 使用 mmap/VirtualAlloc。
- [ ] 大对象的 `gcFlags` 为 `GC_GEN_LARGE`。
- [ ] GC 时：大对象被枚举为根（如果被全局/栈引用），不可达的大对象被 munmap/VirtualFree。
- [ ] 大对象分配/释放不干扰 semi-space 和 free list。
- [ ] `MsBytesObj`（大 bytes 对象）正确走大对象路径。

---

## 测试用例（.ms）

```ms
// 大对象分配
large_bytes := bytes(200_000)   // 200KB → 大对象区
print(len(large_bytes))         // 200000

large_list := list(range(50_000))  // ~400KB（MsValue 数组）
print(len(large_list))           // 50000

// GC 后大对象仍存在
import gc
gc.collect()
print(len(large_bytes))   // 200000（未被回收）

del large_bytes
gc.collect()
print("large_bytes freed")  // 已释放
```

---

## Benchmark

```ms
// benchmarks/bench_large_alloc.ms
import time
t0 := time.now()
for i in range(1000) {
    b := bytes(200_000)   // 1000 × 200KB = 200MB
    del b                 // 立即释放
    import gc
    gc.collect()
}
t1 := time.now()
print("1000 × 200KB alloc/free:", t1-t0, "ms")
// 目标 < 500ms（mmap 直接释放，无 GC 扫描）
```

---

## 风险与边界

- **mmap 的 NUMA 影响**：在多 NUMA 节点服务器上，`mmap` 分配的内存默认在调用线程的 NUMA 节点；多 Worker 访问跨节点大对象可能有性能损失（高级优化，超出初版范围）。
- **内存对齐**：大对象按 4KB 页对齐，最小开销为 0（如 128KB+1 字节 → 128KB+1 字节实际分配，mmap 页对齐到 132KB）。
