# P5-T069 vararg（`...args` 参数收集）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

扩展调用约定以支持 `...args` 可变位置参数：函数定义中的 `...args` 参数将多余的位置参数收集为 list；调用时的 `...iterable` 展开（`OP_CALL_EX`）将可迭代对象展开为位置参数。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T068 | 调用约定基础（`msClosureCall`/`MsFuncProto`/`arityMax`） |
| P4-T059 | list（vararg 收集结果为 list） |
| P4-T065 | 迭代协议（`...iterable` 展开） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.6 CALL_EX/CALL_KW 语义、§4 调用帧 |
| `syntax.md` | §2.1 ParamList/ArgList 文法、§3.4 可变参数 |

---

## 待实现（C 文件）

```
src/runtime/ms_func.c    # msClosureCall 扩展：hasVararg 分支（参数绑定，T068 延续）
src/vm/ms_vm.c            # OP_CALL_EX（展开调用）
```

---

## 实现要点

### 1. 函数定义侧（收集 `...args`，`msClosureCall` 内扩展）

```c
// proto->arityMax 语义不变（T068，ms_func.h）：不含 vararg 槽的最大位置参数数
// （含默认值参数）。hasVararg 时，超出 arityMax 的多余位置参数收集为 list，
// 放入 slots[arityMax]。此步骤须在 T068 的默认值填充**之后**、局部槽预留
// （while 循环）**之前**执行，否则会与默认值/局部槽逻辑相互覆盖 sp。

// msClosureCall（ms_func.c，T068 主体不变，此处仅新增 hasVararg 分支）：
// ...（arity 检查、默认值填充，见 T068）...

if (proto->hasVararg) {
  uint32_t varargCount = argc > proto->arityMax ? argc - proto->arityMax : 0;
  MsValue varargList = msNewList(varargCount);
  msGCPushRoot(varargList);  // 保护跨 msListAppend 可能触发的 GC
  MsListObj* vl = (MsListObj*) MS_AS_OBJ(varargList);
  for (uint32_t i = 0; i < varargCount; i++) {
    msListAppend(vl, newFrame->slots[proto->arityMax + i]);
  }
  msGCPopRoot();
  newFrame->slots[proto->arityMax] = varargList;
  t->sp = newFrame->slots + proto->arityMax + 1;  // 去掉多余的原始参数
}
// ...（局部槽预留 while 循环，见 T068）...
```

### 2. 调用侧（`...iterable` 展开 → `OP_CALL_EX`）

`OP_CALL_EX` 无操作数（`vm.md` §3.6），仅展开末尾参数 list；kwargs 合并由独立指令
`OP_CALL_KW` 承担（T070），本任务不涉及。

```c
// OP_CALL_EX（无操作数）
// 栈：[callee, argsSeq]（argsSeq 为待展开为全部位置实参的可迭代对象；
// 若源码为 f(a, b, ...rest) 形式，由编译器在此指令前将 a、b 与 rest
// 合并为一个序列再压栈——本指令只负责展开单个完整序列，不感知合并细节）
case OP_CALL_EX: {
  MsValue argsSeq = POP();
  uint32_t argc;
  if (!msExpandToStack(t, argsSeq, &argc)) {
    return MS_ERROR_VALUE;  // TypeError（T080 placeholder）：非可迭代
  }
  bool ok;
  MsFrame* newFrame = dispatchCall(t, (uint8_t) argc, &ok);
  if (!ok) return MS_ERROR_VALUE;
  if (newFrame) frame = newFrame;
  DISPATCH();
}

// 将可迭代对象的元素展开压入操作数栈，*outCount 返回压入元素数。
// 逻辑与 ms_vm.c 既有的 collectIntoList（tpIter/tpNext 调用约定、
// msGCPushRoot 保护迭代器）保持一致，区别仅在于此处压栈而非收集进 list。
// 返回 false（不修改 *outCount）表示 seq 不可迭代或迭代出错（T080 placeholder）。
static bool msExpandToStack(MsThread* t, MsValue seq, uint32_t* outCount) {
  MsType* seqType = msTypeOf(seq);
  if (!seqType->tpIter) return false;
  MsValue iter = seqType->tpIter(&gVM, seq);
  if (MS_IS_ERROR(iter)) return false;
  msGCPushRoot(iter);
  MsType* iterType = msTypeOf(iter);
  uint32_t count = 0;
  for (;;) {
    MsValue v = iterType->tpNext(&gVM, iter);
    if (MS_IS_NIL(v)) break;  // StopIteration 哨兵（T065）
    PUSH(v);
    count++;
  }
  msGCPopRoot();
  *outCount = count;
  return true;
}
```

`dispatchCall(t, argc, &ok)` 是从现有 `OP_CALL`（`ms_vm.c`，T068）case 体中提取出的共享静态函数：内置函数快速路径（调用即压入结果，返回 NULL）+ `msClosureCall` 闭包分发逻辑不变，仅从 `case OP_CALL:` 内联代码中抽出，供 `OP_CALL` 与 `OP_CALL_EX` 共用。返回值约定：`*ok == false` 时调用方需 `return MS_ERROR_VALUE`（不可调用或 arity 不匹配）；`*ok == true` 且返回非 NULL 时调用方需 `frame = <返回值>` 切换到新闭包帧；返回 NULL 且 `*ok == true` 表示内置函数调用已直接压入结果，无需切帧。

---

## 验收标准（checklist）

- [ ] `func f(first, ...args) { return args }; f(1, 2, 3)` → `[2, 3]`（list）。
- [ ] `func f(a, ...args) { return a, args }; f(1, 2, 3)` → `1, [2, 3]`。
- [ ] `func f(first, ...args) {}; f(1)` → args = `[]`（空 list）。
- [ ] `f(...[1, 2, 3])` → 等价 `f(1, 2, 3)`（列表展开）。
- [ ] `f(...range(3))` → 等价 `f(0, 1, 2)`（range 展开）。
- [ ] `func f(a, ...args) {}; f(...[1, 2, 3])` → a=1, args=[2, 3]。

---

## 测试用例（.ms）

```ms
func sum(first, ...nums) {
    total := first
    for n in nums { total += n }
    return total
}
print(sum(1, 2, 3, 4, 5))   // 15

// 展开调用
lst := [10, 20, 30]
print(sum(...lst))            // 60

// 混合
func f(a, b, ...rest) { return a + b + sum(...rest) }
print(f(1, 2, 3, 4))         // 10
```

---

## Benchmark

N/A（vararg 成本归入 T068 call bench）。

---

## 风险与边界

- **vararg 与默认值**：`func f(a, b=1, ...args)` 中，`arityMax`（=2，含 `b` 的默认值槽）之后才是 vararg 收集边界；若 `f(10)` 调用，b=1（默认填充），args=[]；默认值填充与 vararg 收集须按顺序执行（先默认值、后 vararg），否则会相互覆盖 sp。
- **展开大型 iterable**：`f(...range(10M))` 会将 1000 万个值压栈，可能栈溢出；VM 未检查栈深度（初版不防护，文档提示）。
