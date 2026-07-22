# P5-T068 调用约定 / 参数绑定 / 默认值

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 `OP_CALL` 的运行时调用约定：创建 `MsFrame`，将参数绑定到局部槽，处理参数数量不匹配（错误），支持默认参数值（从 `MsFuncProto` 的默认值列表填充）。这是函数调用的基础设施，后续 T069（vararg）、T070（kwarg）在此基础上扩展。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | `MsFrame` / `MsThread` 结构 |
| P3-T043 | `MsFuncProto` / `OP_MAKE_FUNC` |
| P4-T052 | upvalue open/close（`MsClosure`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.6 CALL/RETURN 语义、§3.9 MAKE_CLOSURE 操作数、§4 调用帧、§5 闭包与 Upvalue、§9 opcode 命名映射 |

---

## 待实现（C 文件 / 结构）

### 新增文件

```
src/runtime/ms_func.c      # MsFuncProto / MsClosure 完整实现
include/mslang/ms_func.h   # msNewClosure / msClosureCall 等
```

### 修改文件

```
src/vm/ms_vm.c             # OP_CALL case（完整实现）
                            # OP_MAKE_FUNC case（完整实现）
```

---

## 实现要点

### 1. MsFuncProto 结构（P3-T043 的运行时对应）

```c
struct MsFuncProto {
  MsObject    header;
  MsChunk*    chunk;       // 字节码块
  const char* name;        // 函数名（可为 NULL，匿名函数）
  uint32_t    arity;       // 必要参数数量（不含默认参数）
  uint32_t    arityMax;    // 最大参数数量（不含 *args/**kwargs）
  uint32_t    defaultCount; // 默认参数数量
  MsValue*    defaults;    // 默认值数组（从右向左存储：defaults[0] 对应最后一个参数的默认值）
  uint32_t    localCount;  // 局部变量总数（编译期确定，不含参数本身）
  uint8_t     upvalueCount;
  bool        isAsync;
  bool        hasVararg;   // 是否有 *args（T069）
  bool        hasKwarg;    // 是否有 **kwargs（T070）
  uint32_t    kwOnlyCount; // 关键字专用参数数量（T070）
};
```

### 2. MsClosure（运行时函数对象）

```c
struct MsClosure {
  MsObject    header;
  MsFuncProto* proto;
  uint8_t     upvalueCount;
  MsUpvalue*  upvalues[];  // 内联 upvalue 指针数组
};

// OP_MAKE_FUNC 实现
case OP_MAKE_FUNC: {
  uint32_t funcIdx      = READ_U24();   // AX：3 字节常量池索引（§3 约定）
  uint8_t  upvalCount   = READ_BYTE();
  MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(frame->chunk->constants[funcIdx]);
  size_t size = sizeof(MsClosure) + upvalCount * sizeof(MsUpvalue*);
  MsClosure* cl = (MsClosure*) msGCAlloc(&msClosureType, size);
  cl->proto        = proto;
  cl->upvalueCount = upvalCount;
  for (uint8_t i = 0; i < upvalCount; i++) {
    uint8_t isLocal = READ_BYTE();
    uint8_t idx     = READ_BYTE();
    if (isLocal) {
      cl->upvalues[i] = msCaptureUpvalue(t, frame->slots + idx);
    } else {
      MsClosure* encl = (MsClosure*) frame->closure;
      cl->upvalues[i] = encl->upvalues[idx];
    }
  }
  PUSH(MS_OBJ_VAL(cl));
  DISPATCH();
}
```

### 3. OP_CALL 完整实现

```c
case OP_CALL: {
  uint8_t argc = READ_BYTE();
  MsValue callee = PEEK(argc);    // callee 在参数下面

  // 内置函数快速路径（MsCFunctionObj）
  if (MS_IS_OBJ(callee) && MS_AS_OBJ(callee)->type == &msCFunctionType) {
    MsCFunctionObj* cf = (MsCFunctionObj*) MS_AS_OBJ(callee);
    MsValue* args = t->sp - argc;
    MsValue result = cf->fn(args, argc);
    t->sp -= argc + 1;  // 弹出 args + callee
    if (MS_IS_ERROR(result)) return result;
    PUSH(result);
    DISPATCH();
  }

  // 闭包调用
  if (!MS_IS_OBJ(callee) || MS_AS_OBJ(callee)->type != &msClosureType) {
    return msTypeError(t, "not callable");
  }
  MsClosure*   cl    = (MsClosure*) MS_AS_OBJ(callee);
  MsFuncProto* proto = cl->proto;

  // 参数数量检查
  if (argc < proto->arity) {
    return msTypeError(t, "%s() missing %d required argument(s)",
                           proto->name ? proto->name : "<lambda>",
                           proto->arity - argc);
  }
  if (!proto->hasVararg && argc > proto->arityMax) {
    return msTypeError(t, "%s() takes %d argument(s) but %d were given",
                           proto->name ? proto->name : "<lambda>",
                           proto->arityMax, argc);
  }

  // 创建新帧
  MsFrame* newFrame = msNewFrame();   // 从帧池分配（T051 后改为栈分配）
  newFrame->chunk   = proto->chunk;
  newFrame->ip      = proto->chunk->code;
  newFrame->closure = (struct MsObject*) cl;
  newFrame->caller  = frame;

  // slots 指向栈上第一个参数位置（slot 0 = 第一个参数）；callee 位于
  // slots[-1]，不属于本帧局部区，由 OP_RETURN 在 frame->slots - 1 处显式回收
  newFrame->slots     = t->sp - argc;
  newFrame->slotCount = proto->localCount;  // 局部变量槽数（编译期确定）

  // 填充默认参数（defaults 从右向左存储，故按剩余默认参数数逆序索引）
  for (uint32_t i = argc; i < proto->arityMax; i++) {
    uint32_t defIdx = proto->defaultCount - 1 - (i - proto->arity);
    if (defIdx < proto->defaultCount) {
      PUSH(proto->defaults[defIdx]);
    } else {
      PUSH(MS_NIL_VAL);
    }
  }

  // 预留局部变量槽，与操作数栈工作区分离（未初始化局部槽清 NIL）
  while (t->sp < newFrame->slots + newFrame->slotCount) {
    PUSH(MS_NIL_VAL);
  }

  t->topFrame = newFrame;
  frame       = newFrame;
  ip          = newFrame->ip;
  DISPATCH();
}
```

