# mslang 垃圾回收器设计

## 1. 总体目标

| 目标 | 说明 |
|---|---|
| 精确追踪 | GC 知晓所有对象引用（非保守），可安全移动对象 |
| 分代 | 年轻代频繁收集（高回收率），老年代低频收集 |
| 年轻代半区复制 | 零碎片，分配 O(1)；存活对象晋升老年代 |
| 老年代增量/并发标记-清除 | 减少 STW 停顿 |
| 并行清扫/复制 | 利用多 OS 线程加速 GC 工作 |
| 与 VM 协作 | 安全点驱动，精确根来自 VM 栈帧 |

---

## 2. 内存空间布局

```
┌─────────────────────────────────────────────────────────────┐
│  年轻代 (Young Generation)                                  │
│  ┌──────────────┬──────────────┐                            │
│  │  from-space  │  to-space    │  各 4 MB（默认，可配置）   │
│  └──────────────┴──────────────┘                            │
├─────────────────────────────────────────────────────────────┤
│  老年代 (Old Generation)                                    │
│  ┌────────────────────────────┐                             │
│  │ 标记-清除堆（分块列表管理）│  初始 64 MB（默认，可扩张） │
│  └────────────────────────────┘                             │
├─────────────────────────────────────────────────────────────┤
│  大对象区 (Large Object Space)                              │
│  直接 mmap，单独追踪，免复制                                │
└─────────────────────────────────────────────────────────────┘
```

### 分代策略

- **年轻代**：绝大多数对象在此分配；存活超过 `YOUNG_TENURE_AGE`（默认 2 次收集）晋升老年代。
- **老年代**：长寿对象；增量/并发标记，并行清扫。
- **大对象**：`>= 32 KB` 直接分配到大对象区，用位图追踪，不参与复制。

---

## 3. 分配

### 3.1 年轻代 bump 分配器

```c
typedef struct {
    uint8_t *cursor;   // 当前分配指针
    uint8_t *limit;    // from-space 末尾
} Nursery;

MsObject *ms_alloc(MsVM *vm, MsType *type, size_t size) {
    size = ALIGN8(size);
    if (vm->nursery.cursor + size > vm->nursery.limit) {
        ms_collect_young(vm);   // 触发 Minor GC
        /* 若仍不足，扩张或 Major GC */
    }
    MsObject *obj = (MsObject*)vm->nursery.cursor;
    vm->nursery.cursor += size;
    obj->type     = type;
    obj->gc_flags = 0;          // 分代 = 0（年轻代）
    return obj;
}
```

bump 分配：无锁（每个 OS 线程或 mutator 有独立的 thread-local allocation buffer，TLAB）。

### 3.2 老年代空闲列表

老年代用**分块列表（free-list）+ 首次适配/最佳适配**分配，按大小类划分（8B、16B、32B … 4KB、其他）减少碎片。

---

## 4. Minor GC（年轻代半区复制）

触发条件：from-space 耗尽。

### 流程

```
1. STW（停止所有 mutator goroutine，协作式安全点）
2. 根枚举：
   a. 所有 goroutine 的 VM 栈帧（精确，逐 MsValue 槽）
   b. 全局变量表
   c. C API 句柄表 / 本地根栈
   d. remembered set 中指向年轻代的老年代引用
3. Cheney 广度优先复制：
   for each root r:
     copy_object(r)         // 若未复制则复制到 to-space，返回新地址；否则返回 forwarding ptr
   while scan_ptr < to-space.top:
     遍历当前对象的所有 MsValue 子字段（通过 type->traverse），递归 copy_object
     scan_ptr += obj_size
4. 更新所有根指针 → to-space 新地址
5. 年龄计数：age >= YOUNG_TENURE_AGE 的对象晋升老年代（在步骤 3 复制时判断）
6. 交换 from-space 与 to-space；重置 cursor
7. 重置 remembered set
8. Resume goroutines
```

### 转发指针（Forwarding Pointer）

对象复制后，**原 from-space 对象头**写入转发指针：

```c
// gc_flags 第 3 位表示 forwarded
// 若 gc_flags & GC_FORWARDED，则 obj->fwd（与 type 共用匿名 union）存 to-space 目标地址
```

任何通过 copy_object 到达已复制对象的路径直接返回目标地址，保证图一次遍历。

---

## 5. Major GC（老年代增量/并发标记-清除）

触发条件：老年代占用率 > 阈值（默认 75%）或 Minor GC 晋升失败。

### 5.1 三色标记（并发）

对象颜色：
- **白**：未访问，GC 结束时为垃圾
- **灰**：已发现但子字段未扫描（在灰色工作队列中）
- **黑**：已扫描（本身及所有子字段）

