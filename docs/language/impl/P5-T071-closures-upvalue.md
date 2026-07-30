# P5-T071 闭包 upvalue open/close 运行期

> **状态**：✅ 已完成

---

## 任务目标 / 背景

完整实现 upvalue 的运行期语义：open upvalue（指向栈上局部变量）和 close upvalue（被捕获变量离开作用域后转移到堆）。这使闭包能正确捕获并在函数返回后继续访问外层变量。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T068 | 调用约定（帧创建）；`OP_MAKE_FUNC` 当前将 `upvalues[]` 各槽置 `NULL`（stub），真正捕获由本任务实现 |
| P4-T052 | `OP_GET_UPVALUE`/`OP_SET_UPVALUE`/`OP_CLOSE_UPVALUE` 三条指令 stub（仅消费操作数、维持栈平衡，不做真正访问），本任务替换为真实语义 |
| P3-T038 | `msScopeResolveUpvalue`/`locals[].captured`（编译期 upvalue 描述符解析，`src/compiler/ms_scope.h`/`ms_scope.c`） |
| P3-T040 | 作用域结束时按 `captured` 标志发射 `OP_CLOSE_UPVALUE` 替代 `OP_POP`（`src/compiler/ms_scope.c` `msScopeEnd`） |
| P4-T050 | `msGCAlloc`/`msGCPushRoot`/`msGCPopRoot`（简易 GC，`MsUpvalue` 对象分配与根保护） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.2 变量操作（`LOAD_UPVALUE`/`STORE_UPVALUE`/`CLOSE_UPVALUE` 操作码语义） |
| `vm.md` | §3.6 函数调用与返回（`RETURN`/`RETURN_NIL` 帧销毁语义） |
| `vm.md` | §3.9 闭包与类（`MAKE_CLOSURE` 操作数：upvalue 描述符跟在指令后） |
| `vm.md` | §4 调用帧（`MsFrame`/`MsThread` 结构） |
| `vm.md` | §5 闭包与 Upvalue（`struct MsUpvalue`、open/closed 状态转换） |
| `vm.md` | §9 实现层 opcode 命名映射 |
| `type-system.md` | §1.3 MsType（`traverse`/`MsVisitFn` GC 回调接口） |
| `type-system.md` | §2.12 function / closure（`MsUpvalue* upvalues[]`） |
| `gc.md` | §6 分代写屏障（跨代引用，P10 演进后适用） |
| `gc.md` | §8 精确根枚举 |
| `syntax.md` | §3.3 函数字面量与闭包 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_upvalue.h
src/runtime/ms_upvalue.c
```

### 修改文件

```
include/mslang/ms_vm.h     # MsThread 新增 openUpvalues 字段
include/mslang/ms_func.h   # MsClosure.upvalues[] 由 void* 改为 struct MsUpvalue*
src/compiler/ms_scope.h    # 编译期 struct MsUpvalue 更名为 struct MsUpvalueDesc（避免与运行期同名类型冲突，
                            # 见「实现要点 0」）；仅此一处声明类型名，ms_scope.c/ms_compiler.c 均只经
                            # `.upvalues[i].isLocal`/`.index` 字段访问，未拼写类型名，无需同步修改
src/vm/ms_vm.c             # OP_MAKE_FUNC（upvalue 回填）/ OP_GET_UPVALUE / OP_SET_UPVALUE /
                            # OP_CLOSE_UPVALUE / OP_RETURN / OP_RETURN_NIL
src/compiler/ms_disasm.c   # OP_CLOSE_UPVALUE 的 FMT_A 改为 FMT_NONE（见「实现要点 4」）
src/runtime/ms_func.c      # closureTraverse 遍历 upvalues[]
src/gc/ms_gc.c             # markRoots 遍历 t->openUpvalues 链
docs/language/vm.md        # §3.2 CLOSE_UPVALUE 操作数列改为「无」（见「实现要点 4」，标注为本任务的设计文档修正）
```

### 关键结构体

```c
// 文件: include/mslang/ms_upvalue.h（vm.md §5 canonical 定义）
typedef struct MsUpvalue {
  struct MsObject header;
  MsValue* location;          // open: 指向线程栈槽；closed: 指向 &closedVal
  MsValue closedVal;          // close 后，栈槽最新值的副本
  struct MsUpvalue* nextOpen; // t->openUpvalues 单向链表（按 location 降序排列）
} MsUpvalue;

