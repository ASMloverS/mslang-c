# P5-T072 类实例化 / __init__ / 实例属性

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现用户定义类的实例化：`OP_MAKE_CLASS`（创建 `MsType` 对象）、`Class(args)` 调用（创建 `MsInstanceObj`，调用 `__init__`）、实例属性的读写（`self.attr`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T068 | 调用约定（`msClosureCall`/`MsFrame`/frame 池） |
| P3-T044 | `OP_MAKE_CLASS` 编译，class 描述符常量的产出方 |
| P3-T045 | 调用形态（`OP_CALL` 栈布局：callee + argc 个实参） |
| P4-T060 | map（实例 attrs 字典、类 methods 字典） |
| P4-T066 | `OP_GET_ATTR`/`OP_SET_ATTR` 分派、`tpGetattr`/`tpSetattr` 槽约定 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §1.3 MsType（类型槽定义） |
| `type-system.md` | §3.1 声明、§3.2 Instance 结构、§3.6 类属性 vs 实例属性 |
| `vm.md` | §3.9 `MAKE_CLASS` 操作数、§9 opcode 命名映射 |
| `gc.md` | §4 Minor GC（traverse 约定）、§1 gcFlags（`MS_GC_FINALIZABLE`） |

---

## 待实现（C 文件）

### 新增文件

```
src/runtime/ms_class.c     # MsTypeObj / MsInstanceObj、msMetaType、instanceGetAttr/instanceSetAttr/msFindInit
include/mslang/ms_class.h  # struct MsTypeObj/MsInstanceObj 定义、msMetaType 声明
```

### 修改文件

```
src/vm/ms_vm.c             # OP_MAKE_CLASS case、dispatchCall 新增类实例化分支、OP_RETURN/OP_RETURN_NIL 的 isCtor 特判
include/mslang/ms_vm.h     # MsFrame 新增 isCtor 字段
tests/vm/test_class.c      # C 单测
tests/ms/m2/classes.ms     # .ms 端到端测试
tests/ms/m2/classes.expected
tests/CMakeLists.txt       # 注册 test_class 与 ms_add_ms_test(classes ...)
```

---

## 实现要点

### 0. 设计决策：`head.type` 直接指向类描述符

采纳 `type-system.md §3.2` 的方案：**类即类型**——用户定义类的运行时表示 `MsTypeObj` 内嵌一个 `struct MsType`，实例的 `head.type` 直接指向该嵌入结构（`&tp->mstype`），不引入共享的 `msInstanceType`。这样 `msTypeOf(inst)->name` 天然就是该类自己的名字，验收标准「`type(Foo())` → `"Foo"`」无需在 `type()` 内置函数中做任何特判即可成立；`MsInstanceObj` 因此不需要单独的 `klass` 字段——`MS_AS_OBJ(v)->type` 本身就是该实例的类。

`MsTypeObj` 自身的 `head.type` 指向 `&msMetaType`（全局静态描述符，`type(Foo)` → `"type"`）。类对象的 `call` 槽保持 `NULL`：`Foo()` 的实例化通过 `dispatchCall` 对 `msMetaType` 做**身份判定**分派（见 §6），与 `src/vm/ms_vm.c` 现有注释一致——`tpCall`/`type->call` 槽保留给 T077 的 `__call__`，不用于类实例化。

### 1. MsTypeObj（运行时用户定义类对象）

```c
// 内置类型使用静态 struct MsType；用户定义类用 MsTypeObj（GC 管理，堆分配）。
// mstype 是实例的 head.type 目标，其 baseClass/mro/methods 三个字段均为
// struct MsObject*（ms_object.h 既有声明，非 MsValue）——traverse 中按
// src/runtime/ms_func.c 的 closureTraverse 惯例，临时包装为 MsValue 供
// visit() 标记，不改变存储类型。
struct MsTypeObj {
  struct MsObject head;   // head.type == &msMetaType
  struct MsType   mstype; // 嵌入 MsType；实例的 head.type 指向 &mstype
};

extern struct MsType msMetaType;  // 所有用户定义类对象自身的类型
```

