# P4-T067 .ms 端到端打通（run 子命令）+ M1 里程碑

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

将 P1（词法）、P2（语法）、P3（编译）、P4（VM+类型）打通为完整的 `mslang run <file>` 流程，实现 **M1 里程碑**：可运行基础 `.ms` 脚本（含算术/字符串/list/map/range/for 循环/函数/打印）。同时建立 `.ms` 测试基线（`tests/ms/` 目录），为后续所有里程碑的验证奠定基础。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T049 ~ T066 | P4 所有任务（VM + 核心类型） |
| P0-T004 | CLI 骨架（`mslang run` 子命令） |
| P3-T048 | disasm 子命令（辅助调试） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §1 顶层执行流程 |

---

## 待实现（C 文件）

### 修改文件

```
src/cli/ms_cli.c         # cmdRun 实现（读文件 → 编译 → VM 执行）
src/vm/ms_vm.c           # msVMInit：注册内置函数（print/len/type/range）
tests/ms/               # .ms 测试脚本目录（新建）
```

---

## 实现要点

### 1. `mslang run <file>` 实现

```c
static int cmdRun(int argc, char** argv) {
  if (argc < 2) {
    fputs("usage: mslang run <file> [args...]\n", stderr);
    return 1;
  }
  const char* path = argv[1];
  char* src = msReadFile(path);
  if (!src) { perror(path); return 1; }

  MsCompileResult r = msCompileFile(src, strlen(src), path);
  free(src);
  if (r.hadError) {
    fprintf(stderr, "%s\n", r.errBuf);
    return 1;
  }

  msVMInit();
  MsValue result = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);

  return MS_IS_ERROR(result) ? 1 : 0;
}
```

### 2. 内置函数注册（M1 最小集）

在 `msVMInit` 中将以下内置函数注册到全局命名空间：

| 函数 | M1 版本说明 |
|---|---|
| `print(...)` | 将所有参数 repr 后空格分隔输出，末尾加 `\n` |
| `len(x)` | 调用 `tpLen` |
| `type(x)` | 返回类型名称字符串（`msTypeOf(x)->name`） |
| `range(...)` | `msBuiltinRange`（T064） |
| `input(prompt)` | 读取 stdin 一行，返回 str |
| `repr(x)` | 调用 `tpRepr` |
| `str(x)` | 调用 `tpStr` |
| `int(x)` | 字符串→int（简化版） |
| `float(x)` | 字符串→float（简化版） |
| `bool(x)` | `msValueTruthy(x)` → bool |
| `list(iter)` | 将可迭代对象收集为 list |
| `tuple(iter)` | 将可迭代对象收集为 tuple |
| `set(iter)` | 将可迭代对象收集为 set |

```c
void msVMInit(void) {
  msGCInit();
  // 初始化线程
  gVM.mainThread.sp    = gVM.mainThread.stack;
  gVM.mainThread.frame = NULL;
  gVM.mainThread.globals = msNewMap(64);  // 全局命名空间

  // 注册内置类型
  gVM.intType   = &msIntType;
  gVM.floatType = &msFloatType;
  // ...

  // 注册内置函数
  msRegisterBuiltin("print", msBuiltinPrint);
  msRegisterBuiltin("len",   msBuiltinLen);
  msRegisterBuiltin("type",  msBuiltinType);
  msRegisterBuiltin("range", msBuiltinRange);
  msRegisterBuiltin("repr",  msBuiltinRepr);
  msRegisterBuiltin("str",   msBuiltinStr);
  msRegisterBuiltin("int",   msBuiltinInt);
  msRegisterBuiltin("float", msBuiltinFloat);
  msRegisterBuiltin("bool",  msBuiltinBool);
  msRegisterBuiltin("list",  msBuiltinList);
  msRegisterBuiltin("tuple", msBuiltinTuple);
  msRegisterBuiltin("set",   msBuiltinSet);
  msRegisterBuiltin("input", msBuiltinInput);
}
```

### 3. print 实现

```c
static MsValue msBuiltinPrint(MsValue* args, int argc) {
  for (int i = 0; i < argc; i++) {
    if (i > 0) fputc(' ', stdout);
    MsType* tp = msTypeOf(args[i]);
    if (tp->tpStr) {
      MsValue sv = tp->tpStr(args[i]);
      MsStrObj* s = (MsStrObj*)MS_AS_OBJ(sv);
      fwrite(s->data, 1, s->len, stdout);
    } else {
      fputs("?", stdout);
    }
  }
  fputc('\n', stdout);
  return MS_NIL_VAL;
}
```

### 4. .ms 测试框架（`tests/ms/`）

每个 `.ms` 测试文件对应一个 `.expected` 文件（期望的 stdout 输出）：

```bash
# tests/run_ms_tests.sh
MSLANG=./build/mslang
PASS=0; FAIL=0

for ms in tests/ms/**/*.ms; do
    expected="${ms%.ms}.expected"
    actual=$($MSLANG run "$ms" 2>/dev/null)
    if [ -f "$expected" ] && [ "$actual" = "$(cat "$expected")" ]; then
        echo "PASS $ms"
        ((PASS++))
    else
        echo "FAIL $ms"
        [ -f "$expected" ] && diff <(echo "$actual") "$expected"
        ((FAIL++))
    fi
done
echo "MS tests: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
```

