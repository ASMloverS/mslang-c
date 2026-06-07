# P10-T118 分代写屏障 + card table + remembered set

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现**分代写屏障**：当老年代/中代对象写入年轻代对象的引用时，记录这条跨代引用（通过 card table 标记），使 Minor GC 能找到所有跨代引用作为额外根，避免活的年轻代对象被错误回收。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T115 | 对象头分代位 |
| P10-T116 | Minor GC |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `gc.md` | §5 写屏障 + card table |

---

## 实现要点

### 1. Card Table

```c
// Card table：将堆划分为 512 字节 card；每个 card 对应 1 字节标志
// 当 card 中的对象写入年轻代引用时，将对应 card 标记为 dirty

#define CARD_SIZE 512
static uint8_t* gCardTable = NULL;   // 全局 card table
static uint8_t* gHeapBase  = NULL;

static inline uint32_t cardIndex(const void* addr) {
  return (uint32_t)((uintptr_t)addr - (uintptr_t)gHeapBase) / CARD_SIZE;
}

static inline void markCardDirty(const void* addr) {
  gCardTable[cardIndex(addr)] = 1;
}
```

### 2. 写屏障（OP_SET_ATTR / OP_SET_INDEX 的内联检查）

```c
// 每次将一个值写入对象字段时触发写屏障
// 仅当 "写入方是老代/中代，且新值是年轻代对象" 时记录

#define WRITE_BARRIER(obj, newVal) \
  do { \
    if (MS_IS_OBJ(newVal) && OBJ_GEN(MS_AS_OBJ(obj)) != GC_GEN_YOUNG \
      && OBJ_GEN(MS_AS_OBJ(newVal)) == GC_GEN_YOUNG) { \
      markCardDirty(obj); \
    } \
  } while (0)

// 在 OP_SET_ATTR 中：
case OP_SET_ATTR: {
  MsValue val = POP();
  MsValue obj = PEEK(0);
  // ... 赋值逻辑 ...
  WRITE_BARRIER(MS_AS_OBJ(obj), val);  // 写屏障
  DISPATCH();
}
```

### 3. Remembered Set（从 card table 衍生）

```c
// Minor GC 时：扫描所有 dirty card，找到其中老代/中代对象写入的年轻代引用
void msEnumerateRememberedSet(MsRootVisitor visit, void* data) {
  size_t cardCount = heapSize / CARD_SIZE;
  for (size_t i = 0; i < cardCount; i++) {
    if (!gCardTable[i]) continue;
    gCardTable[i] = 0;  // 清除

    // 扫描此 card 范围内的对象
    uint8_t* start = gHeapBase + i * CARD_SIZE;
    uint8_t* end   = start + CARD_SIZE;
    scanCardForYoungRefs(start, end, visit, data);
  }
}

static void scanCardForYoungRefs(uint8_t* start, uint8_t* end,
                                  MsRootVisitor visit, void* data) {
  // 线性扫描 card 中的对象（需要对象大小信息）
  uint8_t* p = start;
  while (p < end) {
    MsObject* obj = (MsObject*)p;
    if (OBJ_GEN(obj) != GC_GEN_YOUNG) {
      // 检查此老/中代对象的所有字段
      obj->type->tpScan(obj, visitIfYoung, data);
    }
    p += ALIGN_UP(msObjSize(obj), 8);
  }
}
```

---

## 验收标准（checklist）

- [ ] 老代对象写入年轻代引用 → card 被标记为 dirty。
- [ ] Minor GC 时：dirty card 扫描的对象引用被加入根集合。
- [ ] 年轻代对象写入任何值 → 不触发写屏障（年轻代 Minor GC 会全量扫描）。
- [ ] 写屏障开销 < 5 ns/次（单纯条件分支 + 内存写）。

---

## 测试用例（C 单测）

```c
// tests/test_write_barrier.c
void testCrossGenRefTracked(void) {
  // 制造老代对象 oldObj
  MsListObj* oldList = forceToOldGen(msNewList());

  // 写入年轻代引用
  MsValue youngStr = msNewStr("young", 5);
  msListAppend(MS_OBJ_VAL((MsObject*)oldList), youngStr);

  // 触发 Minor GC（youngStr 无其他根，但 oldList 引用它）
  msMinorGC();

  // youngStr 应存活（被 remembered set 保护）
  MS_ASSERT(!MS_IS_NIL(oldList->items[0]));
}
```

---

## Benchmark

```c
// 写屏障开销测量（C microbench）
// 目标：1M 次写屏障 < 5ms
```

---

## 风险与边界

- **card table 扫描的准确性**：card 可能跨越两个对象边界；需要从 card 起始处对齐到对象头。实现方案：维护"对象起始偏移"数组，或从 card 起始对齐到 8 字节对象边界。
- **Dijkstra 并发写屏障（T120）**：并发 GC 时还需要 Dijkstra 增量写屏障；本任务实现分代写屏障（card table），T120 在此基础上叠加三色写屏障。
