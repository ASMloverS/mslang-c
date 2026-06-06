# P10-T119 中代标记-清除 + 晋升

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现中代（Middle Generation）GC：中代对象经历过 Minor GC 晋升，使用标记-清除算法回收；在每 N 次 Minor GC 后触发一次 Major GC（中代+老代）；中代存活对象晋升到老代。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T116 | Minor GC（晋升到中代的逻辑） |
| P10-T117 | 精确根枚举 |
| P10-T118 | 写屏障（跨代引用） |

---

## 实现要点

### 1. 中代布局

```c
// 中代：链式分配（free list）
// 对象头双链接：allObjects 链（用于 mark-sweep）
typedef struct MsMidGen {
    MsObject* allObjects;   // 链表头（mark-sweep 用）
    size_t    bytesUsed;
    size_t    threshold;    // 触发 Major GC 的阈值
    uint32_t  minorsSinceLastMajor;
    uint32_t  majorInterval;  // 默认 8（每 8 次 Minor GC 触发一次 Major GC）
} MsMidGen;

MsMidGen gMidGen = { .threshold = 16 * 1024 * 1024, .majorInterval = 8 };
```

### 2. Major GC（中代 + 老代）触发时机

```c
// 在每次 Minor GC 末尾检查：
void msMinorGC(void) {
    // ... Minor GC 逻辑 ...
    gMidGen.minorsSinceLastMajor++;
    if (gMidGen.minorsSinceLastMajor >= gMidGen.majorInterval ||
        gMidGen.bytesUsed > gMidGen.threshold) {
        gMidGen.minorsSinceLastMajor = 0;
        msMajorGC();  // 触发 Major GC
    }
}
```

### 3. 中代 Mark-Sweep

```c
// 标记阶段：从根出发，递归标记所有可达对象
void msMajorGC(void) {
    msStopAllWorkers();

    // 标记：重用 T050 的 markObject（WHITE/GRAY/BLACK 颜色）
    // 但此时枚举根包含年轻代（已完全由 Minor GC 处理），只枚举中/老代
    gVM.gc.markPhase = true;
    msEnumerateRoots(markRootVisitor, NULL);
    while (!grayQueueEmpty()) {
        MsObject* obj = grayQueuePop();
        obj->type->tp_mark(obj);  // 标记所有引用的子对象
        obj->gcFlags |= GC_BLACK;
    }

    // 清扫中代：释放 WHITE 对象，晋升存活次数多的对象到老代
    sweepMidGen();
    sweepOldGen();  // T120 完成后改为增量

    msResumeAllWorkers();
    gVM.gc.majorCount++;
}

void sweepMidGen(void) {
    MsObject** p = &gMidGen.allObjects;
    while (*p) {
        MsObject* obj = *p;
        if ((obj->gcFlags & GC_BLACK) == 0) {
            // 白色：不可达，释放
            *p = obj->gcNext;
            if (obj->type->tp_free) obj->type->tp_free(obj);
            msFree(obj);
            gMidGen.bytesUsed -= msObjSize(obj);
        } else {
            // 存活：晋升年龄
            obj->gcFlags &= ~GC_BLACK;  // 重置为 WHITE（为下次标记）
            uint8_t age = (obj->gcFlags >> 4) & 0x3;
            if (age >= 3) {
                // 晋升到老代
                promoteToOld(obj);
            } else {
                obj->gcFlags = (obj->gcFlags & ~(0x3 << 4)) | ((age + 1) << 4);
                p = &obj->gcNext;
            }
        }
    }
}
```

---

## 验收标准（checklist）

- [ ] 经历 2 次以上 Minor GC 的对象晋升到中代链表。
- [ ] `msMajorGC` 后，中代中不可达对象被释放。
- [ ] 晋升年龄 >= 3 的中代对象移入老代（T120 的老代链表）。
- [ ] Major GC 后内存使用量正确减少（无内存泄漏）。
- [ ] 每 8 次 Minor GC 自动触发 Major GC。

---

## 测试用例（.ms）

```ms
// 生命周期较长的对象最终晋升
import gc
gc.enable()

long_lived := []
for i in range(1000) {
    long_lived.append(str(i))  // 会经历 Minor GC 晋升
}
gc.collect()
print("survived:", len(long_lived))  // 1000（全部存活）
```

---

## Benchmark

```ms
// benchmarks/bench_major_gc.ms
// 长期存活对象 + 短期死亡对象混合
import time
survivors := []
t0 := time.now()
for i in range(10_000_000) {
    x := str(i)          // 短期对象（Minor GC 回收）
    if i % 100 == 0 {
        survivors.append(x)  // 长期对象（晋升）
    }
}
t1 := time.now()
print("mixed alloc:", t1-t0, "ms")
print("survivors:", len(survivors))
// 目标：< 5s（含多次 Minor + Major GC）
```

---

## 风险与边界

- **中代内存分配器**：中代使用 free list 而非 bump pointer（不能移动对象）；初版用系统 `malloc`/`free` + 链表，后续可引入 size-class 分配器。
- **Major GC 的 STW**：Major GC 目前仍是 STW；T120 将引入增量/并发老代 GC，减少停顿时间。
