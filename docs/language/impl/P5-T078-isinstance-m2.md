# P5-T078 isinstance / type + OP_ISINSTANCE + M2 里程碑

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `isinstance(obj, Type)` / `type(obj)` 内置函数和 `OP_ISINSTANCE` 指令，完成 P5 阶段的 **M2 里程碑**：函数、闭包、class 系统（含继承、魔术方法、super）完整可运行，`.ms` 测试套件全部通过。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T072 ~ T077 | 完整 class 系统 |
| P5-T073 | MRO（isinstance 沿 MRO 检查） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §22 isinstance / type |
| `vm.md` | §5.3 OP_ISINSTANCE |

---

## 实现要点

### 1. OP_ISINSTANCE

```c
// 栈：[obj, type_or_tuple]
case OP_ISINSTANCE: {
  MsValue typeSpec = POP();
  MsValue obj      = POP();
  bool result = msIsInstance(obj, typeSpec);
  PUSH(MS_BOOL_VAL(result));
  DISPATCH();
}

bool msIsInstance(MsValue obj, MsValue typeSpec) {
  // typeSpec 可以是单个类或类的 tuple
  if (MS_IS_OBJ(typeSpec) && MS_AS_OBJ(typeSpec)->type == &msTupleType) {
    MsTupleObj* types = (MsTupleObj*)MS_AS_OBJ(typeSpec);
    for (uint32_t i = 0; i < types->len; i++) {
      if (msIsInstance(obj, types->items[i])) return true;
    }
    return false;
  }

  // 获取 obj 的类
  MsType* objType = msTypeOf(obj);

  // 内置类型匹配
  if (MS_IS_OBJ(typeSpec) && MS_AS_OBJ(typeSpec)->type == &msMetaType) {
    MsTypeObj* tp = (MsTypeObj*)MS_AS_OBJ(typeSpec);
    // 沿 MRO 查找
    if (objType == &msInstanceType) {
      MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(obj);
      for (uint32_t i = 0; i < inst->klass->mroLen; i++) {
        if (inst->klass->mro[i] == (MsType*)tp) return true;
      }
    }
    return false;
  }
  return false;  // typeSpec 不是类 → TypeError
}
```

### 2. isinstance() 内置函数

```c
static MsValue msBuiltinIsinstance(MsValue* args, int argc) {
  if (argc != 2) return MS_ERROR_VALUE;
  return MS_BOOL_VAL(msIsInstance(args[0], args[1]));
}
```

### 3. type() 内置函数（扩展）

```c
static MsValue msBuiltinType(MsValue* args, int argc) {
  if (argc == 1) {
    MsType* tp = msTypeOf(args[0]);
    if (tp == &msInstanceType) {
      MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(args[0]);
      return MS_OBJ_VAL(inst->klass);  // 返回类对象
    }
    return msNewStr(tp->name, strlen(tp->name));
  }
  // type(name, bases, dict) → 动态创建类（初版不支持）
  return MS_ERROR_VALUE;
}
```

---

## M2 测试套件（`tests/ms/m2/`）

### `tests/ms/m2/class_basic.ms`

```ms
class Animal {
    func __init__(self, name) { self.name = name }
    func speak(self) { return "..." }
}
class Dog extends Animal {
    func speak(self) { return $"Woof! I'm {self.name}" }
}

d := Dog("Rex")
print(d.speak())                 // Woof! I'm Rex
print(isinstance(d, Dog))        // true
print(isinstance(d, Animal))     // true
print(isinstance(d, int))        // false
```

### `tests/ms/m2/closures.ms`

```ms
func compose(f, g) { return func(x) { return f(g(x)) } }
double := func(x) { return x * 2 }
inc    := func(x) { return x + 1 }
doubleInc := compose(double, inc)
print(doubleInc(5))   // 12（(5+1)*2）
```

### `tests/ms/m2/magic_methods.ms`

```ms
class Fraction {
    func __init__(self, n, d) { self.n = n; self.d = d }
    func __add__(self, o) { return Fraction(self.n*o.d + o.n*self.d, self.d*o.d) }
    func __repr__(self) { return $"{self.n}/{self.d}" }
}
a := Fraction(1, 2)
b := Fraction(1, 3)
print(repr(a + b))   // 5/6
```

---

## 验收标准（checklist）

- [ ] `isinstance(Dog(), Animal)` → true（继承检查）。
- [ ] `isinstance(Dog(), (Animal, str))` → true（tuple of types）。
- [ ] `isinstance(42, int)` → true（内置类型）。
- [ ] `isinstance("x", int)` → false。
- [ ] `OP_ISINSTANCE` 正确（用于 T046 catch 类型匹配）。
- [ ] `tests/ms/m2/*.ms` 全部通过。
- [ ] 完整 M2 验收：class/继承/MRO/魔术方法/super/闭包/isinstance 均正确。

---

## M2 Benchmark 目标

| 测试 | 目标 |
|---|---|
| 类实例化 100 万次 | < 1s |
| 方法调用 100 万次 | < 1s |
| 继承链 5 层 MRO 查找 | < 0.5s |
| 闭包 counter 1000 万次调用 | < 2s |

---

## 风险与边界

- **`isinstance` 与内置类型**：`isinstance(42, int)` 需要将内置 `int` 识别为类型对象；初版通过特殊路径（`msTypeOf(args[0]) == &msIntType && typeSpec == gVM.intTypeObj`）处理。
- **`type()` 返回类对象 vs 字符串**：初版 `type(x)` 对实例返回类对象，对内置值返回类型名字符串（简化）；M2 后可统一返回类型对象。
