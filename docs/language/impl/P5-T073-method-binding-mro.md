# P5-T073 方法绑定 + MRO 查找

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现方法绑定（`MsBoundMethodObj`）和方法解析顺序（MRO，C3 线性化）。当实例访问方法时，返回绑定了 `self` 的方法对象；调用时自动将 `self` 作为第一个参数。MRO 支持多继承。

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

### 2. C3 MRO 线性化

```c
// 输入：bases 列表（MsType* 数组）
// 输出：mro 列表（含 self + 所有基类，按 C3 顺序）
MsType** msBuildMRO(MsTypeObj* cls, uint32_t* outLen) {
  // C3 算法：
  // mro(C) = [C] + merge(mro(B1), mro(B2), ..., [B1, B2, ...])
  // merge：每次从候选头部找第一个"不在任何其他列表尾部"的类，追加到结果
  // 实现参考 Python PEP 3141
  // ...
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
- [ ] MRO 顺序：`class C extends A, B {}` → MRO=[C,A,B,object]。
- [ ] 钻石继承（`class D extends B, C {}` B/C 都继承 A）→ C3 正确（A 只出现一次）。
- [ ] MRO 不一致时（违反 C3）→ TypeError。

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

// 多继承
class A { func hello(self) { return "from A" } }
class B extends A {}
class C extends A {}
class D extends B, C {}
print(D().hello())   // from A（C3 MRO: D→B→C→A）
```

---

## Benchmark

N/A（方法绑定成本在整体 class bench 中体现）。

---

## 风险与边界

- **MRO 计算时机**：在 `OP_MAKE_CLASS` 执行时（类定义时），在 VM 中执行 C3 算法；结果缓存在 `MsTypeObj.mro`。
- **`object` 基类**：所有用户定义类的 MRO 末尾都是 `object`（全局根类，方法字典含 `__repr__`/`__str__`/`__eq__` 等默认实现）。
