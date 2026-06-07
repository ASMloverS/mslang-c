# P8-T096 内置函数：print / input / len / type / repr / str

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现最核心的一批内置函数，注册到 VM 全局命名空间：`print`、`input`、`len`、`type`、`repr`、`str`。这些函数在 M1（T067）已有最小占位实现，本任务提供完整规范版本。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj |
| P5-T072 | MsTypeObj（type() 返回类型对象） |

---

## 实现要点

### 1. `print`

```c
// print(*args, sep=" ", end="\n", file=stdout)
// 简化版：只支持 positional args，sep=" "，end="\n"
static MsValue builtinPrint(MsThread* t, MsValue* args, int argc) {
  for (int i = 0; i < argc; i++) {
    if (i > 0) fputs(" ", stdout);
    MsValue s = msValueStr(args[i]);   // 调用 tpStr 或默认 repr
    if (MS_IS_ERROR(s)) return s;
    MsStrObj* str = (MsStrObj*)MS_AS_OBJ(s);
    fwrite(str->data, 1, str->len, stdout);
  }
  fputs("\n", stdout);
  fflush(stdout);
  return MS_NIL_VAL;
}
```

### 2. `input`

```c
// input([prompt]) → str
static MsValue builtinInput(MsThread* t, MsValue* args, int argc) {
  if (argc >= 1) {
    MsValue prompt = msValueStr(args[0]);
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(prompt);
    fwrite(s->data, 1, s->len, stdout);
    fflush(stdout);
  }
  char buf[4096];
  if (!fgets(buf, sizeof(buf), stdin)) return MS_NIL_VAL;
  size_t len = strlen(buf);
  if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
  return msNewStr(buf, len);
}
```

### 3. `len`

```c
static MsValue builtinLen(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "len() takes exactly 1 argument");
  MsType* ty = msTypeOf(args[0]);
  if (!ty->tpLen) return msRaiseTypeError(t, "object has no len()");
  int64_t n = ty->tpLen(args[0]);
  if (n < 0) return MS_ERROR_VALUE;  // 已设异常
  return MS_INT_VAL(n);
}
```

### 4. `type`

```c
static MsValue builtinType(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "type() takes exactly 1 argument");
  MsType* ty = msTypeOf(args[0]);
  // 对于 MsTypeObj，返回 metaclass；对于其他，返回 MsTypeObj*
  if (MS_IS_OBJ(args[0]) && MS_AS_OBJ(args[0])->type == &msTypeType) {
    return MS_OBJ_VAL(MS_AS_OBJ(args[0]));  // type(SomeClass) → SomeClass
  }
  return MS_OBJ_VAL(ty->typeObj);  // 返回 MsTypeObj*
}
```

### 5. `repr` / `str`

```c
static MsValue builtinRepr(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "repr() takes 1 argument");
  return msValueRepr(args[0]);  // 调用 tpRepr
}

static MsValue builtinStr(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "str() takes 1 argument");
  return msValueStr(args[0]);   // 调用 tpStr（或 tpRepr 回退）
}
```

### 6. 注册

```c
void msRegisterGlobals(MsThread* t) {
  msSetGlobal(t, "print",  msNewCFunction(builtinPrint,  "print",  -1));
  msSetGlobal(t, "input",  msNewCFunction(builtinInput,  "input",   0));
  msSetGlobal(t, "len",    msNewCFunction(builtinLen,    "len",     1));
  msSetGlobal(t, "type",   msNewCFunction(builtinType,   "type",    1));
  msSetGlobal(t, "repr",   msNewCFunction(builtinRepr,   "repr",    1));
  msSetGlobal(t, "str",    msNewCFunction(builtinStr,    "str",     1));
  // ...
}
```

---

## 验收标准（checklist）

- [ ] `print(1, 2, 3)` → `1 2 3\n`。
- [ ] `print()` → `\n`（空行）。
- [ ] `len([1,2,3])` → `3`；`len("hello")` → `5`（codepoints）。
- [ ] `type(42)` → 返回 int 类型对象；`type(42).__name__` → `"int"`。
- [ ] `repr([1, "a"])` → `[1, "a"]`。
- [ ] `str(3.14)` → `"3.14"`；`str(nil)` → `"nil"`。

---

## 测试用例（.ms）

```ms
// 基本打印
print("hello", "world")   // hello world
print(1, 2.0, true, nil)  // 1 2.0 true nil

// len
print(len("你好"))         // 2（codepoints）
print(len([1,2,3]))        // 3
print(len({a:1, b:2}))    // 2

// type
print(type(42).__name__)      // int
print(type(3.14).__name__)    // float
print(type("s").__name__)     // str
print(type([]).__name__)      // list
print(type(nil).__name__)     // nil

// repr / str
print(repr("hello\nworld"))  // "hello\nworld"
print(str(42))               // 42
```

---

## Benchmark

```ms
// benchmarks/bench_print.ms
// 1M 次 str() 转换
n := 1_000_000
for i in range(n) { str(i) }
// 目标 < 1s
```

---

## 风险与边界

- **`print` 关键字参数**：完整版支持 `sep`/`end`/`file`；当前简化版仅支持 positional args，sep=" ", end="\n"。T104（vars/dir/open）之后可补全关键字参数支持。
