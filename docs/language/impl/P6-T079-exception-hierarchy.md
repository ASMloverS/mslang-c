# P6-T079 异常类层次构建

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在运行时建立 mslang 的异常类层次结构，所有异常类都是继承自 `BaseException` 的用户定义类（通过 `OP_MAKE_CLASS` 在 VM 初始化时创建）。构建完成后，`isinstance`（T078）和 catch 类型匹配（T081）可以沿 MRO 正确分派。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T072 | 类实例化 |
| P5-T073 | MRO |
| P5-T078 | isinstance |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `errors.md` | §1 异常层次（完整树） |

---

## 异常类层次

```
BaseException
├── SystemExit
├── KeyboardInterrupt
└── Exception
    ├── StopIteration
    ├── ArithmeticError
    │   ├── ZeroDivisionError
    │   └── OverflowError
    ├── AttributeError
    ├── ImportError
    │   └── ModuleNotFoundError
    ├── IndexError
    ├── KeyError
    ├── NameError
    │   └── UnboundLocalError
    ├── TypeError
    ├── ValueError
    │   └── UnicodeError
    ├── RuntimeError
    │   └── RecursionError
    ├── OSError
    │   ├── FileNotFoundError
    │   └── PermissionError
    ├── NotImplementedError
    └── AssertionError
```

---

## 实现要点

### 1. 内置异常注册

```c
// 在 msVMInit 中调用
void msBuiltinExceptionsInit(void) {
  // 1. 创建 BaseException 类
  gVM.BaseException = msCreateExcClass("BaseException", NULL);
  // 2. 依次创建子类
  gVM.Exception = msCreateExcClass("Exception", gVM.BaseException);
  gVM.TypeError  = msCreateExcClass("TypeError",  gVM.Exception);
  gVM.ValueError = msCreateExcClass("ValueError", gVM.Exception);
  gVM.IndexError = msCreateExcClass("IndexError", gVM.Exception);
  gVM.KeyError   = msCreateExcClass("KeyError",   gVM.Exception);
  gVM.NameError  = msCreateExcClass("NameError",  gVM.Exception);
  gVM.AttributeError = msCreateExcClass("AttributeError", gVM.Exception);
  gVM.ZeroDivisionError = msCreateExcClass("ZeroDivisionError",
                                              msCreateExcClass("ArithmeticError", gVM.Exception));
  gVM.StopIteration = msCreateExcClass("StopIteration", gVM.Exception);
  gVM.AssertionError = msCreateExcClass("AssertionError", gVM.Exception);
  gVM.RuntimeError   = msCreateExcClass("RuntimeError", gVM.Exception);
  gVM.ImportError    = msCreateExcClass("ImportError",  gVM.Exception);
  gVM.OSError        = msCreateExcClass("OSError",      gVM.Exception);
  // ... 其余子类

  // 3. 注册到全局命名空间
  msRegisterGlobal("BaseException", MS_OBJ_VAL(gVM.BaseException));
  msRegisterGlobal("Exception",     MS_OBJ_VAL(gVM.Exception));
  msRegisterGlobal("TypeError",     MS_OBJ_VAL(gVM.TypeError));
  // ...
}

// 辅助：创建只有 __init__ + __str__ 的异常类
MsTypeObj* msCreateExcClass(const char* name, MsTypeObj* base) {
  MsTypeObj* tp = msNewTypeObj(name, base ? &base->mstype : NULL);
  // 添加 __init__(self, *args) → self.args = args; self.message = str(args[0]) if args
  // 添加 __str__(self) → self.message 或 repr(self.args)
  // 添加 __repr__(self) → "TypeName(message)"
  return tp;
}
```

### 2. 异常实例创建辅助

```c
// VM 内部创建异常实例（无需 .ms 代码）
MsValue msNewException(MsTypeObj* excClass, const char* msg) {
  MsInstanceObj* inst = msAllocInstance(excClass);
  MsValue msgVal = msNewStr(msg, strlen(msg));
  msMapSet(MS_OBJ_VAL(inst->attrs), msInternStr("message"), msgVal);
  msMapSet(MS_OBJ_VAL(inst->attrs), msInternStr("args"),    MS_OBJ_VAL(msNewTuple1(msgVal)));
  return MS_OBJ_VAL(inst);
}

// 快捷函数（在 VM 指令中使用）
MsValue msRaiseTypeError(MsThread* t, const char* fmt, ...) {
  char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  MsValue exc = msNewException(gVM.TypeError, buf);
  t->currentException  = exc;
  t->hasException      = true;
  return MS_ERROR_VALUE;
}
// 类似：msRaiseNameError / msRaiseAttributeError / msRaiseIndexError / ...
```

---

## 验收标准（checklist）

- [ ] `TypeError` 是 `Exception` 的子类：`isinstance(TypeError(), Exception)` → true。
- [ ] `BaseException` 是所有异常的根。
- [ ] `ZeroDivisionError` 是 `ArithmeticError` 的子类。
- [ ] 异常实例有 `.message` 和 `.args` 属性。
- [ ] `repr(TypeError("bad"))` → `"TypeError('bad')"`。
- [ ] 所有内置异常注册到全局命名空间（`TypeError`、`ValueError` 等可直接使用）。

---

## 测试用例（.ms）

```ms
// 检查继承关系
try {
    raise TypeError("oops")
} catch Exception as e {
    print("caught Exception:", e.message)   // caught Exception: oops
}

// isinstance 检查
e := ValueError("bad value")
print(isinstance(e, ValueError))   // true
print(isinstance(e, Exception))    // true
print(isinstance(e, TypeError))    // false
print(isinstance(e, BaseException)) // true

// 异常属性
err := OSError("file not found")
print(err.message)   // file not found
print(err.args)      // ("file not found",)
```

---

## Benchmark

N/A（异常层次构建只在 VM 初始化时执行一次）。

---

## 风险与边界

- **`BaseException.__init__` vs `Exception.__init__`**：两者共享同一 `__init__` 实现；子类若不重写则继承。
- **`msNewException` vs 用户 `raise ClassName(msg)`**：VM 内部错误（如 TypeError in VM 指令）使用 `msNewException`（C 快捷路径）；用户代码 `raise TypeError("x")` 走正常 class 实例化路径。