extern struct MsType msUpvalueType;
```

### 关键函数签名

```c
// 查找或创建一个指向 location 的 open upvalue。
//
// t:        捕获发生所在的线程；location 必须指向 t 当前存活的栈窗口内。
// location: 被捕获的局部变量在操作数栈中的地址（frame->slots + 局部槽号）。
//
// 返回值由 GC 管理（msGCAlloc 分配，随 t->openUpvalues 链或调用方闭包的
// upvalues[] 被引用而存活）。同一线程内对同一栈槽的多次捕获必须返回同一个
// MsUpvalue 对象（闭包共享同一变量的前提）。
MsUpvalue* msCaptureUpvalue(struct MsThread* t, MsValue* location);

// 关闭 t->openUpvalues 链中所有 location >= slot 的 open upvalue：将当前值
// 复制到 closedVal，location 重定向到 &closedVal，并从链表摘除。
// slot 是即将失效的栈区域起始地址（函数返回时为 frame->slots；
// 作用域结束时为该作用域第一个局部变量的槽地址）。
void msCloseUpvalues(struct MsThread* t, MsValue* slot);
```

---

## 实现要点

### 0. 命名冲突与 `OP_CLOSE_UPVALUE` 编码裁决（前置说明）

- **类型名冲突**：`src/compiler/ms_scope.h` 已存在编译期 `struct MsUpvalue { bool isLocal; uint8_t index; }`（upvalue 捕获描述符，P3-T038），与本任务引入的运行期 `struct MsUpvalue`（`vm.md §5`）同名。本任务将编译期类型更名为 `struct MsUpvalueDesc`，运行期类型沿用设计文档既定的 `struct MsUpvalue`/`upvalues[]` 命名（`type-system.md §2.12`、`impl/P5-T068-call-convention.md §2`），不引入新名词造成设计文档失配。
- **`OP_CLOSE_UPVALUE` 无操作数**：`src/compiler/ms_scope.c`（`msScopeEnd`）与 `src/compiler/ms_compiler.c`（`break` 语句展开）均已用 `msChunkEmitOp`（不带操作数）发射该指令，作为「捕获局部变量离开作用域」时 `OP_POP` 的替代——语义为**关闭 `t->sp - 1` 处的 open upvalue 并弹出栈顶**，栈效应 -1。`vm.md §3.2` 现有表项写作 `A: 本地槽号`（带操作数）与已发射的字节流不符；`P4-T052` 的 stub 按 `vm.md` 原文做了 `READ_BYTE()`，同样需要修正。本任务按**已发射的字节流**（无操作数）实现，并同步修正 `vm.md §3.2` 该行与 `src/compiler/ms_disasm.c` 的 `FMT_A` → `FMT_NONE`（此为本任务对设计文档的修正，非新增分歧）。

### 1. msCaptureUpvalue（创建或复用 open upvalue）

```c
// MsThread.openUpvalues 维护按 location 降序排列的 open upvalue 单向链表；
// 捕获同一栈槽两次必须复用同一个 MsUpvalue 对象，否则多个闭包共享同一变量的
// 语义（vm.md §5 "多个闭包共享同一 struct MsUpvalue"）无法成立。
MsUpvalue* msCaptureUpvalue(struct MsThread* t, MsValue* location) {
  MsUpvalue* prev = NULL;
  MsUpvalue* cur = t->openUpvalues;
  while (cur && cur->location > location) {
    prev = cur;
    cur = cur->nextOpen;
  }
  if (cur && cur->location == location) {
    return cur;  // 已存在，复用
  }

  // msGCAlloc 可能触发 GC；新对象的字段须先全部初始化完毕，再链入
  // t->openUpvalues（否则 GC 若在初始化中途运行，会遍历到字段未定义的节点）。
  MsUpvalue* uv = (MsUpvalue*) msGCAlloc(&msUpvalueType, sizeof(*uv));
  uv->location = location;
  uv->closedVal = MS_NIL_VAL;
  uv->nextOpen = cur;
  if (prev) {
    prev->nextOpen = uv;
  } else {
    t->openUpvalues = uv;
  }
  return uv;
}
```

### 2. msCloseUpvalues（函数返回 / 作用域结束时关闭）

```c
// 关闭 t->openUpvalues 链中所有 location >= slot 的 open upvalue（slot 是即
// 将失效的栈区域起始地址）。closedVal = *location 是"堆对象写入堆引用"，
// P10 分代 GC 落地后需经 generationalWriteBarrier（gc.md §6，见「风险与边界」）。
void msCloseUpvalues(struct MsThread* t, MsValue* slot) {
  while (t->openUpvalues && t->openUpvalues->location >= slot) {
    MsUpvalue* uv = t->openUpvalues;
    uv->closedVal = *uv->location;
    uv->location = &uv->closedVal;
    t->openUpvalues = uv->nextOpen;
    uv->nextOpen = NULL;
  }
}
```

### 3. OP_MAKE_FUNC：upvalue 捕获回填

`src/vm/ms_vm.c` 现有 `OP_MAKE_FUNC`（P5-T068 落地）对每个 upvalue 描述符仅
`cl->upvalues[i] = NULL;  // T071 stub`，本任务替换为真正回填。捕获循环期间
`clVal` 尚未被任何根引用，而 `msCaptureUpvalue` 内部会 `msGCAlloc`（可能触发
GC），故须在循环期间保护 `clVal`（`msGCPushRoot`/`msGCPopRoot`，与既有
`proto` 保护手法一致）：