### 2. MsInstanceObj（实例对象）

```c
// type-system.md §3.2：head.type 指向所属类描述符（&tp->mstype），
// 因此无需额外的 klass 字段。
struct MsInstanceObj {
  struct MsObject   head;
  struct MsObject*  attrs;  // MsMapObj*，实例属性字典
};
```

### 3. class 描述符常量的结构

`chunk->constants` 是 `MsValue*`（`include/mslang/ms_chunk.h`），常量必须能装入 `MsValue`；因此不新增专用 GC 类型，复用既有 `MsListObj`（P3-T044 编译期用 `msNewList`/`msListAppend` 构建，与函数原型常量同期创建，同样受常量池 GC 保护）：

```
classConstIdx 指向的常量 = MsListObj，恰好 3 个元素：
  items[0]  MsValue(MsStrObj*)   类名
  items[1]  MsValue(MsListObj*)  方法对 [name, protoVal] 的列表（protoVal 指向 MsFuncProto）
  items[2]  MsValue(int)         baseCount：0 或 1（单继承）
```

### 4. OP_MAKE_CLASS 实现

`vm.md §3.9`：`MAKE_CLASS | AX: 类常量索引`，与 `P3-T044` 的 `classConstIdx` 一致，单个 3 字节操作数，指令流中不跟随任何逐方法操作数。

```c
case OP_MAKE_CLASS: {
  uint32_t classConstIdx = READ_AX();
  struct MsListObj* desc = (struct MsListObj*) MS_AS_OBJ(frame->chunk->constants[classConstIdx]);
  MsValue nameVal = desc->items[0];
  MsValue pairsVal = desc->items[1];
  int baseCount = (int) MS_AS_INT(desc->items[2]);

  MsValue baseVal = MS_NIL_VAL;
  if (baseCount == 1) {
    baseVal = POP();
    if (!MS_IS_OBJ(baseVal) || MS_AS_OBJ(baseVal)->type != &msMetaType) {
      return MS_ERROR_VALUE;  // TypeError: base must be a class (T080 placeholder)
    }
  }

  struct MsTypeObj* tp = (struct MsTypeObj*) msGCAlloc(&msMetaType, sizeof(struct MsTypeObj));
  msGCPushRoot(MS_OBJ_VAL(tp));
  memset(&tp->mstype, 0, sizeof(tp->mstype));
  // 复制类名到独立拥有的缓冲区（同 src/runtime/ms_func.c 的 msNewFuncProto 对
  // proto->name 的处理），而非直接存 nameStr->data：后者是 GC 堆对象内联数据，
  // 半区复制会移动它，裸指针将悬垂；typeDestroy（见 §5）负责释放。
  struct MsStrObj* nameStr = (struct MsStrObj*) MS_AS_OBJ(nameVal);
  char* nameCopy = MS_ALLOC_N(char, nameStr->len + 1);
  memcpy(nameCopy, nameStr->data, nameStr->len + 1);
  tp->mstype.name = nameCopy;
  tp->mstype.objSize = sizeof(struct MsInstanceObj);
  tp->mstype.traverse = instanceTraverse;
  tp->mstype.tpGetattr = instanceGetAttr;
  tp->mstype.tpSetattr = instanceSetAttr;
  tp->mstype.baseClass = MS_IS_NIL(baseVal) ? NULL : MS_AS_OBJ(baseVal);

  // 方法：逐个把 proto 包装为 upvalueCount=0 的闭包，登记进新建的方法字典
  struct MsListObj* pairs = (struct MsListObj*) MS_AS_OBJ(pairsVal);
  MsValue methodsMap = msNewMap((uint32_t) pairs->len);
  msGCPushRoot(methodsMap);
  for (uint32_t i = 0; i < pairs->len; i++) {
    struct MsListObj* pair = (struct MsListObj*) MS_AS_OBJ(pairs->items[i]);
    MsValue mClosure = msNewClosure((MsFuncProto*) MS_AS_OBJ(pair->items[1]), 0);
    msMapSet(&gVM, methodsMap, pair->items[0], mClosure);
  }
  tp->mstype.methods = MS_AS_OBJ(methodsMap);
  msGCPopRoot();  // methodsMap
  msGCPopRoot();  // tp

  PUSH(MS_OBJ_VAL(tp));
  DISPATCH();
}
```

