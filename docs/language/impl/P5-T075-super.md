# P5-T075 super() 代理

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `super()` 内置函数与 `OP_GET_SUPER` 指令：在方法体内调用 `super()` 返回一个代理对象，属性查找从 MRO 中当前类的下一个类开始（跳过当前类），支持 `super().__init__(args)` 等用法。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T073 | MRO 查找 |
| P5-T072 | 实例/类对象 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §20 super() 代理 |
| `vm.md` | §5.2 OP_GET_SUPER |

---

## 实现要点

### 1. MsSuperObj

```c
typedef struct MsSuperObj {
  MsObject   header;
  MsTypeObj* startType;  // 从 MRO 中 startType 的下一个开始查找
  MsValue    instance;   // 绑定的实例（self）
} MsSuperObj;
```

### 2. super() 语义

```
super()  →  MsSuperObj(startType=当前类, instance=当前 self)
super(Type, inst)  →  MsSuperObj(startType=Type, instance=inst)
```

"当前类"通过编译器在方法体内隐式传入（`OP_GET_SUPER` 指令从当前帧的 closure 中提取类信息）。

### 3. MsSuperObj.tp_getattr

```c
static MsValue superGetAttr(MsValue v, MsValue name) {
  MsSuperObj* su = (MsSuperObj*)MS_AS_OBJ(v);
  MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(su->instance);

  // 从 MRO 中 startType 之后的类开始查找
  bool found = false;
  for (uint32_t i = 0; i < inst->klass->mroLen; i++) {
    if (inst->klass->mro[i] == (MsType*)su->startType) { found = true; continue; }
    if (found) {
      MsTypeObj* cur = (MsTypeObj*)inst->klass->mro[i];
      MsValue method = msMapGet(MS_OBJ_VAL(cur->methods), name);
      if (!MS_IS_NIL(method)) {
        return msNewBoundMethod(method, su->instance);
      }
    }
  }
  return MS_NIL_VAL;  // AttributeError
}
```

---

## 验收标准（checklist）

- [ ] `class B(A): func __init__(self): super().__init__()` → 正确调用 A.__init__。
- [ ] `super()` 在类方法外调用 → RuntimeError。
- [ ] 多层继承链中 `super()` 正确按 MRO 顺序传递。

---

## 测试用例（.ms）

```ms
class Animal {
    func __init__(self, name) { self.name = name }
    func speak(self) { return "..." }
}

class Dog extends Animal {
    func __init__(self, name, breed) {
        super().__init__(name)   // 调用 Animal.__init__
        self.breed = breed
    }
    func speak(self) { return $"Woof! I'm {self.name}" }
}

d := Dog("Rex", "Labrador")
print(d.name)   // Rex（由 Animal.__init__ 设置）
print(d.breed)  // Labrador
print(d.speak()) // Woof! I'm Rex

// 多继承 super 链
class A { func hello(self) { return "A" } }
class B extends A { func hello(self) { return "B," + super().hello() } }
class C extends A { func hello(self) { return "C," + super().hello() } }
class D extends B, C {}  // MRO: D→B→C→A

d := D()
print(d.hello())   // B,C,A
```

---

## 风险与边界

- **`super()` 无参数（零参数）版本**：编译器需要识别 `super()` 并注入当前类信息；初版实现：在方法体内 `super()` 等同于 `super(__class__, self)`，`__class__` 是编译器插入的隐式变量（存储当前类的 upvalue）。