```c
case OP_MAKE_FUNC: {
  uint32_t funcIdx = READ_AX();
  uint8_t upvalCount = READ_BYTE();
  MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(frame->chunk->constants[funcIdx]);

  msGCPushRoot(MS_OBJ_VAL(proto));
  for (uint32_t i = 0; i < proto->defaultCount; i++) {
    proto->defaults[i] = POP();
  }

  MsValue clVal = msNewClosure(proto, upvalCount);
  MsClosure* cl = (MsClosure*) MS_AS_OBJ(clVal);
  msGCPushRoot(clVal);  // T071: protect cl across msCaptureUpvalue's msGCAlloc
  for (uint8_t i = 0; i < upvalCount; i++) {
    uint8_t isLocal = READ_BYTE();
    uint8_t idx = READ_BYTE();
    if (isLocal) {
      cl->upvalues[i] = msCaptureUpvalue(t, frame->slots + idx);
    } else {
      MsClosure* enclosing = (MsClosure*) frame->closure;
      cl->upvalues[i] = enclosing->upvalues[idx];
    }
  }
  msGCPopRoot();  // clVal
  msGCPopRoot();  // proto
  PUSH(clVal);
  DISPATCH();
}
```

### 4. OP_GET_UPVALUE / OP_SET_UPVALUE（`vm.md §3.2`）

```c
case OP_GET_UPVALUE: {
  uint8_t idx = READ_BYTE();
  MsClosure* cl = (MsClosure*) frame->closure;
  PUSH(*cl->upvalues[idx]->location);
  DISPATCH();
}
case OP_SET_UPVALUE: {
  // 与 OP_SET_LOCAL 一致：不弹出栈顶（编译器随后单独 emit OP_POP，vm.md §3.2）
  uint8_t idx = READ_BYTE();
  MsClosure* cl = (MsClosure*) frame->closure;
  *cl->upvalues[idx]->location = PEEK(0);
  DISPATCH();
}
```

### 5. OP_CLOSE_UPVALUE（作用域结束时关闭，无操作数——见「实现要点 0」）

```c
case OP_CLOSE_UPVALUE: {
  // 关闭并弹出栈顶槽，替代该局部变量本应发射的 OP_POP（src/compiler/ms_scope.c
  // msScopeEnd；栈效应 -1，与 OP_POP 相同，无额外操作数）
  msCloseUpvalues(t, t->sp - 1);
  POP();
  DISPATCH();
}
```

### 6. 函数返回时关闭 upvalue（`OP_RETURN` 与 `OP_RETURN_NIL` 两处）

`OP_RETURN_NIL` 是编译器为无显式 `return` 的函数体落尾发射的指令（`ms_compiler.c`
`compileFuncBody`），与 `OP_RETURN` 共用同一套弹帧逻辑，必须同样关闭本帧的
open upvalue——否则无显式 `return` 的闭包函数（如本文档 `makePair` 中的
`setter`）返回后，其 upvalue 仍悬垂指向已失效的栈槽：