`mstype.mro` 保持 `NULL`（未初始化字段已被 `memset` 清零）：T073 负责计算并填充；本任务的 `__init__` 查找不依赖它，直接沿 `baseClass` 单向链查找（见 §7）。

### 5. `msMetaType` 与 GC traverse/destroy

```c
// src/runtime/ms_class.c
static void instanceTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) obj;
  if (inst->attrs) {
    MsValue attrsVal = MS_OBJ_VAL(inst->attrs);
    visit(&attrsVal, ctx);  // 同 ms_func.c closureTraverse 惯例：仅标记，不回写
  }
}

static void typeTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsTypeObj* tp = (struct MsTypeObj*) obj;
  if (tp->mstype.baseClass) {
    MsValue v = MS_OBJ_VAL(tp->mstype.baseClass);
    visit(&v, ctx);
  }
  if (tp->mstype.methods) {
    MsValue v = MS_OBJ_VAL(tp->mstype.methods);
    visit(&v, ctx);
  }
  if (tp->mstype.mro) {
    MsValue v = MS_OBJ_VAL(tp->mstype.mro);
    visit(&v, ctx);
  }
}

// 类对象自身的类型：objSize 描述 MsTypeObj 本体大小（供 GC 复制时计算）；
// destroy 只需释放 name 的拥有缓冲区（baseClass/methods/mro 均为 GC 自管理
// 对象，无需手动释放）。
static void typeDestroy(struct MsObject* obj) {
  struct MsTypeObj* tp = (struct MsTypeObj*) obj;
  msFree((void*) tp->mstype.name);
}

struct MsType msMetaType = {
    .name = "type",
    .objSize = sizeof(struct MsTypeObj),
    .traverse = typeTraverse,
    .destroy = typeDestroy,
};
```

### 6. 实例化分派（`dispatchCall` 新增分支，替代不存在的 `msCallFn`）

`src/vm/ms_vm.c` 的 `dispatchCall`（T068 落地）目前只识别 `msNativeFnType`（身份判定，快速路径）与 `msClosureType`；`Foo()` 需要在此新增第三条身份判定分支。VM 没有可从 C 重入 eval 循环的 `msCallFn`——本任务复用既有「压帧」机制：`__init__` 作为一个普通闭包帧被压入，返回时由 `OP_RETURN`/`OP_RETURN_NIL` 的 `isCtor` 特判把返回值替换为实例。

