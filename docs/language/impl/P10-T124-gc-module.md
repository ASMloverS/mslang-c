# P10-T124 `gc` 内置模块（collect / disable / stats）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `gc` 内置模块，向脚本层暴露 GC 控制接口：手动触发 GC、禁用/启用 GC、查询 GC 统计信息、控制分代阈值。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T119 | Major GC |
| P10-T120 | 增量 GC |
| P7-T090 | 内置模块注册 |

---

## 实现要点

### 1. gc 模块 API

```c
// gc.collect([generation])   → 触发 GC（0=Minor，1=Major）
static MsValue gcCollect(MsThread* t, MsValue* args, int argc) {
  int gen = (argc >= 1 && MS_IS_INT(args[0])) ? (int)MS_AS_INT(args[0]) : 1;
  if (gen == 0) msMinorGC();
  else          msMajorGC();
  return MS_NIL_VAL;
}

// gc.disable() → 禁用自动 GC
static MsValue gcDisable(MsThread* t, MsValue* args, int argc) {
  gVM.gc.enabled = false;
  return MS_NIL_VAL;
}

// gc.enable() → 启用自动 GC
static MsValue gcEnable(MsThread* t, MsValue* args, int argc) {
  gVM.gc.enabled = true;
  return MS_NIL_VAL;
}

// gc.enable_incremental() → 启用增量模式
static MsValue gcEnableIncremental(MsThread* t, MsValue* args, int argc) {
  gVM.gc.incrementalEnabled = true;
  return MS_NIL_VAL;
}

// gc.stats() → map 类型，包含 GC 统计
static MsValue gcStats(MsThread* t, MsValue* args, int argc) {
  MsValue d = msNewMap();
  msMapSetStr(d, "minorCount",   MS_INT_VAL(gVM.gc.minorCount));
  msMapSetStr(d, "majorCount",   MS_INT_VAL(gVM.gc.majorCount));
  msMapSetStr(d, "bytesAlloc",   MS_INT_VAL(gVM.gc.bytesAlloc));
  msMapSetStr(d, "numObjects",   MS_INT_VAL(gVM.gc.numObjects));
  msMapSetStr(d, "youngSize",    MS_INT_VAL(gYoung.semiSize));
  msMapSetStr(d, "midSize",      MS_INT_VAL(gMidGen.bytesUsed));
  return d;
}

// gc.set_threshold(minor_interval, mid_threshold)
static MsValue gcSetThreshold(MsThread* t, MsValue* args, int argc) {
  if (argc >= 1) gMidGen.majorInterval    = (uint32_t)MS_AS_INT(args[0]);
  if (argc >= 2) gMidGen.threshold        = (size_t)MS_AS_INT(args[1]);
  return MS_NIL_VAL;
}

// gc.is_enabled() → bool
// gc.get_count() → (minor, major) tuple
// gc.get_objects() → list（调试用，慎用）
```

### 2. 注册

```c
static MsCFunctionDef gcFuncs[] = {
  { "collect",           gcCollect,          -1 },
  { "disable",           gcDisable,           0 },
  { "enable",            gcEnable,            0 },
  { "enable_incremental",gcEnableIncremental, 0 },
  { "stats",             gcStats,             0 },
  { "set_threshold",     gcSetThreshold,     -1 },
  { NULL }
};

MsBuiltinModuleDef msGcModuleDef = { .name = "gc", .funcs = gcFuncs };
```

---

## 验收标准（checklist）

- [ ] `import gc; gc.collect()` → 触发 Major GC，不崩溃。
- [ ] `gc.disable()` 后，不触发自动 GC（对象不被回收）。
- [ ] `gc.enable()` 恢复自动 GC。
- [ ] `gc.stats()` 返回有意义的数值（minorCount 递增）。
- [ ] `gc.set_threshold(4, 8MB)` 改变 GC 触发阈值。

---

## 测试用例（.ms）

```ms
import gc

// 禁用 GC 后手动触发
gc.disable()
for i in range(100_000) {
    x := str(i)  // 临时对象，不被 GC 回收（已禁用）
}
s1 := gc.stats()
print("before collect:", s1["bytesAlloc"])

gc.collect()  // 手动触发
s2 := gc.stats()
print("after collect:", s2["bytesAlloc"])
// after < before（大量临时对象被回收）

gc.enable()   // 恢复自动 GC

// 统计
s := gc.stats()
print("minor GC count:", s["minorCount"])
print("major GC count:", s["majorCount"])
```

---

## Benchmark

N/A（gc 模块是控制接口，不在热路径）。

---

## 风险与边界

- **`gc.get_objects()`**：收集所有存活对象的列表，用于内存泄漏分析；因遍历完整 GC 链表，在大型应用中可能慢（文档说明仅用于调试）。