```c
case OP_RETURN: {
  MsValue result = POP();
  msCloseUpvalues(t, frame->slots);  // T071: 关闭本帧窗口内的所有 open upvalue
  t->sp = frame->slots - 1;
  t->topFrame = frame->caller;
  if (!t->topFrame) {
    return result;  // 顶层帧：msVMRun 的 C 局部变量，不属于帧池，不可 msFreeFrame
  }
  msFreeFrame(frame);
  PUSH(result);
  frame = t->topFrame;
  DISPATCH();
}
case OP_RETURN_NIL: {
  msCloseUpvalues(t, frame->slots);  // T071: 同 OP_RETURN
  t->sp = frame->slots - 1;
  t->topFrame = frame->caller;
  if (!t->topFrame) {
    return MS_NIL_VAL;
  }
  msFreeFrame(frame);
  PUSH(MS_NIL_VAL);
  frame = t->topFrame;
  DISPATCH();
}
```

### 7. GC 集成：`msUpvalueType.traverse` + `closureTraverse` + 根枚举

`struct MsType`（`type-system.md §1.3`）没有 `tpMark` 槽，GC 钩子唯一入口是
`traverse`（`MsTraverseFn`，签名 `void (struct MsObject*, MsVisitFn, void*)`）；
`MsVisitFn` 访问的是 `MsValue*` 槽而非对象指针（`ms_gc.c` 的 `markVisit` 据此
调用内部 `markValue`）：

```c
// msUpvalueType.traverse
static void upvalueTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  MsUpvalue* uv = (MsUpvalue*) obj;
  if (uv->location == &uv->closedVal) {
    visit(&uv->closedVal, ctx);  // close 状态：值已转移到堆，由本对象负责标记
  }
  // open 状态：location 指向线程栈槽，由 markRoots 的栈枚举（gc.md §8）覆盖
}

struct MsType msUpvalueType = {
    .name = "upvalue",
    .objSize = sizeof(MsUpvalue),
    .traverse = upvalueTraverse,
};
```

`src/runtime/ms_func.c` 的 `closureTraverse` 现仅遍历 `cl->proto`（注释明确
"upvalues[] entries are a T071 stub... nothing else to traverse yet"）；本任务
须为每个非 NULL 的 `cl->upvalues[i]` 补充遍历，否则闭包持有的 `MsUpvalue`
对象无人标记，GC 可能误回收：

```c
static void closureTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  MsClosure* cl = (MsClosure*) obj;
  MsValue protoVal = MS_OBJ_VAL(cl->proto);
  visit(&protoVal, ctx);
  for (uint8_t i = 0; i < cl->upvalueCount; i++) {
    if (cl->upvalues[i]) {
      MsValue uvVal = MS_OBJ_VAL(cl->upvalues[i]);
      visit(&uvVal, ctx);
    }
  }
}
```

`t->openUpvalues` 链本身也必须是 GC 根（否则一个变量被捕获但外层函数尚未
返回时，若该 upvalue 对象未被任何闭包引用到——例如捕获发生在闭包对象创建
之前的中间态——标记阶段会漏标）。在 `src/gc/ms_gc.c` 的 `markRoots` 中新增
遍历：

```c
MsUpvalue* uv = t->openUpvalues;
while (uv) {
  markValue(MS_OBJ_VAL(uv));  // 包装为 MsValue 供 markValue 使用
  uv = uv->nextOpen;
}
```

---

## 验收标准（checklist）

- [x] `makeCounter()` 返回的函数每次调用递增 count（count 在堆上存活）。<!-- v:ctest:test_closures --><!-- v:ms:ms_m2_closures -->
- [x] 多个闭包共享同一 upvalue：均看到最新值。<!-- v:ctest:test_closures --><!-- v:ms:ms_m2_closures -->
- [x] 闭包在外层函数返回后仍可访问 upvalue（含无显式 `return` 的函数，验证 `OP_RETURN_NIL` 路径同样关闭 upvalue）。<!-- v:ctest:test_closures --><!-- v:ms:ms_m2_closures -->
- [x] GC 不误回收 open upvalue（`t->openUpvalues` 链作为 GC 根；显式 `msGCCollect()` 后闭包与其 upvalue 仍存活）。<!-- v:ctest:test_closures -->
- [x] close upvalue 后 `location == &closedVal` 且 `closedVal` 保存的是关闭时刻栈槽中的最新值（而非捕获时刻的初始值）。<!-- v:ctest:test_closures -->
- [x] 嵌套闭包（upvalue of upvalue）正确。<!-- v:ms:ms_m2_closures -->
- [x] 循环体每次迭代捕获同一名字的局部变量时，各次迭代产生互不影响的独立 upvalue（作用域结束发射的 `OP_CLOSE_UPVALUE` 存在的意义）。<!-- v:ms:ms_m2_closures -->

