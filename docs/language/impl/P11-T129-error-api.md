# P11-T129 错误处理 API + 内置异常指针

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 C API 的错误处理接口：C 扩展函数可抛出 mslang 异常（`msRaise*`）、检查当前是否有异常（`msHasError`）、获取异常对象；同时暴露内置异常类型的 C 指针，供扩展直接使用。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T079 | 内置异常类层次 |
| P11-T127 | 嵌入 API（VM 状态） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `c-api.md` | §4.4 错误处理 |

---

## 实现要点

### 1. 内置异常类型指针

```c
// 在 msVMInit 后初始化，供 C 扩展直接引用
MsValue msExcBaseException;    // BaseException
MsValue msExcException;        // Exception
MsValue msExcTypeError;        // TypeError
MsValue msExcValueError;       // ValueError
MsValue msExcIndexError;       // IndexError
MsValue msExcKeyError;         // KeyError
MsValue msExcNameError;        // NameError
MsValue msExcAttributeError;   // AttributeError
MsValue msExcRuntimeError;     // RuntimeError
MsValue msExcOSError;          // OSError
MsValue msExcFileNotFoundError;// FileNotFoundError
MsValue msExcStopIteration;    // StopIteration
MsValue msExcAssertionError;   // AssertionError
MsValue msExcImportError;      // ImportError
MsValue msExcNotImplementedError; // NotImplementedError
MsValue msExcZeroDivisionError;   // ZeroDivisionError
MsValue msExcOverflowError;       // OverflowError
MsValue msExcRecursionError;      // RecursionError
```

### 2. 抛出异常的 C API

```c
// 抛出特定类型的异常（设置 vm 当前异常，返回 MS_ERROR_VALUE；c-api.md §4.4）
void    msRaiseString   (MsVM* vm, struct MsType* excType, const char* msg);
void    msRaiseException(MsVM* vm, struct MsObject* exc);

// 快捷函数（便于常见异常）
MsValue msRaiseTypeError    (MsVM* vm, const char* fmt, ...);
MsValue msRaiseValueError   (MsVM* vm, const char* fmt, ...);
MsValue msRaiseIndexError   (MsVM* vm, const char* fmt, ...);
MsValue msRaiseKeyError     (MsVM* vm, MsValue key);
MsValue msRaiseAttributeError(MsVM* vm, const char* name);
MsValue msRaiseRuntimeError (MsVM* vm, const char* fmt, ...);
MsValue msRaiseOSError      (MsVM* vm, int errnum, const char* path);
MsValue msRaiseStopIteration(MsVM* vm);
MsValue msRaiseAssertionError(MsVM* vm, const char* msg);
MsValue msRaiseNotImplementedError(MsVM* vm, const char* msg);
MsValue msRaiseZeroDivisionError(MsVM* vm);
MsValue msRaiseOverflowError(MsVM* vm, const char* msg);

// 内部实现
MsValue msRaiseTypeError(MsVM* vm, const char* fmt, ...) {
  char msgbuf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
  va_end(ap);
  msRaiseString(vm, msExcTypeError, msgbuf);
  return MS_ERROR_VALUE;
}
```

### 3. 异常检查与清除

```c
// 检查返回值是否为错误哨兵（c-api.md §4.4）
int     msIsError(MsValue v);           // v.tag == MS_TAG_ERROR

// 获取当前异常对象（含 message/traceback 等属性）
MsValue msGetError(MsVM* vm);

// 清除当前异常（消费异常后调用）
void    msClearError(MsVM* vm);
```

### 4. C 扩展函数的典型错误处理模式

```c
// C 扩展函数模板（c-api.md §6.1：MsCFunction 签名）
static MsValue myFunc(MsVM* vm, MsValue* argv, int argc) {
  if (argc != 2)
    return msRaiseTypeError(vm, "myFunc() takes 2 arguments, got %d", argc);

  if (!MS_IS_INT(argv[0]))
    return msRaiseTypeError(vm, "first argument must be int");

  int64_t n = MS_AS_INT(argv[0]);
  if (n < 0)
    return msRaiseValueError(vm, "n must be non-negative, got %lld", (long long)n);

  // 调用可能失败的子函数
  MsValue result = someOtherCall(vm, argv[1]);
  if (MS_IS_ERROR(result)) return result;  // 透传异常

  return MS_INT_VAL(n * 2);
}
```

---

## 验收标准（checklist）

- [ ] `msRaiseTypeError(vm, "bad arg")` → 返回 `MS_ERROR_VALUE`，`msGetError(vm)` 返回异常对象。
- [ ] 异常被 .ms 的 `catch (e: TypeError)` 捕获。
- [ ] `msGetError(vm)` 返回正确的异常对象（含 message 属性）。
- [ ] `msClearError(vm)` 后 `msIsError(msGetError(vm))` = false。
- [ ] `msExcTypeError` 等全局指针在 `msNew()` 后有效。

---

## 测试用例（C 单测）

```c
// tests/test_error_api.c
void testRaiseAndCatch(void) {
  MsVM* vm = msNew();

  // C 函数抛出异常
  msSetGlobal(vm, "bad_func",
    msNewCFunction(badFunc, "bad_func", 0));  // badFunc: MsCFunction 签名

  // .ms 中捕获
  MsValue r = msRunString(vm,
    "try { bad_func() } catch (e: ValueError) { e.message }",
    "<test>");
  // r 应为字符串 "intentional error"（若顶层表达式返回值可访问）

  msFree(vm);
}
```

---

## Benchmark

N/A。

---

## 风险与边界

- **格式化字符串**：`msRaise` 使用 `vsnprintf` 将格式化的 C 字符串构建为 exception message；最大长度 512 字节（超出截断）。关键场景（大路径名等）需要单独的 `msRaiseWithStr` 函数接受 `MsValue`。
