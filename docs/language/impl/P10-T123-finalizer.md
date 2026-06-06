# P10-T123 `__del__` 终结 + 复活

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `__del__` 终结器（finalizer）：当 GC 准备回收一个对象时，先调用其 `__del__` 方法；若 `__del__` 将对象重新赋值给全局变量（复活），则此次不回收（下次 GC 再判断）。语义对齐 Python `__del__`。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T119 | Major GC（回收发生点） |
| P5-T074 | 魔术方法分派 |

---

## 实现要点

### 1. 终结器队列

```c
// GC 标记阶段结束后，在清扫之前：
// 扫描中/老代，找出将被回收但有 __del__ 的对象
// 加入终结器队列（先不释放，等 __del__ 执行完再判断）

typedef struct MsFinalizerQueue {
    MsObject** objects;
    uint32_t   count, cap;
} MsFinalizerQueue;

MsFinalizerQueue gFinalizerQ = {0};

// GC 清扫前：找出需要终结的对象
void msCollectFinalizable(void) {
    MsObject* obj = gMidGen.allObjects;
    while (obj) {
        MsObject* next = obj->gcNext;
        if ((obj->gcFlags & 0x03) == GC_WHITE) {
            // 即将回收：检查是否有 __del__
            if (MS_IS_OBJ(MS_OBJ_VAL(obj)) && obj->type == &msInstanceType) {
                MsValue del = msTypeLookupMethodMRO(
                    ((MsInstanceObj*)obj)->klass, "__del__");
                if (!MS_IS_NIL(del)) {
                    // 暂时复活：加入终结器队列
                    obj->gcFlags = (obj->gcFlags & ~0x03) | GC_GRAY;  // 临时灰色
                    finalizerQueuePush(&gFinalizerQ, obj);
                }
            }
        }
        obj = next;
    }
}
```

### 2. 终结器执行

```c
// 在 GC 完成（或在安全点）执行终结器
void msRunFinalizers(void) {
    for (uint32_t i = 0; i < gFinalizerQ.count; i++) {
        MsObject* obj = gFinalizerQ.objects[i];
        MsValue self = MS_OBJ_VAL(obj);

        // 调用 __del__
        MsValue del = msTypeLookupMethodMRO(
            ((MsInstanceObj*)obj)->klass, "__del__");
        MsValue result = msCallFn(gVM.mainThread, del, &self, 1);
        if (MS_IS_ERROR(result)) {
            // __del__ 内异常：打印到 stderr，吞掉
            msPrintError(stderr, gVM.mainThread.currentException);
            gVM.mainThread.hasException = false;
        }

        // 检查复活：若 obj 现在可达（gcFlags != WHITE），不回收
        if ((obj->gcFlags & 0x03) != GC_WHITE) {
            // 复活成功：obj 在 __del__ 中被重新引用了
            // 标记为不可终结（避免下次 GC 再次调用 __del__）
            obj->gcFlags |= GC_NO_FINALIZER;  // 特殊位
        } else {
            // 仍不可达：正常释放
            if (obj->type->tp_free) obj->type->tp_free(obj);
            msFree(obj);
        }
    }
    gFinalizerQ.count = 0;
}
```

### 3. 复活语义

```ms
// __del__ 将 self 赋值给全局变量 → 复活
class Resource {
    func __del__(self) {
        print("finalizing:", self.name)
        Resurrection.saved = self  // 复活！
    }
}
class Resurrection {}
```

---

## 验收标准（checklist）

- [ ] 有 `__del__` 的对象在 GC 前调用 `__del__`。
- [ ] `__del__` 执行后对象仍不可达 → 正常释放。
- [ ] `__del__` 中复活对象 → 本次不释放，下次 GC 再判断（无 `__del__` 重复调用）。
- [ ] `__del__` 内异常 → 打印到 stderr，不影响 GC 流程。
- [ ] 无 `__del__` 的对象 GC 行为不变（无额外开销）。

---

## 测试用例（.ms）

```ms
closed := false

class Conn {
    func __init__(self, name) { self.name = name }
    func __del__(self) {
        closed = true
        print("closed:", self.name)
    }
}

c := Conn("db")
del c

import gc
gc.collect()
print(closed)  // true（__del__ 已调用）
```

```ms
// 复活测试
class Ghost {
    saved := nil   // 类属性

    func __del__(self) {
        print("haunting!")
        Ghost.saved = self  // 复活！
    }
}

g := Ghost()
del g
import gc
gc.collect()
print(Ghost.saved != nil)  // true（复活）

del Ghost.saved
gc.collect()
print("finally gone")   // 第二次 GC，无 __del__，正常释放
```

---

## Benchmark

N/A（终结器是罕见路径，不需要 benchmark）。

---

## 风险与边界

- **循环引用 + `__del__`**：若有 `__del__` 的对象参与引用循环，Python 无法自动解决（需要弱引用或手动 break cycle）；初版同样不解决此问题，文档说明。
- **`__del__` 调用时机**：不保证在任何特定时间调用（GC 触发时），与 Python 一致；不建议在 `__del__` 中做重要资源管理（推荐 `with` 语句）。