---

## M1 测试基线（`.ms` 脚本）

### `tests/ms/m1/hello.ms`

```ms
print("Hello, mslang!")
```
**期望输出**：`Hello, mslang!`

### `tests/ms/m1/arith.ms`

```ms
print(1 + 2)           // 3
print(10 / 3)          // 3
print(10 % 3)          // 1
print(2 ** 10)         // 1024
print(3.14 * 2)        // 6.28
print(1 + 2.0)         // 3.0
```
**期望输出**：
```
3
3
1
1024
6.28
3.0
```

### `tests/ms/m1/variables.ms`

```ms
x := 10
y := 20
z := x + y
print(z)            // 30

a, b := 1, 2
print(a, b)         // 1 2
a, b = b, a
print(a, b)         // 2 1
```

### `tests/ms/m1/strings.ms`

```ms
s := "Hello, " + "World!"
print(s)                // Hello, World!
print(len(s))           // 13
print(s[0])             // H
print(s[-1])            // !
print(s[7:12])          // World
print(s.upper())        // HELLO, WORLD!
print("ab" * 3)         // ababab
```

### `tests/ms/m1/control.ms`

```ms
// if/else
x := 5
if x > 3 {
    print("big")
} else {
    print("small")
}
// big

// for range
sum := 0
for i in range(10) {
    sum += i
}
print(sum)   // 45

// for in list
for v in [1, 2, 3] {
    print(v)
}
// 1 2 3

// while-style
n := 5
fac := 1
for n > 0 {
    fac *= n
    n -= 1
}
print(fac)   // 120
```

### `tests/ms/m1/functions.ms`

```ms
func add(a, b) { return a + b }
print(add(3, 4))   // 7

func factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
print(factorial(10))   // 3628800

// 闭包
func makeAdder(n) {
    return func(x) { return x + n }
}
add5 := makeAdder(5)
print(add5(3))    // 8
print(add5(10))   // 15
```

### `tests/ms/m1/collections.ms`

```ms
// list
lst := [3, 1, 4, 1, 5, 9, 2, 6]
lst.sort()
print(lst)   // [1, 1, 2, 3, 4, 5, 6, 9]

// map
m := {"a": 1, "b": 2, "c": 3}
for k in m.keys() {
    print(k, m[k])
}

// set
s := {1, 2, 2, 3, 3}
print(len(s))   // 3
print(1 in s)   // true

// range
print(list(range(5)))   // [0, 1, 2, 3, 4]
```

### `tests/ms/m1/fibonacci_bench.ms`（性能基准）

```ms
// 朴素递归 fib（测试函数调用性能）
func fib(n) {
    if n <= 1 { return n }
    return fib(n-1) + fib(n-2)
}

import time
t0 := time.now()
print(fib(35))   // 9227465
t1 := time.now()
print($"fib(35) took {(t1-t0)*1000:.1f}ms")
// 目标：< 5s（CPython 约 2-3s，初版 mslang 可略慢）
```

---

## 验收标准（checklist）

- [ ] `mslang run hello.ms` 输出 `Hello, mslang!`。
- [ ] `mslang run arith.ms` 输出全部正确（数值精度）。
- [ ] `mslang run functions.ms` 递归 `factorial(10)` 正确。
- [ ] 闭包（makeAdder）正确捕获 upvalue。
- [ ] `mslang run collections.ms` list/map/set/range 全部正确。
- [ ] `tests/ms/m1/*.ms` 全部 golden 测试通过。
- [ ] `mslang disasm arith.ms` 输出可读反汇编（P3 里程碑）。
- [ ] GC 无内存泄漏（valgrind 运行 all M1 tests 无报告）。
- [ ] `fib(30)` 可以完成（无栈溢出）。

---

## M1 Benchmark 目标

| 测试 | 目标 |
|---|---|
| `fib(30)` 递归 | < 2s |
| `sum(range(10M))` 循环 | < 1s |
| 100 万次 list.append | < 1s |
| 100 万次 map 插入 | < 1s |
| hello world 冷启动 | < 50ms |

---

## 风险与边界

- **内置函数 `OP_CALL` 分派**：调用内置函数（`MsCFunction*`）时，VM `OP_CALL` 需检测 callee 是否为内置（`MsCFunctionType`）并直接调用 C 函数，而非创建调用帧。T068 调用约定完善此处理。
- **`print` 关键字 vs 函数**：mslang 中 `print` 是普通函数（非关键字），通过 `OP_GET_GLOBAL` + `OP_CALL` 调用，无特殊处理。
- **异常显示**：M1 阶段遇到 `MS_ERROR_VALUE` 直接打印 "RuntimeError" 到 stderr 并退出；P6 后才有完整异常层次和 traceback。
- **`time` 模块**：`tests/ms/m1/fibonacci_bench.ms` 中使用 `time.now()`；M1 阶段可不实现 time 模块（bench 注释掉 time 调用）；P12 stdlib 阶段实装。