---

## 测试用例（.ms，`tests/ms/m2/closures.ms` + `closures.expected`）

```ms
// 基础闭包：count 在堆上存活，每次调用递增
func makeCounter() {
    count := 0
    return func() {
        count += 1
        return count
    }
}


counter := makeCounter()
print(counter())  // 1
print(counter())  // 2
print(counter())  // 3


// 共享 upvalue：getter/setter 两个闭包捕获同一个 value
func makePair() {
    value := 0
    getter := func() { return value }
    setter := func(newValue) { value = newValue }
    return (getter, setter)
}


pair := makePair()
pair[1](42)
print(pair[0]())   // 42


// 深层嵌套：upvalue of upvalue
func outer() {
    x := 1
    func middle() {
        func inner() {
            return x   // 经由 middle 的 upvalue 数组间接引用 outer 的 x
        }
        return inner
    }
    return middle
}


print(outer()()())   // 1


// 循环体内每次迭代捕获同一名字的局部变量：各次迭代产生独立 upvalue
// （msScopeEnd 在每次迭代的作用域结束处发射 OP_CLOSE_UPVALUE）
func makeAdders() {
    adders := []
    for i in range(3) {
        n := i
        adders.append(func() { return n })
    }
    return adders
}


for adder in makeAdders() {
    print(adder())   // 0, 1, 2（而非全部输出 2）
}
```

---

## Benchmark

N/A（upvalue 成本归入 T068 call bench）。

---

## 风险与边界

- **GC 根枚举**：`t->openUpvalues` 链中的所有 open upvalue 必须是 GC 根，已纳入「实现要点 7」的 `markRoots` 修改，非本节额外遗留项。
- **线程局部**：P9 多协程后，每个 `MsThread` 有独立的 `openUpvalues` 链，天然隔离。
- **未覆盖：异常展开路径**：`try`/`catch`/`finally` 的处理器栈展开（`errors.md`、P6-T081）弹出帧时同样需要关闭本帧的 open upvalue；本任务仅覆盖 `OP_RETURN`/`OP_RETURN_NIL` 两条正常返回路径，异常展开路径的 `msCloseUpvalues` 调用点留给 P6-T081 处理。
- **未覆盖：分代 GC 写屏障**：P10 分代 GC（`gc.md §6`）落地后，`msCloseUpvalues` 中 `uv->closedVal = *uv->location` 这一"堆对象写入堆引用"须改为经 `generationalWriteBarrier`，当前简易 mark-sweep GC（P4 baseline）阶段无需处理。
- **未覆盖：移动式 GC 下的自引用指针**：P10 半区复制（`gc.md §4`）迁移 `MsUpvalue` 对象时，`location == &closedVal` 这一自引用指针会因对象整体搬迁而失效，需要在复制后重新计算 `location = &newObj->closedVal`；当前 GC 不移动对象，此问题不出现。
- **既存命名不一致（非本任务引入）**：`docs/language/impl/P10-T117-root-enumeration.md` 草稿中沿用了 `MsUpvalueObj` 这一非本任务采用的名字，待该任务落地时应对齐本任务确立的 `struct MsUpvalue` 命名。
- **`msGCAlloc` 不检查返回值**：`msGCAlloc`（进而 `msAlloc`）在内存不足时直接 `abort()`，不会返回 `NULL`（`src/core/ms_alloc.c`），与本仓库其余所有 `msGCAlloc` 调用点（`msNewFuncProto`/`msNewClosure`/`OP_MAKE_FUNC` 等）行为一致，故 `msCaptureUpvalue` 无需（也不应该）额外做 `NULL` 检查。