```c
// MsFrame 新增字段（include/mslang/ms_vm.h）：
//   bool isCtor;  // true 时 OP_RETURN/OP_RETURN_NIL 用 slots[0]（self）替代返回值
// msNewFrame()/msClosureCall() 必须显式将新分配或复用的帧的 isCtor 置 false，
// 避免 frame 池复用时残留的 true 值污染普通函数调用。

// src/vm/ms_vm.c：dispatchCall 新增分支（置于 msNativeFnType 判定之后、
// msClosureType 判定之前）。callee->type == &msMetaType 身份判定类似
// msNativeFnType 的处理方式，不经过通用 tpCall/type->call 槽（该槽保留给 T077）。
static MsFrame* dispatchClassCall(struct MsThread* t, struct MsTypeObj* tp, uint8_t argc, bool* ok) {
  struct MsInstanceObj* inst =
      (struct MsInstanceObj*) msGCAlloc(&tp->mstype, sizeof(struct MsInstanceObj));
  msGCPushRoot(MS_OBJ_VAL(inst));
  inst->attrs = MS_AS_OBJ(msNewMap(0));
  msGCPopRoot();

  MsValue initFn = msFindInit(&gVM, &tp->mstype);  // 见 §7：沿 baseClass 单向链查找
  if (MS_IS_NIL(initFn)) {
    if (argc != 0) {
      *ok = false;  // TypeError: Foo() 无 __init__ 却传入实参 (T080 placeholder)
      return NULL;
    }
    t->sp = t->sp - argc - 1;  // 弹出 callee（无实参）
    PUSH(MS_OBJ_VAL(inst));
    *ok = true;
    return NULL;
  }
  if (!MS_IS_OBJ(initFn) || MS_AS_OBJ(initFn)->type != &msClosureType) {
    *ok = false;  // TypeError: __init__ 被非法遮蔽为不可调用值 (T080 placeholder)
    return NULL;
  }

  // 用实例覆盖 callee 槽：栈从 [Foo, arg0..argc-1] 变为 [inst, arg0..argc-1]，
  // 与绑定方法调用的栈形态完全一致，msClosureCall 不需要挪动参数。
  *(t->sp - argc - 1) = MS_OBJ_VAL(inst);
  struct MsClosure* initCl = (struct MsClosure*) MS_AS_OBJ(initFn);
  MsFrame* newFrame = msClosureCall(t, initCl, (uint32_t) argc + 1);
  if (!newFrame) {
    *ok = false;  // TypeError: __init__ 参数个数不匹配 (T080 placeholder)
    return NULL;
  }
  newFrame->isCtor = true;
  *ok = true;
  return newFrame;
}
```

在 `dispatchCall` 内，`msNativeFnType` 判定之后插入：

```c
if (MS_IS_OBJ(callee) && MS_AS_OBJ(callee)->type == &msMetaType) {
  return dispatchClassCall(t, (struct MsTypeObj*) MS_AS_OBJ(callee), argc, ok);
}
```

`OP_RETURN`/`OP_RETURN_NIL` 各增一行（`ms_vm.c:835-865`）：仍需 `POP()` 以保持栈平衡，只是丢弃该值，改用 `frame->slots[0]`（`__init__` 的 `self`）：

```c
case OP_RETURN: {
  MsValue popped = POP();
  MsValue result = frame->isCtor ? frame->slots[0] : popped;
  // ……（其余不变）
}
case OP_RETURN_NIL: {
  MsValue result = frame->isCtor ? frame->slots[0] : MS_NIL_VAL;
  // ……（其余不变）
}
```

### 7. 实例属性读写（`tpGetattr`/`tpSetattr`）与 `__init__` 查找

签名对齐 `include/mslang/ms_object.h` 的 `MsBinaryFn`/`MsTernaryFn`（均含 `struct MsVM* vm` 首参）。`msMapGet` 在键不存在时返回 `MS_NIL_VAL`（`ms_map.h:51-54`），与「属性值本身为 `nil`」无法区分，因此必须先用 `msMapHas` 判定存在性。

