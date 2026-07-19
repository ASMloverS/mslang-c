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
| `vm.md` | §1 顶层执行流程、§3.2 变量操作（`LOAD_GLOBAL`/`STORE_GLOBAL`） |
| `stdlib.md` | §1 内置函数（无需 import）、§3 f-string 内插（格式规范初版不支持） |
| `execution.md` | §1 概览（缓存流程图）、§2.1 CLI 接口 |
| `syntax.md` | §1.8.1 f-string 字面量、§2.2 语句文法、§3.2 for 的三种形式 |
| `type-system.md` | §2.5 string、§3.4 魔术方法（`__str__`）、§5 类型内省 |
| `c-api.md` | §6.1 原生函数签名（`MsCFunction`） |

---

## 待实现（C 文件）

### 修改文件

```
src/core/ms_cli.c        # cmdRun 实现（当前为 stub："not implemented yet"）；复用已有 msVMInit/msVMRunFile/msVMShutdown（src/vm/ms_vm.c）
src/vm/ms_vm.c            # msVMInit：新增 t->globals（当前为 stub MS_NIL_VAL）与内置函数注册（print/len/type/range 等）；
                          # OP_GET_GLOBAL/OP_SET_GLOBAL（当前为 stub，push nil / 空操作，见"实现要点 4"）
tests/ms/                # .ms 测试脚本目录（新建）
```

---

## 实现要点

### 1. `mslang run <file>` 实现

> `src/vm/ms_vm.c` 已实现 `msVMRunFile(const char* path)`（读文件 → `msCompile` → `msVMRun`，内部处理编译错误输出），`cmdRun` 只需复用，不再手工重复读文件/编译逻辑。签名对齐同文件 `cmdDisasm`/`cmdCompile` 已用的 `struct MsCliCtx*` 约定。

```c
static int cmdRun(struct MsCliCtx* ctx) {
  if (!ctx->script) {
    fprintf(stderr, "mslang run: no input file\n");
    return 1;
  }

  msVMInit();
  MsValue result = msVMRunFile(ctx->script);
  msVMShutdown();

  return MS_IS_ERROR(result) ? 1 : 0;
}
```

### 2. 内置函数注册（M1 最小集）

在 `msVMInit` 中将以下内置函数注册到全局命名空间：

| 函数 | M1 版本说明 |
|---|---|
| `print(...)` | 将所有参数 str 化（`tpStr`）后空格分隔输出，末尾加 `\n`（`type-system.md §3.4`：`print(x)` 触发 `__str__`） |
| `len(x)` | 调用 `tpLen` |
| `type(x)` | **M1 临时降级**：返回类型名称字符串（`msTypeOf(x)->name`），非 `type-system.md §5` 规定的类型对象；完整类型对象语义（可与 `==`/`isinstance` 配合）由 P5-T078（`class-isinstance-type.md`，M2）实现 |
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

> `msVMInit` 已存在（`src/vm/ms_vm.c`），目前只初始化线程字段与类型槽；`t->globals` 现为 stub `MS_NIL_VAL`。本任务在其基础上新增全局命名空间与内置函数注册（下方标注"本任务新增"的部分）。`msRegisterBuiltin` 的函数指针形参类型为 `MsCFunction`（`c-api.md §6.1`），与各 `msBuiltinXxx` 签名一致。

```c
void msVMInit(void) {
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  t->topFrame = NULL;
  t->globals = msNewMap(64);  // 全局命名空间（本任务新增，替换既有 stub 的 MS_NIL_VAL）
  t->exception = MS_NIL_VAL;
  t->exceptStack = NULL;
  t->coro = NULL;

  gVM.intType   = &msIntType;
  gVM.floatType = &msFloatType;
  gVM.boolType  = &msBoolType;
  gVM.nilType   = &msNilType;
  gVM.strType   = &msStrType;
  gVM.bytesType = &msBytesType;
  gVM.listType  = &msListType;
  gVM.mapType   = &msMapType;
  gVM.tupleType = NULL;  // T061 落地后补
  gVM.setType   = NULL;  // T062 落地后补

  msGCInit();
  msStrInternInit();

  // 内置函数注册（本任务新增）
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
static MsValue msBuiltinPrint(MsVM* vm, MsValue* argv, int argc) {
  for (int i = 0; i < argc; i++) {
    if (i > 0) {
      fputc(' ', stdout);
    }
    struct MsType* tp = msTypeOf(argv[i]);
    if (tp->tpStr) {
      MsValue sv = tp->tpStr(vm, argv[i]);
      struct MsStr* s = (struct MsStr*) MS_AS_OBJ(sv);
      fwrite(s->data, 1, s->len, stdout);
    } else {
      fputs("?", stdout);
    }
  }
  fputc('\n', stdout);
  return MS_NIL_VAL;
}
```

