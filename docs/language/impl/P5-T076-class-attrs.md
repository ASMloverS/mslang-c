# P5-T076 类属性 vs 实例属性

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现类属性（存储在类的 `attrs` 字典而非实例的 `attrs` 字典）与实例属性的查找顺序：实例属性优先，其次类属性（沿 MRO 查找），与 Python 语义一致。支持类属性作为默认值（所有实例共享）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T073 | MRO + 方法绑定 |
| P5-T072 | 实例属性 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §21 类属性与实例属性 |

---

## 实现要点

### 1. 类属性字典

```c
typedef struct MsTypeObj {
  // ...（现有字段）
  MsObject* classAttrs;  // MsMapObj*（类属性，非方法）
} MsTypeObj;
```

class body 中的非函数赋值（`x := 1`）→ 类属性，编译为 class body 执行语句。

### 2. 属性查找顺序（`instanceGetAttr` 扩展）

```c
static MsValue instanceGetAttr(MsValue v, MsValue name) {
  MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(v);
  // 1. 实例属性（最高优先级）
  MsValue attr = msMapGet(MS_OBJ_VAL(inst->attrs), name);
  if (!MS_IS_NIL(attr)) return attr;

  // 2. 沿 MRO 查找类方法和类属性
  for (uint32_t i = 0; i < inst->klass->mroLen; i++) {
    MsTypeObj* tp = (MsTypeObj*)inst->klass->mro[i];
    // 2a. 方法（methods 字典）
    if (tp->methods) {
      MsValue m = msMapGet(MS_OBJ_VAL(tp->methods), name);
      if (!MS_IS_NIL(m)) return msNewBoundMethod(m, v);
    }
    // 2b. 类属性（classAttrs 字典）
    if (tp->classAttrs) {
      MsValue ca = msMapGet(MS_OBJ_VAL(tp->classAttrs), name);
      if (!MS_IS_NIL(ca)) return ca;
    }
  }
  return MS_NIL_VAL;  // → AttributeError
}
```

### 3. 实例属性设置不影响类属性

```c
// inst.x = val → 总是写入实例 attrs，不修改类属性
static MsValue instanceSetAttr(MsValue v, MsValue* args, int argc) {
  MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(v);
  msMapSet(MS_OBJ_VAL(inst->attrs), args[0], args[1]);
  return MS_NIL_VAL;
}
```

---

## 验收标准（checklist）

- [ ] 类属性被所有实例共享（未被覆盖时）。
- [ ] 实例属性覆盖同名类属性（实例优先）。
- [ ] 修改实例属性不影响其他实例或类属性。
- [ ] 类属性可通过类名直接访问（`ClassName.attr`）。
- [ ] 类属性修改影响所有未覆盖的实例（Python 语义）。

---

## 测试用例（.ms）

```ms
class Counter {
    count := 0   // 类属性（所有实例共享）

    func __init__(self) {
        // 未覆盖 count → 使用类属性
    }
    func inc(self) {
        Counter.count += 1   // 修改类属性
    }
}

c1 := Counter()
c2 := Counter()
c1.inc()
c1.inc()
print(c1.count)   // 2（类属性）
print(c2.count)   // 2（共享）

c1.count = 99     // 创建实例属性（覆盖类属性）
print(c1.count)   // 99（实例属性）
print(c2.count)   // 2（仍然是类属性）
print(Counter.count)  // 2（类属性未变）
```

---

## 风险与边界

- **类属性 vs 方法**：方法（`MsClosureObj`）存储在 `methods` 字典，类属性（非函数值）存储在 `classAttrs` 字典。
- **class body 执行**：class body 内的非函数语句在 `OP_MAKE_CLASS` 执行时运行（Python 语义：class body 是一个代码块执行环境）；初版实现为在 `OP_MAKE_CLASS` 之前编译并执行 class body 的非方法语句。