```c
// src/runtime/ms_class.c

// 沿 baseClass 单向链查找 __init__（T072 阶段不依赖 T073 的 mstype.mro）。
// gInitNameVal 是 "__init__" 的字符串常量，msVMInit() 时创建一次并缓存为
// 全局 MsValue（避免每次实例化都 msNewStr 新建，也避免每次比较都做内容匹配）。
static MsValue msFindInit(struct MsVM* vm, struct MsType* tp) {
  while (tp) {
    if (tp->methods) {
      MsValue m = msMapGet(vm, MS_OBJ_VAL(tp->methods), gInitNameVal);
      if (!MS_IS_NIL(m)) {
        return m;
      }
    }
    tp = tp->baseClass ? &((struct MsTypeObj*) tp->baseClass)->mstype : NULL;
  }
  return MS_NIL_VAL;
}

// 实例属性优先；未命中则查本类（不沿 baseClass 链，继承查找留待 T073 的
// MRO 完善）methods 字典。此处返回的是**未绑定**闭包——self 不会自动传入，
// 调用方须自行以 `Foo.method(instance, ...)` 等价方式传参；`obj.method()`
// 语法糖需要 MsBoundMethodObj（T073）在此结果外包一层绑定，T072 阶段
// `obj.method()` 会因缺少 self 实参报参数个数错误，属已知边界（见风险与边界）。
static MsValue instanceGetAttr(struct MsVM* vm, MsValue obj, MsValue name) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) MS_AS_OBJ(obj);
  if (MS_AS_BOOL(msMapHas(vm, MS_OBJ_VAL(inst->attrs), name))) {
    return msMapGet(vm, MS_OBJ_VAL(inst->attrs), name);
  }
  struct MsType* tp = MS_AS_OBJ(obj)->type;
  if (tp->methods) {
    MsValue m = msMapGet(vm, MS_OBJ_VAL(tp->methods), name);
    if (!MS_IS_NIL(m)) {
      return m;
    }
  }
  return MS_NIL_VAL;  // 未命中：OP_GET_ATTR 据此判定 AttributeError
}

static MsValue instanceSetAttr(struct MsVM* vm, MsValue obj, MsValue name, MsValue val) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) MS_AS_OBJ(obj);
  return msMapSet(vm, MS_OBJ_VAL(inst->attrs), name, val);
}
```

---

## 验收标准（checklist）

<!-- v:... 标签供 verify_task.py 自动勾选，见 _template.md -->
- [ ] 编译通过，无警告（`cmake --build build`）。 <!-- v:build -->
- [ ] C 单测通过。 <!-- v:ctest:test_class -->
- [ ] `.ms` 端到端测试输出与期望一致。 <!-- v:ms:classes -->
- [ ] `class Foo {}; Foo()` → 创建 MsInstanceObj，`head.type == &Foo 自身的 mstype`。 <!-- v:ctest:test_class -->
- [ ] `class Foo { func __init__(self, x) { self.x = x } }; f := Foo(42); f.x` → 42。 <!-- v:ms:classes -->
- [ ] 实例属性可读写：`f.x = 100; f.x` → 100。 <!-- v:ms:classes -->
- [ ] `f.x = nil; f.x` → `nil`（与「属性不存在」区分，`msMapHas` 而非 nil 判定）。 <!-- v:ctest:test_class -->
- [ ] `f.missing` → AttributeError（`MS_ERROR_VALUE`，T080 占位）。 <!-- v:ms:classes -->
- [ ] `type(Foo())` → `"Foo"`（无需 `type()` 特判，`head.type` 直接是该类描述符）。 <!-- v:ms:classes -->
- [ ] 无 `__init__` 的类被传参 → TypeError（`Foo()` 无参正常构造）。 <!-- v:ctest:test_class -->
- [ ] `class Dog extends Animal {}`（`Dog` 无 `__init__`）→ 构造时调用 `Animal.__init__`（`baseClass` 单向链查找）。 <!-- v:ms:classes -->
- [ ] GC：实例被正确 mark（`attrs` 字典可达）；类对象被正确 mark（`baseClass`/`methods` 可达）。 <!-- v:manual:需人工用 gc.stats() 观察 -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/vm/test_class.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_vm.h"

static MsValue run(const char* src) {
  MsCompileResult r = msCompile(src, strlen(src), "<t>");
  msVMInit();
  MsValue v = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static void testInstantiateAndInit(void) {
  MsValue v = run("class Foo { func __init__(self, x) { self.x = x } }\nFoo(42).x");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 42, "Foo(42).x == 42");
}

static void testNilAttrVsMissingAttr(void) {
  MsValue v = run("class Foo {}\nf := Foo()\nf.x = nil\nf.x");
  MS_ASSERT_TRUE(MS_IS_NIL(v), "f.x explicitly nil, not AttributeError");
}

static void testNoInitTakesNoArgs(void) {
  MsValue v = run("class Foo {}\nFoo(1)");
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "Foo(1) with no __init__ is a TypeError");
}

