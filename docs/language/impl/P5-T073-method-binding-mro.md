# P5-T073 方法绑定 + MRO 查找

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现方法绑定（`MsBoundMethodObj`）和方法解析顺序（MRO）。当实例访问方法时，返回绑定了 `self` 的方法对象；调用时自动将 `self` 作为第一个参数。单继承下 MRO 为线性父类链。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T072 | 实例 / 类 对象定义 |
| P4-T060 | map（方法字典） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §3 MsType（MRO 算法） |
| `type-system.md` | §18 方法绑定 |

---

## 实现要点

### 1. MsBoundMethodObj

```c
typedef struct MsBoundMethodObj {
  MsObject  header;
  MsValue   func;   // MsClosureObj* 或 MsCFunctionObj*
  MsValue   self;   // 绑定的实例
} MsBoundMethodObj;

// tpCall：自动注入 self
static MsValue boundMethodCall(MsValue bm, MsValue* args, int argc) {
  MsBoundMethodObj* m = (MsBoundMethodObj*)MS_AS_OBJ(bm);
  // 在 args 前插入 self
  MsValue newArgs[256];
  newArgs[0] = m->self;
  memcpy(newArgs + 1, args, argc * sizeof(MsValue));
  return msCallFn(&gVM.mainThread, m->func, newArgs, argc + 1);
}
```

### 2. MRO 线性化（单继承）

```c
// 单继承 MRO：[cls, parent, grandparent, ..., object]
MsType** msBuildMRO(MsTypeObj* cls, uint32_t* outLen) {
  MsType* chain[64]; int len = 0;
  MsTypeObj* cur = cls;
  while (cur != NULL) {
    chain[len++] = &cur->mstype;
    cur = cur->mstype.base ? (MsTypeObj*)MS_AS_OBJ(cur->mstype.base) : NULL;
  }
  // chain[len-1] は object（または NULL で終了）
  MsType** mro = msAlloc(len * sizeof(MsType*));
  memcpy(mro, chain, len * sizeof(MsType*));
  *outLen = (uint32_t)len;
  return mro;
}
```

### 3. MRO 查找

```c
MsValue msTypeLookupMethodMRO(MsTypeObj* tp, MsValue name) {
  for (uint32_t i = 0; i < tp->mroLen; i++) {
    MsTypeObj* cur = (MsTypeObj*)tp->mro[i];
    MsValue method = msMapGet(MS_OBJ_VAL(cur->methods), name);
    if (!MS_IS_NIL(method)) return method;
  }
  return MS_NIL_VAL;
}
```

---

## 验收标准（checklist）

- [ ] `class A { func f(self) {} }; A().f` → MsBoundMethodObj（self 已绑定）。
- [ ] `A().f()` 自动将 A 实例作为 self 传入。
- [ ] `class B extends A {}; B().f()` → 从 A 继承的 f 被调用。
- [ ] MRO 顺序：`class B extends A {}; class C extends B {}` → MRO=[C,B,A,object]。

---

## 测试用例（.ms）

```ms
class Animal {
    func speak(self) { return "..." }
}
class Dog extends Animal {
    func speak(self) { return "Woof!" }
}
class Cat extends Animal {
    func speak(self) { return "Meow!" }
}

d := Dog()
c := Cat()
print(d.speak())   // Woof!
print(c.speak())   // Meow!

class A { func hello(self) { return "from A" } }
class B extends A {}
class C extends B {}
print(C().hello())   // from A（MRO: C→B→A→object）
```

---

## Benchmark

N/A（方法绑定成本在整体 class bench 中体现）。

---

## 风险与边界

- **MRO 计算时机**：在 `OP_MAKE_CLASS` 执行时（类定义时），结果缓存在 `MsTypeObj.mro`。单继承下为线性父类链，无 C3 算法。
- **`object` 基类**：所有用户定义类的 MRO 末尾都是 `object`（全局根类，方法字典含 `__repr__`/`__str__`/`__eq__` 等默认实现）。