并发阶段：GC 线程与 mutator goroutine **并行**运行。

### 5.2 写屏障（Snapshot-at-the-beginning，SATB 或 Dijkstra）

采用 **Dijkstra 插入屏障**（较简单）：每当 mutator 将黑色对象的字段指向白色对象时，将白色对象涂灰：

```c
// 伪代码，编译器插入
void write_barrier(MsObject *obj, MsValue *field, MsValue new_val) {
    if (gc_concurrent_marking && IS_BLACK(obj) && IS_OBJ(new_val) && IS_WHITE(new_val.obj)) {
        mark_grey(new_val.obj);
    }
    *field = new_val;
}
```

VM 在 `STORE_LOCAL`/`STORE_ATTR`/`STORE_ITEM` 等写指令中插入屏障检查（当并发标记活跃时）。

### 5.3 阶段划分

| 阶段 | 是否 STW | 说明 |
|---|---|---|
| 初始标记 | 是（极短） | 仅标记直接 GC 根（栈根快照） |
| 并发标记 | 否 | GC 线程并发遍历对象图（写屏障保护） |
| 最终重标记 | 是（短） | 处理并发阶段产生的新引用（SATB 队列） |
| 并发清扫 | 否 | GC 线程将白色对象加回空闲列表 |

### 5.4 并行清扫

多 OS 线程划分老年代区域并行执行清扫，无锁（每线程负责独立区段）。

---

## 6. 分代写屏障（跨代引用）

当老年代对象引用年轻代对象时，该引用须记录进 **remembered set（card table）**，供 Minor GC 枚举根时使用。

```c
// card table：老年代按 512B 一个 card，脏标记用字节数组
void generational_write_barrier(MsObject *obj, MsValue new_val) {
    if (GEN(obj) > 0 && IS_OBJ(new_val) && GEN(new_val.obj) == 0) {
        MARK_CARD_DIRTY(card_table, obj);
    }
}
```

Minor GC 时只扫描脏 card 中的老年代对象，找出跨代引用作为年轻代根。

---

## 7. 安全点协作

所有 goroutine 在以下安全点检查 `vm->safepoint_requested`：

- 循环回边（`FOR_ITER`/反向 `JMP`）
- 函数调用前（`CALL`）
- `AWAIT`/`CHAN_RECV`/`CHAN_SEND`（已挂起，天然安全点）

进入安全点时，goroutine 将当前帧指针（精确根）上报给 GC，然后阻塞直到 GC 允许继续。

---

## 8. 精确根枚举

**VM 栈帧精确根**：`MsFrame.slots[0..slot_count-1]` 中每个 `MsValue`，若 `tag == MS_TAG_OBJ` 则为根。编译器为每个帧生成固定的 `slot_count`，无需保守扫描。

**C API 根**：通过句柄表与本地根栈管理（见 c-api.md），GC 直接遍历这两个结构。

**全局表根**：`MsVM.globals`（`MsMap`）中所有 value。

---

## 9. 大对象区

`>= 32 KB` 的对象（如大字符串、大 list 的 items 数组扩张）直接用 `mmap` 分配，用链表追踪，不参与复制 GC。Major GC 时参与标记-清除，`munmap` 回收。

**混合所有权**：list 本体（`MsList`）在年轻代，其 `items` 缓冲扩张超过 32 KB 后位于大对象区。Minor GC 复制 list 头时，仅更新 `items` 指针至其在大对象区的原地址（不复制缓冲），两者均纳入根追踪，不存在悬空。

---

## 10. 可终结对象（`__del__`）

实现了 `__del__` 的对象在 GC 确认无引用后，不立即回收，而是加入终结队列。
GC 恢复 mutator 后，终结线程调用 `__del__`，之后对象才真正回收。

**不保证及时性**：不应在 `__del__` 中依赖资源的即时释放（用 `with`/`try/finally` 代替）。

---

## 11. GC 参数与调优

| 参数 | 默认值 | 说明 |
|---|---|---|
| `gc.young_size` | 4 MB | 单个半区大小 |
| `gc.old_initial` | 64 MB | 老年代初始大小 |
| `gc.tenure_age` | 2 | 晋升年龄阈值 |
| `gc.major_threshold` | 0.75 | 老年代占用触发比例 |
| `gc.large_obj_threshold` | 32 KB | 大对象阈值 |
| `gc.parallel_workers` | CPU 核数 - 1 | 并行 GC 线程数 |

脚本可通过 `gc` 内置模块查询与调整：

```ms
import gc
gc.collect()          // 强制 Minor + Major GC
gc.disable()          // 禁用自动 GC（测试用）
stats := gc.stats()   // 返回统计 map
```
