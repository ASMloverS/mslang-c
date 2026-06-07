# P12-T194 stdlib: weakref

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `weakref` 模块（对齐 `stdlib/weakref.md`）：弱引用支持，不阻止 GC 回收，用于缓存和避免循环引用内存泄漏。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T115 | 分代 GC（弱引用清除集成到 GC 流程） |
| P10-T123 | 终结器（弱引用回调类似 finalizer） |

---

## API 清单

```ms
// 弱引用
ref := weakref.ref(obj, callback=nil)
// callback(ref)：obj 被 GC 回收前调用（接收弱引用本身）

live_obj := ref()      // 调用弱引用 → 返回 obj 或 nil（若已回收）

// WeakValueDictionary（值为弱引用的字典）
d := weakref.WeakValueDictionary()
d["key"] = some_object
d["key"]          // → object 或 KeyError（若已回收）
list(d.keys())    // 仅返回仍存活的键

// WeakKeyDictionary（键为弱引用的字典）
d := weakref.WeakKeyDictionary()
d[some_object] = value
// some_object 被回收时，对应条目自动删除

// WeakSet（元素为弱引用的集合）
s := weakref.WeakSet()
s.add(obj)
len(s)         // 仅计活跃元素
obj in s       // 检查（obj 活跃时）

// finalize（析构回调，比 weakref.ref + callback 更安全）
f := weakref.finalize(obj, func, *args, **kwargs)
// 当 obj 被回收时调用 func(*args, **kwargs)
f.alive → bool       // obj 是否仍存活
f.peek() → (obj,args,kwargs) | nil
f.detach()  → (obj,args,kwargs) | nil  // 取消注册（返回 obj 弱引用）
f()          // 手动触发并注销
```

---

## 实现要点

```c
// 弱引用核心：MsWeakRefObj
// 包含：指向目标对象的指针（不增加引用计数）
// 目标对象的 MsObject.weakrefs 链表：所有指向它的弱引用

typedef struct MsWeakRefObj {
  MsObject      header;
  MsObject*     target;    // 目标对象（GC 回收时清零）
  MsWeakRefObj* nextRef;   // 同一目标的弱引用链表
  MsValue       callback;  // nil 或 callable
} MsWeakRefObj;

// MsObject 扩展：
// MsWeakRefObj* weakrefs;  // 弱引用链表头（默认 nil）

// GC 流程修改（各代 GC）：
// 标记阶段：弱引用对象**不**使目标对象被标记为可达
// 清除阶段：遍历未标记对象的 weakrefs 链表 → 将每个 weakref.target 置 nil
// 在对象释放前：执行 weakref.callback（若有）
// 对象释放后：弱引用返回 nil（已置零）

// WeakValueDictionary：
// 内部存 WeakRef，访问时调用 ref() 并在 nil 时删除条目
// 弱引用 callback：自动删除 dict 中对应键

// finalize 实现：
// finalize(obj, f, *args) ≈ weakref.ref(obj, lambda r: f(*args))
// 但额外处理：f 只执行一次，atexit 时也触发（alive 对象）
// MsFinalizeObj：管理注册表，atexit 钩子

// WeakSet：
// 内部是 WeakValueDictionary（id(obj)→ref）
```

---

## 验收标准（checklist）

- [ ] 弱引用目标 GC 后，`ref()` 返回 nil。
- [ ] `weakref.ref(obj, callback)` 在对象回收时调用 callback。
- [ ] WeakValueDictionary 自动删除已回收对象的条目。
- [ ] WeakKeyDictionary 键回收后条目消失。
- [ ] `finalize` 在对象回收时正确触发，不会触发两次。
- [ ] 循环引用 A→B→A 中都是弱引用时，GC 可以回收（不泄漏）。

---

## 测试用例（.ms）

```ms
import weakref, gc

// 基础弱引用
class Obj: pass
obj := Obj()
ref := weakref.ref(obj)
print(ref() is obj)   // true（obj 存活）

del obj
gc.collect()           // 强制 GC
print(ref())           // nil（已回收）

// 带 callback
notified := [false]
def on_death(r) { notified[0] = true }

obj2 := Obj()
r2 := weakref.ref(obj2, on_death)
del obj2
gc.collect()
print(notified[0])     // true

// WeakValueDictionary
d := weakref.WeakValueDictionary()
obj3 := Obj()
d["x"] = obj3
print("x" in d)   // true
del obj3
gc.collect()
print("x" in d)   // false（自动删除）

// finalize
log := []
obj4 := Obj()
f := weakref.finalize(obj4, lambda: log.append("dead"))
print(f.alive)   // true
del obj4
gc.collect()
print(log)       // ["dead"]
print(f.alive)   // false
```