### 4. 帧的内存管理

初版使用"帧在栈上"的方案（`MsFrame` 内联在 C 调用栈，通过 C 递归实现）。但 VM 求值循环是单个大 `eval` 函数，`goto dispatch` 循环不适合 C 递归——改用**帧链表**（每个 `MsFrame` 通过 `caller` 链表连接，分配自帧池或 GC）。

为避免每次调用 malloc 开销，使用**帧池**：

```c
#define FRAME_POOL_SIZE 256
static MsFrame  gFramePool[FRAME_POOL_SIZE];
static int      gFramePoolTop = 0;

static MsFrame* msNewFrame(void) {
  if (gFramePoolTop < FRAME_POOL_SIZE) return &gFramePool[gFramePoolTop++];
  MsFrame* f = msAlloc(sizeof(*f));   // 超出池时动态分配
  return f;
}
static void msFreeFrame(MsFrame* f) {
  if (f >= gFramePool && f < gFramePool + FRAME_POOL_SIZE) gFramePoolTop--;
  else msFree(f);
}
```

---

## 验收标准（checklist）

- [x] `func f(a, b) { return a + b }; f(1, 2)` → 3。<!-- v:ctest:test_call --><!-- v:ms:ms_m2_functions -->
- [x] `func f(a, b=10) { return a + b }; f(1)` → 11（默认参数）。<!-- v:ctest:test_call --><!-- v:ms:ms_m2_functions -->
- [x] `func f(a, b=10) { return a + b }; f(1, 2)` → 3（覆盖默认值）。<!-- v:ctest:test_call --><!-- v:ms:ms_m2_functions -->
- [x] `f()` 时参数不足 → TypeError（"missing ... required argument(s)"）。<!-- v:ctest:test_call -->
- [x] `func f(a) {}; f(1, 2)` 参数过多 → TypeError（"takes 1 but 2 were given"）。<!-- v:ctest:test_call -->
- [x] 递归：`fib(10)` 正确（帧链表深度 10）。<!-- v:ms:ms_m2_functions -->
- [ ] 闭包 upvalue：`makeCounter()` 正确（T071 配合）。<!-- v:manual:upvalue capture (msCaptureUpvalue) 是 T071 范围，OP_MAKE_FUNC 当前仅将 upvalues[] 置 NULL 存根 -->

---

## 测试用例（.ms）

```ms
// 默认参数
func greet(name, prefix="Hello") {
    return $"{prefix}, {name}!"
}
print(greet("world"))       // Hello, world!
print(greet("you", "Hi"))   // Hi, you!


// 默认参数求值时机（定义时，非调用时）
sharedDefault := [1, 2]
func f(lst=sharedDefault) { return lst }
sharedDefault.append(3)
print(f())    // [1, 2, 3]（Python 风格，共享默认值）


// 递归
func fib(n) {
    if n <= 1 { return n }
    return fib(n-1) + fib(n-2)
}
print(fib(10))   // 55
```

---

## Benchmark

```ms
// benchmarks/bench_call.ms（T067 后可运行）
func add(a, b) { return a + b }


n := 10_000_000
sum := 0
for i in range(n) {
    sum = add(sum, 1)
}
print(sum)   // 10000000
// 目标：> 10M function calls/sec
```

---

## 风险与边界

- **默认值共享**：默认值在函数定义时求值（编译器在 `OP_MAKE_FUNC` 前压栈默认值表达式，见 `P3-T043` §3 参数默认值），共享同一 `MsValue`；若默认值为可变对象（list），多次调用共享同一对象（Python 著名坑，mslang 保持相同语义，文档说明）。
- **帧池并发**：P9 并发后多个协程不能共享帧池；改为线程局部（TLS）或在每个 `MsThread` 中维护独立帧池。
