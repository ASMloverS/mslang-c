# P5-T074 魔术方法分派（__add__ / __len__ / __iter__ 等）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现用户定义类的魔术方法（dunder methods）分派：当 VM 执行算术/比较/容器操作时，优先查找实例所属类的对应魔术方法并调用。将 Python 风格的运算符重载接入 mslang 类系统。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T073 | MRO 查找 |
| P5-T072 | 实例对象 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §19 魔术方法表 |

---

## 实现要点

### 1. 魔术方法与类型槽映射表

在 `instanceGetAttr` 和 `BINARY_OP` 宏中，若操作数是 `MsInstanceObj`，优先调用对应的魔术方法：

| 操作 / 槽 | 魔术方法 | 反向方法 |
|---|---|---|
| `tp_add` | `__add__` | `__radd__` |
| `tp_sub` | `__sub__` | `__rsub__` |
| `tp_mul` | `__mul__` | `__rmul__` |
| `tp_div` | `__div__` | `__rdiv__` |
| `tp_mod` | `__mod__` | `__rmod__` |
| `tp_pow` | `__pow__` | `__rpow__` |
| `tp_neg` | `__neg__` | — |
| `tp_bitnot` | `__invert__` | — |
| `tp_eq` | `__eq__` | — |
| `tp_lt` | `__lt__` | `__gt__`（反向） |
| `tp_len` | `__len__` | — |
| `tp_iter` | `__iter__` | — |
| `tp_next` | `__next__` | — |
| `tp_call` | `__call__` | — |
| `tp_getitem` | `__getitem__` | — |
| `tp_setitem` | `__setitem__` | — |
| `tp_contains` | `__contains__` | — |
| `tp_hash` | `__hash__` | — |
| `tp_repr` | `__repr__` | — |
| `tp_str` | `__str__` | — |
| `tp_bool` | `__bool__` | — |

### 2. 实例类型槽动态查找

```c
// 实例的 tp_add：查找 __add__ 方法
static MsValue instanceAdd(MsValue a, MsValue b) {
    MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(a);
    MsValue method = msTypeLookupMethodMRO(inst->klass, msNewStr("__add__", 7));
    if (!MS_IS_NIL(method)) {
        MsValue args[2] = {a, b};
        return msCallFn(&gVM.mainThread, method, args, 2);
    }
    // 尝试 b 的 __radd__
    if (MS_IS_OBJ(b) && MS_AS_OBJ(b)->type == &msInstanceType) {
        MsInstanceObj* bi = (MsInstanceObj*)MS_AS_OBJ(b);
        MsValue radd = msTypeLookupMethodMRO(bi->klass, msNewStr("__radd__", 8));
        if (!MS_IS_NIL(radd)) {
            MsValue args[2] = {b, a};
            return msCallFn(&gVM.mainThread, radd, args, 2);
        }
    }
    return MS_ERROR_VALUE;  // TypeError
}
```

所有实例的 `tp_*` 槽都设置为类似上述的"动态查找 + 调用方法"版本；可以用宏简化：

```c
#define INSTANCE_BINARY_SLOT(slot, name, rname) \
static MsValue instance_##slot(MsValue a, MsValue b) { \
    MsInstanceObj* i = (MsInstanceObj*)MS_AS_OBJ(a); \
    MsValue m = msTypeLookupMethodMRO(i->klass, msInternStr(name)); \
    if (!MS_IS_NIL(m)) { MsValue args[2]={a,b}; return msCallFn(&gVM.mainThread,m,args,2); } \
    /* try radd */ return MS_ERROR_VALUE; \
}
```

### 3. `__bool__` 与 `__len__` 用于真值测试

```c
// msValueTruthy 对 MsInstanceObj 的处理
case MS_TAG_OBJ:
    if (obj->type == &msInstanceType) {
        MsInstanceObj* inst = (MsInstanceObj*)obj;
        // 1. 查找 __bool__
        MsValue boolM = msTypeLookupMethodMRO(inst->klass, msInternStr("__bool__"));
        if (!MS_IS_NIL(boolM)) {
            MsValue r = msCallFn(&gVM.mainThread, boolM, &MS_OBJ_VAL(inst), 1);
            return msValueTruthy(r);
        }
        // 2. 查找 __len__（len==0 → false）
        MsValue lenM = msTypeLookupMethodMRO(inst->klass, msInternStr("__len__"));
        if (!MS_IS_NIL(lenM)) {
            MsValue r = msCallFn(&gVM.mainThread, lenM, &MS_OBJ_VAL(inst), 1);
            return MS_AS_INT(r) != 0;
        }
        return true;  // 默认：实例为真
    }
```

---

## 验收标准（checklist）

- [ ] `class Vec { func __add__(self, o) { return Vec(self.x+o.x, self.y+o.y) } }; Vec(1,2) + Vec(3,4)` → Vec(4,6)。
- [ ] `class MyList { func __len__(self) { return len(self.data) } }; len(MyList())` → 调用 `__len__`。
- [ ] `class Iter { ... }` 实现 `__iter__`/`__next__` → 可被 `for` 循环迭代。
- [ ] `class Num { func __bool__(self) { return self.val != 0 } }; if Num(0) { }` → 不进入 body。
- [ ] `a + b`（a 无 __add__，b 有 __radd__）→ 调用 b.__radd__(a)。
- [ ] `__repr__`/`__str__` 被 `repr()`/`str()` 调用。

---

## 测试用例（.ms）

```ms
class Vector {
    func __init__(self, x, y) { self.x = x; self.y = y }
    func __add__(self, other) { return Vector(self.x+other.x, self.y+other.y) }
    func __mul__(self, n) { return Vector(self.x*n, self.y*n) }
    func __repr__(self) { return $"Vector({self.x}, {self.y})" }
    func __eq__(self, other) { return self.x == other.x and self.y == other.y }
    func __len__(self) { return 2 }
}

v1 := Vector(1, 2)
v2 := Vector(3, 4)
print(v1 + v2)      // Vector(4, 6)
print(v1 * 3)       // Vector(3, 6)
print(v1 == Vector(1, 2))  // true
print(len(v1))      // 2
```

---

## Benchmark

N/A（魔术方法调用路径与普通方法调用相同，性能归入 T068 bench）。

---

## 风险与边界

- **热路径开销**：每次 `+`/`-` 操作若涉及用户定义类，都要走 MRO 查找（哈希表查找 + 字符串比较）；对性能敏感代码影响显著。可用**类型缓存**（inline cache）优化，初版不实现。
- **`__eq__` 与 hash 一致性**：若定义了 `__eq__` 而未定义 `__hash__`，实例默认不可哈希（Python 规则）；初版可不强制，文档提示。