### 4. `OP_GET_GLOBAL` / `OP_SET_GLOBAL` 实现

> `src/vm/ms_vm.c:272-280` 当前是 stub：`OP_GET_GLOBAL` 直接 `PUSH(MS_NIL_VAL)`，`OP_SET_GLOBAL` 为空操作，均不读写 `t->globals`。这会导致顶层变量赋值（`x := 10`）与内置函数解析（`print` 本身即通过 `OP_GET_GLOBAL` 取值）失效，M1 milestone 无法端到端跑通，故补入本任务范围。
>
> 语义对齐 `vm.md §3.2`：`STORE_GLOBAL` 与 `STORE_LOCAL` 同规则——**不弹出**栈顶值，由调用方（`compileVarDecl`/`compileAssign`，`ms_compiler.c`）统一 emit 尾随 `OP_POP` 清理。

```c
case OP_GET_GLOBAL: {
  uint32_t nameIdx = READ_AX();
  MsValue name = frame->chunk->constants[nameIdx];
  if (!MS_AS_BOOL(msMapHas(&gVM, t->globals, name))) {
    return MS_ERROR_VALUE;  // NameError（T080 前占位）
  }
  PUSH(msMapGet(&gVM, t->globals, name));
  DISPATCH();
}

case OP_SET_GLOBAL: {
  uint32_t nameIdx = READ_AX();
  MsValue name = frame->chunk->constants[nameIdx];
  MsValue val = PEEK(0);  // 不弹出，见上方 STORE_GLOBAL 语义
  MsValue r = msMapSet(&gVM, t->globals, name, val);
  if (MS_IS_ERROR(r)) {
    return r;
  }
  DISPATCH();
}
```

### 5. .ms 测试框架（`tests/ms/`）

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
print(s[0])              // 72（字节值 int，见 type-system.md §2.5）
print(s[0:1])            // H（切片返回子串）
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

print(fib(35))   // 9227465
// M1 阶段 time 模块未实现（P12 stdlib 阶段实装，见"风险与边界"），计时代码延后：
// import time
// t0 := time.now()
// t1 := time.now()
// print($"fib(35) took {round((t1-t0)*1000, 1)}ms")
// 目标：< 5s（CPython 约 2-3s，初版 mslang 可略慢）
```

---

## 验收标准（checklist）

- [ ] `OP_GET_GLOBAL`/`OP_SET_GLOBAL` 正确读写 `t->globals`（`variables.ms` 顶层 `x := 10` 后 `print(x)` 输出 10；未定义全局名报 `MS_ERROR_VALUE`）。
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
- **`print` 关键字 vs 函数**：mslang 中 `print` 是普通函数（非关键字），通过 `OP_GET_GLOBAL` + `OP_CALL` 调用，无特殊处理；`OP_GET_GLOBAL` 本身由本任务实现要点 4 补齐（此前为 stub）。
- **异常显示**：M1 阶段遇到 `MS_ERROR_VALUE` 直接打印 "RuntimeError" 到 stderr 并退出；P6 后才有完整异常层次和 traceback。
- **`time` 模块**：`tests/ms/m1/fibonacci_bench.ms` 中使用 `time.now()`；M1 阶段可不实现 time 模块（bench 注释掉 time 调用）；P12 stdlib 阶段实装。
- **暂不接入字节码缓存**：`msVMRunFile` 当前直接读文件+编译+执行，不查/写 `__mscache__`（`execution.md §6` 的 `loadChunk` 属 P7-T091~095 交付物，M1 阶段尚未实现）；`execution.md §1` 说明缓存对 VM/嵌入者透明，待 P7 落地后可在 `msVMRunFile` 内部接入，无需改动 `cmdRun` 调用方。
- **VM 生命周期 API 命名**：本任务沿用代码库既有的 `msVMInit`/`msVMRunFile`/`msVMShutdown`（`src/vm/ms_vm.c`，操作全局单例 `gVM`，`include/mslang/ms_vm.h:40` 明确注明"single instance, gVM"），而非 `c-api.md §4/§9` 描述的实例式嵌入 API（`msNew`/`msFree`/`msRunFile`，面向外部嵌入者，P11 C API 阶段交付）；两者服务不同场景，当前 CLI 内部实现以既有全局单例约定为准。
