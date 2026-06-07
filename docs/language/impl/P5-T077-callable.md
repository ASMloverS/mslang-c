# P5-T077 __call__ / 可调用对象

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现可调用对象协议（`__call__`）：任何定义了 `__call__` 方法的实例都可以用 `obj(args)` 调用，等价于 `obj.__call__(args)`。同时完善 `OP_CALL` 对所有可调用类型的统一分派。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T074 | 魔术方法分派 |
| P5-T068 | OP_CALL |

---

## 实现要点

### 1. OP_CALL 的可调用类型分派

```c
case OP_CALL: {
  // ... （基础参数解析）
  MsType* tp = msTypeOf(callee);

  // 1. 内置函数（MsCFunctionObj）→ 直接 C 调用
  // 2. 闭包（MsClosureObj）→ 创建帧
  // 3. 绑定方法（MsBoundMethodObj）→ 注入 self 后调用
  // 4. 类对象（MsTypeObj）→ 实例化（tpCall=typeCall）
  // 5. 普通实例有 __call__（MsInstanceObj）→ 查找并调用
  // 6. 其他 → TypeError

  if (tp->tpCall) {
    MsValue* args = t->sp - argc;
    MsValue result = tp->tpCall(callee, args, argc);
    t->sp -= argc + 1;
    PUSH(result);
    DISPATCH();
  }
  if (MS_IS_OBJ(callee) && MS_AS_OBJ(callee)->type == &msInstanceType) {
    // 查找 __call__
    MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(callee);
    MsValue callM = msTypeLookupMethodMRO(inst->klass, msInternStr("__call__"));
    if (!MS_IS_NIL(callM)) {
      // callM 是 func，注入 self 调用
      MsValue* args = t->sp - argc;
      MsValue newArgs[256]; newArgs[0] = callee;
      memcpy(newArgs + 1, args, argc * sizeof(MsValue));
      MsValue result = msCallFn(t, callM, newArgs, argc + 1);
      t->sp -= argc + 1;
      PUSH(result);
      DISPATCH();
    }
  }
  return msTypeError(t, "'%s' object is not callable", tp->name);
}
```

### 2. callable() 内置函数

```c
static MsValue msBuiltinCallable(MsValue* args, int argc) {
  if (argc != 1) return MS_ERROR_VALUE;
  MsValue v = args[0];
  MsType* tp = msTypeOf(v);
  if (tp->tpCall) return MS_BOOL_VAL(true);
  if (MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msInstanceType) {
    MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(v);
    MsValue m = msTypeLookupMethodMRO(inst->klass, msInternStr("__call__"));
    return MS_BOOL_VAL(!MS_IS_NIL(m));
  }
  return MS_BOOL_VAL(false);
}
```

---

## 验收标准（checklist）

- [ ] `class Adder { func __init__(self, n) { self.n = n } func __call__(self, x) { return x + self.n } }; add5 = Adder(5); add5(3)` → 8。
- [ ] `callable(Adder(5))` → true。
- [ ] `callable(42)` → false。
- [ ] `callable(print)` → true（内置函数）。

---

## 测试用例（.ms）

```ms
class Multiplier {
    func __init__(self, factor) { self.factor = factor }
    func __call__(self, x) { return x * self.factor }
}

double := Multiplier(2)
triple := Multiplier(3)
print(double(5))    // 10
print(triple(5))    // 15
print(callable(double))  // true
print(callable(5))       // false

// 作为高阶函数参数
nums := [1, 2, 3, 4, 5]
doubled := list(map(double, nums))
print(doubled)   // [2, 4, 6, 8, 10]
```

---

## 风险与边界

- **`tpCall` 与 `__call__` 的优先级**：内置类型通过 `tpCall` 槽实现可调用；用户定义类通过 `__call__` 魔术方法；`OP_CALL` 先检查 `tpCall`（快路径），再检查 `__call__`（慢路径）。
