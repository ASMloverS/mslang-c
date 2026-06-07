# P10-T116 Minor GC：半区复制（Cheney）+ 转发指针

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现年轻代 Minor GC：使用 Cheney's BFS 半区复制算法，将 Young 代 from-space 中存活的对象复制到 to-space，清除 from-space，晋升老对象（age >= 阈值）到中代/老代。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T115 | TLAB + 年轻代布局 |
| P10-T117 | 精确根枚举（Minor GC 需要扫描所有根） |
| P10-T118 | 写屏障 + remembered set（跨代引用根） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `gc.md` | §4 Minor GC / Cheney 算法 |

---

## 实现要点

### 1. 转发指针

```c
// 复制后，原对象的 type 字段被替换为特殊 "forwarded" 标记
// type 字段低位设为 1 表示已转发（利用指针对齐保证低位为 0）
#define GC_FORWARDED_TAG ((MsType*)0x1)

static inline bool objIsForwarded(MsObject* obj) {
  return (uintptr_t)obj->type & 0x1;
}

// 转发指针存在 gcNext 字段（复用）
static inline MsObject* getForwardPtr(MsObject* obj) {
  return (MsObject*)obj->gcNext;  // 指向 to-space 的新地址
}

static void setForwardPtr(MsObject* from, MsObject* to) {
  from->gcNext = (MsObject*)to;   // gcNext 存新地址
  from->type   = GC_FORWARDED_TAG; // 标记已转发
}
```

### 2. Cheney BFS 复制

```c
void msMinorGC(void) {
  msStopAllWorkers();  // STW（单线程时为空操作）

  uint8_t* scanPtr = gYoung.toStart;  // BFS 扫描指针
  uint8_t* freePtr = gYoung.toStart;  // to-space 分配指针

  // 1. 复制根直接可达的年轻代对象
  msEnumerateRoots(copyRootRef, &freePtr);   // 包括 remembered set
  msEnumerateRememberedSet(copyRootRef, &freePtr);

  // 2. BFS：扫描已复制对象的引用，递归复制
  while (scanPtr < freePtr) {
    MsObject* obj = (MsObject*)scanPtr;
    size_t sz = msObjSize(obj);
    // 扫描 obj 的所有字段（通过 tpMark 的特化版本）
    obj->type->tpScan(obj, copyRef, &freePtr);
    scanPtr += ALIGN_UP(sz, 8);
  }

  // 3. swap from / to
  uint8_t* tmp = gYoung.fromStart;
  gYoung.fromStart = gYoung.toStart;
  gYoung.toStart   = tmp;
  gYoung.fromEnd   = gYoung.fromStart + gYoung.semiSize;
  gYoung.toEnd     = gYoung.toStart   + gYoung.semiSize;

  // 4. 重置所有 TLAB
  msResetAllTLABs();

  msResumeAllWorkers();
  gVM.gc.minorCount++;
}

// 复制单个对象到 to-space
static MsObject* copyObj(MsObject* obj, uint8_t** freePtr) {
  if (!isInYoung(obj)) return obj;    // 不在年轻代：不复制
  if (objIsForwarded(obj)) return getForwardPtr(obj);  // 已复制

  size_t sz = msObjSize(obj);
  MsObject* newObj = (MsObject*)(*freePtr);
  *freePtr += ALIGN_UP(sz, 8);
  memcpy(newObj, obj, sz);

  // 晋升判断：age 达到阈值 → 移到中代
  newObj->gcFlags = obj->gcFlags;
  uint8_t age = (obj->gcFlags >> 4) & 0x3;
  if (age >= 2) {
    OBJ_SET_GEN(newObj, GC_GEN_MIDDLE);
    msMidGenAdd(newObj);  // 加入中代链表
    *freePtr -= ALIGN_UP(sz, 8);  // 撤销在 to-space 的分配
    newObj = (MsObject*)msMidGenAlloc(sz);
    memcpy(newObj, obj, sz);
  } else {
    OBJ_SET_GEN(newObj, GC_GEN_YOUNG);
    newObj->gcFlags = (newObj->gcFlags & ~(0x3 << 4)) | ((age + 1) << 4);
  }

  setForwardPtr(obj, newObj);
  return newObj;
}
```

---

## 验收标准（checklist）

- [ ] Minor GC 后，from-space 清空，存活对象在 to-space。
- [ ] 转发指针正确：对 from-space 中死亡对象的引用被 NULL（无悬垂指针）。
- [ ] 老对象（age >= 2）晋升到中代，不在 to-space 中。
- [ ] Remembered set 中的跨代引用被正确枚举（T118 完成后测试）。
- [ ] Minor GC 时间 < 5ms（4MB young gen，1M 存活对象）。

---

## 测试用例（C 单测）

```c
// tests/test_minor_gc.c
void testMinorGcCollect(void) {
  // 分配大量临时对象，触发 Minor GC
  for (int i = 0; i < 1000000; i++) {
    MsObject* o = msGCAlloc(64, &msStrType);
    (void)o;  // 立即丢弃（死亡对象）
  }
  // GC 后 from-space 应被清空（freePtr 回到起点）
  MS_ASSERT(gVM.gc.minorCount > 0);
}
```

---

## Benchmark

```ms
// benchmarks/bench_minor_gc.ms
// 大量短期对象（触发频繁 Minor GC）
t0 := time.now()
for i in range(10_000_000) {
    x := [i, i*2, i*3]  // 分配后立即丢弃
}
t1 := time.now()
print("10M temp alloc:", t1-t0, "ms")
// 目标：含 GC 在内 < 2s（年轻代高效回收）
```

---

## 风险与边界

- **`tpScan` vs `tpMark`**：Minor GC 需要知道对象中每个 `MsValue` 字段的位置（精确引用）；`tpMark` 已有此信息；需为 Cheney 增加 `tpScan(obj, visitFn, freePtr)` 版本（访问并更新每个 MsValue 引用）。
- **栈帧的精确引用**：在 T117 完成前，Minor GC 需要保守扫描栈（将所有看起来像指针的值都认为是根）；T117 完成后改为精确根枚举。