int main(void) {
  MS_RUN(testInstantiateAndInit);
  MS_RUN(testNilAttrVsMissingAttr);
  MS_RUN(testNoInitTakesNoArgs);
  return msTestSummary();
}
```

### `.ms` 端到端测试（`tests/ms/m2/classes.ms` + `classes.expected`）

```ms
// class: instantiation/__init__/instance attrs/type() (T072)

class Animal {
    func __init__(self, name) {
        self.name = name
    }
}


class Dog extends Animal {}


animal := Animal("Rex")
print(animal.name)      // Rex
print(type(animal))     // Animal

dog := Dog("Fido")
print(dog.name)          // Fido，__init__ 沿 baseClass 继承自 Animal
print(type(dog))         // Dog

dog.age = 3
print(dog.age)           // 3
```

方法调用语法（`obj.method()`）与继承覆盖需要 T073 的 `MsBoundMethodObj`，其端到端测试见 `P5-T073-method-binding-mro.md`，不在本任务范围内。

---

## Benchmark

```ms
// benchmarks/bench_class.ms
import time


class Vec {
    func __init__(self, x, y) {
        self.x = x
        self.y = y
    }
}


ITERATIONS := 1_000_000

start := time.perfCounter()
for i in range(ITERATIONS) {
    v := Vec(i, i + 1)
}
elapsed := time.perfCounter() - start
print($"耗时: {elapsed * 1000:.2f} ms")
```

**指标参考**：> 1M instantiation/sec。

---

## 风险与边界

- **实例的 `attrs` 字典开销**：`msNewMap(0)` 按 `ms_map.h` 最小容量（8 槽）分配（`sizeof(MsMapObj) + 8 * sizeof(MsMapEntry)`，非早期草稿估算的约 100B）；高频创建小对象时内存压力大，后续可优化为内联槽（`__slots__`）。
- **`__init__` 返回值**：`type-system.md §3.4` 未规定返回值处理；本任务采纳「忽略 `__init__` 返回值，构造表达式恒求值为新实例」（`dispatchClassCall` 用 `frame->slots[0]` 替代返回值即是此语义的落地），该决策回写设计文档留待后续 Info 级别整理任务处理。
- **`__init__` 内 `raise`**：半构造的 `inst` 已通过 `frame->slots[0]` 可达（在值栈上，是 GC 根），异常经由 `OP_RETURN`/T079-T083 的异常传播机制正常向上冒泡；`isCtor` 标记不影响异常路径（异常路径不经过 `OP_RETURN`）。
- **frame 池复用与 `isCtor`**：`msNewFrame()`/`msClosureCall()` 必须显式重置 `isCtor = false`，否则前一次构造调用遗留的 `true` 会污染复用帧的普通函数调用。
- **方法访问未绑定**：`instanceGetAttr` 对本类 `methods` 的命中返回未绑定闭包（无 self），`obj.method()` 在 T073 落地前会因缺少首个实参报错；仅 `__init__` 通过 `dispatchClassCall` 手动完成了 self 绑定。
- **继承查找的范围**：`__init__` 沿 `baseClass` 单向链向上查找（T072 自实现，不依赖 `mstype.mro`）；其余方法/属性的完整 MRO 查找留给 T073。
- **`__del__` 与可终结标志**：本任务不查找 `__del__`、不设置 `MS_GC_FINALIZABLE`（`gc.md §1`/`ms_object.h` `MsGcFlags`）；可终结对象支持留给 T076/P10-T123，实例化路径届时需在 `dispatchClassCall` 中补一次 MRO 查找。
- **类属性**：`type-system.md §3.6` 规定类属性存于 `MsType.methods`；`P3-T044` 现阶段类体顶层赋值仍报编译错误（TODO），本任务不涉及。
- **多继承**：`baseCount` 恒为 0 或 1（单继承，`type-system.md §3.1`）；`OP_MAKE_CLASS` 对 `baseCount != {0,1}` 的处理是编译器产出的常量描述符本身的不变量，运行时不做额外校验。
