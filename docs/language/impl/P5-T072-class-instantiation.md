# P5-T072 类实例化 / __init__ / 实例属性

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现用户定义类的实例化：`OP_MAKE_CLASS`（创建 `MsType` 对象）、`Class(args)` 调用（创建 `MsInstanceObj`，调用 `__init__`）、实例属性的读写（`self.attr`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T068 | 调用约定 |
| P3-T044 | `OP_MAKE_CLASS` 编译 |
| P4-T060 | map（实例 attrs 字典） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §3 MsType（实例化流程） |
| `type-system.md` | §17 MsInstanceObj |
| `vm.md` | §5 OP_MAKE_CLASS 语义 |

---

## 待实现（C 文件）

```
src/runtime/ms_class.c     # MsTypeObj / MsInstanceObj + tp_call / tp_getattr
include/mslang/ms_class.h  # msNewType / msNewInstance
```

---

## 实现要点

### 1. MsTypeObj（运行时用户定义类对象）

```c
// 注意：内置类型使用静态 MsType；用户定义类用 MsTypeObj（GC 管理）
typedef struct MsTypeObj {
  MsObject  header;      // type 的 type 是 &msMetaType
  MsType    mstype;      // 嵌入 MsType 结构（方法查找用）
  MsObject* methods;    // MsMapObj*（方法字典）
  MsObject** mro;       // MsType* 数组（T073 计算）
  uint32_t   mroLen;
} MsTypeObj;
```

### 2. MsInstanceObj（实例对象）

```c
typedef struct MsInstanceObj {
  MsObject  header;
  MsTypeObj* klass;    // 所属类（不经过 GC 直接存指针）
  MsObject*  attrs;    // MsMapObj*（实例属性字典）
} MsInstanceObj;
```

### 3. OP_MAKE_CLASS 实现

```c
case OP_MAKE_CLASS: {
  uint16_t nameIdx    = READ_U16();
  uint8_t  methodCount = READ_BYTE();
  uint8_t  baseCount  = READ_BYTE();

  // 从栈弹出基类
  MsType** bases = msAlloc(baseCount * sizeof(MsType*));
  for (int i = baseCount - 1; i >= 0; i--) {
    MsValue base = POP();
    if (MS_IS_OBJ(base) && MS_AS_OBJ(base)->type == &msMetaType) {
      bases[i] = &((MsTypeObj*)MS_AS_OBJ(base))->mstype;
    } else {
      return msTypeError(t, "base must be a class");
    }
  }

  MsTypeObj* tp = (MsTypeObj*)msGCAlloc(&msMetaType, sizeof(*tp));
  MsStrObj*  name = (MsStrObj*)MS_AS_OBJ(frame->chunk->consts[nameIdx]);
  tp->mstype.name  = name->data;
  tp->mstype.tpCall    = typeCall;      // 实例化
  tp->mstype.tpGetattr = typeGetAttr;   // 查找方法

  // 构建方法字典
  tp->methods = MS_AS_OBJ(msNewMap(methodCount * 2));
  for (int i = 0; i < methodCount; i++) {
    uint16_t mNameIdx = READ_U16();
    uint16_t mFuncIdx = READ_U16();
    MsValue mName = frame->chunk->consts[mNameIdx];
    MsValue mFunc = frame->chunk->consts[mFuncIdx];
    msMapSet(MS_OBJ_VAL(tp->methods), mName, mFunc);
  }

  // T073 计算 MRO
  msFree(bases);
  PUSH(MS_OBJ_VAL(tp));
  DISPATCH();
}
```

### 4. 类的 tp_call（实例化）

```c
static MsValue typeCall(MsValue self, MsValue* args, int argc) {
  MsTypeObj* tp = (MsTypeObj*)MS_AS_OBJ(self);

  // 1. 分配实例
  MsInstanceObj* inst = (MsInstanceObj*)msGCAlloc(&msInstanceType, sizeof(*inst));
  inst->klass = tp;
  inst->attrs = MS_AS_OBJ(msNewMap(4));

  // 2. 查找并调用 __init__
  MsValue initMethod = msMapGet(MS_OBJ_VAL(tp->methods), msNewStr("__init__", 8));
  if (!MS_IS_NIL(initMethod)) {
    // 构建 self + args 的参数列表
    MsValue callArgs[256];
    callArgs[0] = MS_OBJ_VAL(inst);
    memcpy(callArgs + 1, args, argc * sizeof(MsValue));
    MsValue r = msCallFn(&gVM.mainThread, initMethod, callArgs, argc + 1);
    if (MS_IS_ERROR(r)) return r;
  }

  return MS_OBJ_VAL(inst);
}
```

### 5. 实例属性读写（tp_getattr / tp_setattr）

```c
// 实例的 tpGetattr：先查实例 attrs，再查类的 methods（T073）
static MsValue instanceGetAttr(MsValue v, MsValue name) {
  MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(v);
  // 1. 实例属性优先
  MsValue attr = msMapGet(MS_OBJ_VAL(inst->attrs), name);
  if (!MS_IS_NIL(attr)) return attr;
  // 2. 类方法查找（T073 MRO）
  MsValue method = msTypeLookupMethodMRO(inst->klass, name);
  if (!MS_IS_NIL(method)) return msNewBoundMethod(method, v);
  return MS_NIL_VAL;
}

static MsValue instanceSetAttr(MsValue v, MsValue* args, int argc) {
  MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(v);
  // args[0]=name, args[1]=val
  msMapSet(MS_OBJ_VAL(inst->attrs), args[0], args[1]);
  return MS_NIL_VAL;
}
```

---

## 验收标准（checklist）

- [ ] `class Foo {}; Foo()` → 创建 MsInstanceObj，类型为 Foo。
- [ ] `class Foo { func __init__(self, x) { self.x = x } }; f = Foo(42); f.x` → 42。
- [ ] 实例属性可读写：`f.x = 100; f.x` → 100。
- [ ] 未定义属性 → AttributeError。
- [ ] `type(Foo())` → "Foo"。
- [ ] GC：实例被正确 mark（attrs 字典 + klass）。

---

## 测试用例（.ms）

```ms
class Point {
    func __init__(self, x, y) {
        self.x = x
        self.y = y
    }
    func distance(self) {
        return (self.x ** 2 + self.y ** 2) ** 0.5
    }
}

p := Point(3, 4)
print(p.x)            // 3
print(p.y)            // 4
print(p.distance())   // 5.0
print(type(p))        // Point
```

---

## Benchmark

```ms
// benchmarks/bench_class.ms
class Vec { func __init__(self, x, y) { self.x = x; self.y = y } }
n := 1_000_000
for i in range(n) { _ = Vec(i, i+1) }
// 目标：> 1M instantiation/sec
```

---

## 风险与边界

- **实例的 `attrs` 字典开销**：每个实例创建一个 MsMapObj（约 100B）；高频创建小对象时内存压力大。后续可优化为内联槽（`__slots__`）。
- **`__init__` 返回值**：Python 中 `__init__` 必须返回 None；mslang 同样忽略 `__init__` 的返回值（始终返回 inst）。
