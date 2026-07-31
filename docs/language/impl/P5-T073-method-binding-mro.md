# P5-T073 方法绑定 + MRO 查找

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现方法绑定（`MsBoundMethodObj`）和方法解析顺序（MRO）。当实例访问方法时，返回绑定了 `self` 的方法对象；调用时自动将 `self` 作为第一个参数。单继承下 MRO 为线性父类链。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T072 | 实例 / 类对象定义（`MsTypeObj`/`MsInstanceObj`/`msMetaType`/`instanceGetAttr`/`msFindInit`，已落地于 `src/runtime/ms_class.c`） |
| P5-T068 | 调用约定（`msClosureCall`/`MsFrame`/frame 池，绑定方法调用复用此机制） |
| P5-T071 | 闭包对象（`MsClosure`/`msClosureType`，方法体的运行时表示） |
| P4-T059 | list（`MsListObj`/`msNewList`/`msListAppend`，MRO 列表的载体） |
| P4-T060 | map（方法字典，`MsType.methods`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §1.3 MsType（`baseClass`/`mro`/`methods` 字段） |
| `type-system.md` | §3.2 Instance 结构、§3.3 MRO 与方法查找、§3.6 类属性 vs 实例属性 |
| `vm.md` | §3.9 `MAKE_CLASS`（MRO 计算时机） |
| `gc.md` | §8 精确根枚举（`mro`/`func`/`self` 的 traverse 要求） |

---

## 待实现（C 文件）

### 修改文件

```
src/runtime/ms_class.c    # 新增 MsBoundMethodObj/msBoundMethodType/msNewBoundMethod/msBuildMRO/
                           # msTypeLookupMethodMRO；改造 instanceGetAttr 接入绑定
include/mslang/ms_class.h  # 新增上述符号的声明
src/vm/ms_vm.c             # OP_MAKE_CLASS 尾部调用 msBuildMRO；dispatchCall 新增
                           # msBoundMethodType 身份分派分支（dispatchBoundMethodCall）
tests/vm/test_method_binding.c   # C 单测（沿用 tests/vm/test_class.c 的 run() 惯例）
tests/ms/m2/method_binding.ms    # .ms 端到端测试
tests/ms/m2/method_binding.expected
tests/CMakeLists.txt       # 注册 test_method_binding 与 ms_add_ms_test(method_binding ...)
```

---

## 实现要点

### 0. 设计决策：MRO 用 `MsListObj` 承载；不引入 `object` 根类

`include/mslang/ms_object.h` 中 `struct MsType.mro` 的字段类型是 `struct MsObject*`（GC 管理的堆对象，非裸指针数组），且 `src/runtime/ms_class.c` 的 `typeTraverse` 已经把它当 `MsValue` 包装访问（`MS_OBJ_VAL(tp->mstype.mro)`）。因此本任务用既有 `MsListObj`（P4-T059）承载 MRO：`tp->mstype.mro` 指向一个元素为 `MS_OBJ_VAL(struct MsTypeObj*)` 的列表，长度即 `MsListObj.len`，不再需要单独的 `mroLen` 字段。

`OP_MAKE_CLASS`（`src/vm/ms_vm.c:505-544`）无 `extends` 时 `tp->mstype.baseClass = NULL`，代码库中不存在全局 `object` 根类，也没有任何任务负责创建它。本任务**不引入** `object` 根类：MRO 链沿 `baseClass` 走到 `NULL` 为止。

### 1. `MsBoundMethodObj`

```c
// 文件: include/mslang/ms_class.h
struct MsBoundMethodObj {
  struct MsObject head;  // head.type == &msBoundMethodType
  MsValue func;          // MsClosure*（本任务仅支持闭包方法）
  MsValue self;          // 绑定的实例
};

extern struct MsType msBoundMethodType;

MsValue msNewBoundMethod(MsValue func, MsValue self);
```

```c
// 文件: src/runtime/ms_class.c
static void boundMethodTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsBoundMethodObj* bm = (struct MsBoundMethodObj*) obj;
  visit(&bm->func, ctx);
  visit(&bm->self, ctx);
}

struct MsType msBoundMethodType = {
    .name = "method",
    .objSize = sizeof(struct MsBoundMethodObj),
    .traverse = boundMethodTraverse,
};

MsValue msNewBoundMethod(MsValue func, MsValue self) {
  struct MsBoundMethodObj* bm =
      (struct MsBoundMethodObj*) msGCAlloc(&msBoundMethodType, sizeof(struct MsBoundMethodObj));
  bm->func = func;
  bm->self = self;
  return MS_OBJ_VAL(bm);
}
```

调用方须在调用 `msNewBoundMethod` 前，用 `msGCPushRoot` 保护 `func`/`self`（它们通常是从别处解引用出来的、尚未再次落到 VM 值栈上的临时值），分配完成后再 `msGCPopRoot`（见 §4 的 `instanceGetAttr` 用例）。

### 2. MRO 构建（单继承，沿 `baseClass` 链）

```c
// 文件: include/mslang/ms_class.h
void msBuildMRO(struct MsTypeObj* cls);
```

```c
// 文件: src/runtime/ms_class.c
// 单继承 MRO：[cls, parent, grandparent, ...]，链尾为 baseClass == NULL 处
// （本任务不引入 object 根类，见 ss0）。cls->mstype.baseClass 此时必须已经
// 由 OP_MAKE_CLASS 设置好（msBuildMRO 须在其后调用，见 ss6）。
void msBuildMRO(struct MsTypeObj* cls) {
  uint32_t len = 0;
  for (struct MsTypeObj* cur = cls; cur != NULL;
       cur = cur->mstype.baseClass ? (struct MsTypeObj*) cur->mstype.baseClass : NULL) {
    len++;
  }

  MsValue mroVal = msNewList(len);
  msGCPushRoot(mroVal);
  struct MsListObj* mroList = (struct MsListObj*) MS_AS_OBJ(mroVal);
  for (struct MsTypeObj* cur = cls; cur != NULL;
       cur = cur->mstype.baseClass ? (struct MsTypeObj*) cur->mstype.baseClass : NULL) {
    msListAppend(mroList, MS_OBJ_VAL(cur));
  }
  cls->mstype.mro = MS_AS_OBJ(mroVal);
  msGCPopRoot();
}
```

继承深度没有硬编码上限（两趟遍历直接按实际长度分配列表，不使用固定大小的临时数组），因此不存在原草稿 `chain[64]` 的越界写风险。

### 3. MRO 查找

```c
// 文件: include/mslang/ms_class.h
MsValue msTypeLookupMethodMRO(struct MsVM* vm, struct MsType* tp, MsValue name);
```

```c
// 文件: src/runtime/ms_class.c
// 沿 tp->mro（msBuildMRO 预计算的 MsListObj）依次查各类自身的 methods 字典；
// 与 instanceGetAttr/msFindInit 一致的 miss 约定：返回 MS_ERROR_VALUE，不用
// MS_NIL_VAL（nil 是合法的属性值，不能复用作「未找到」哨兵）。
MsValue msTypeLookupMethodMRO(struct MsVM* vm, struct MsType* tp, MsValue name) {
  if (!tp->mro) {
    return MS_ERROR_VALUE;
  }
  struct MsListObj* mro = (struct MsListObj*) tp->mro;
  for (uint32_t i = 0; i < mro->len; i++) {
    struct MsTypeObj* cur = (struct MsTypeObj*) MS_AS_OBJ(mro->items[i]);
    if (cur->mstype.methods) {
      MsValue m = msMapGet(vm, MS_OBJ_VAL(cur->mstype.methods), name);
      if (!MS_IS_NIL(m)) {
        return m;
      }
    }
  }
  return MS_ERROR_VALUE;
}
```

### 4. `instanceGetAttr` 接入绑定（修改 T072 已落地的函数）

`src/runtime/ms_class.c` 现有 `instanceGetAttr`（P5-T072）只查本类 `tp->methods` 且返回未绑定闭包（其注释明确写「self-binding via MsBoundMethodObj lands in T073」）。本任务将其改为：属性优先 → 沿 MRO 查方法 → 命中则包一层 `MsBoundMethodObj`。

```c
// 文件: src/runtime/ms_class.c（替换现有 instanceGetAttr 函数体）
MsValue instanceGetAttr(struct MsVM* vm, MsValue obj, MsValue name) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) MS_AS_OBJ(obj);
  if (MS_AS_BOOL(msMapHas(vm, MS_OBJ_VAL(inst->attrs), name))) {
    return msMapGet(vm, MS_OBJ_VAL(inst->attrs), name);
  }
  struct MsType* tp = MS_AS_OBJ(obj)->type;
  MsValue m = msTypeLookupMethodMRO(vm, tp, name);
  if (MS_IS_ERROR(m)) {
    return MS_ERROR_VALUE;  // not found: OP_GET_ATTR propagates this as AttributeError
  }
  // obj 已被 OP_GET_ATTR 的调用点 POP 出值栈（见 ms_vm.c 的 OP_GET_ATTR case），
  // 不再是 GC 根；m 虽经由 tp->mstype.methods 可达，但 tp 本身此刻也只经由
  // obj->type 间接引用。msNewBoundMethod 内部的 msGCAlloc 可能触发 GC，
  // 故显式保护两者。
  msGCPushRoot(obj);
  msGCPushRoot(m);
  MsValue bound = msNewBoundMethod(m, obj);
  msGCPopRoot();  // m
  msGCPopRoot();  // obj
  return bound;
}
```

### 5. `dispatchCall` 新增绑定方法分支

`src/vm/ms_vm.c` 的 `dispatchCall`（T068 落地，T072 已加过一次 `msMetaType` 分支）依次按对象身份识别 `msNativeFnType` → `msMetaType`（T072）→ `msClosureType`；本任务在 `msMetaType` 判定之后再插入一条 `msBoundMethodType` 分支，复用 T072 `dispatchClassCall` 确立的「原地覆盖 callee 槽为 self」范式。

```c
// 文件: src/vm/ms_vm.c
// obj.method(args...): bound-method call dispatch, identity-matched on
// msBoundMethodType (same style as dispatchClassCall's msMetaType match).
// Overwrites the callee slot with self so msClosureCall sees exactly the
// stack shape it expects: [self, arg0..argc-1].
static MsFrame* dispatchBoundMethodCall(struct MsThread* t, struct MsBoundMethodObj* bm, uint8_t argc, bool* ok) {
  if (!MS_IS_OBJ(bm->func) || MS_AS_OBJ(bm->func)->type != &msClosureType) {
    *ok = false;  // TypeError: bound value is not callable (T080 placeholder) --
                  // unreachable while all methods are closures (T072/T073);
                  // reserved for future non-closure callables (T077).
    return NULL;
  }
  *(t->sp - argc - 1) = bm->self;
  struct MsClosure* cl = (struct MsClosure*) MS_AS_OBJ(bm->func);
  MsFrame* newFrame = msClosureCall(t, cl, (uint32_t) argc + 1);
  if (!newFrame) {
    *ok = false;  // TypeError: arity mismatch (T080 placeholder)
    return NULL;
  }
  *ok = true;
  return newFrame;
}
```

在 `dispatchCall` 内，`msMetaType` 判定之后插入：

```c
if (MS_IS_OBJ(callee) && MS_AS_OBJ(callee)->type == &msBoundMethodType) {
  return dispatchBoundMethodCall(t, (struct MsBoundMethodObj*) MS_AS_OBJ(callee), argc, ok);
}
```

`dispatchCall` 由 `OP_CALL`/`OP_CALL_EX` 共用，因此二者自动获得绑定方法调用能力；`OP_CALL_KW` 不经过 `dispatchCall`（硬编码只接受 `msClosureType`），`obj.method(**kwargs)` 暂不支持，留待风险与边界说明。

### 6. `OP_MAKE_CLASS` 接入 `msBuildMRO`

`src/vm/ms_vm.c:505-544` 的 `OP_MAKE_CLASS` 在 `tp->mstype.baseClass` 赋值、`tp->mstype.methods` 赋值完成后（此时 `tp` 仍在 `msGCPushRoot` 保护区间内），追加一行：

```c
      tp->mstype.methods = MS_AS_OBJ(methodsMap);
      msGCPopRoot();  // methodsMap
      msBuildMRO(tp);  // T073: 必须在 baseClass 已赋值之后调用
      msGCPopRoot();  // tp
```

---

## 验收标准（checklist）

<!-- v:... 标签供 verify_task.py 自动勾选，见 _template.md -->
- [x] 编译通过，无警告（`cmake --build build`）。 <!-- v:build -->
- [x] C 单测通过。 <!-- v:ctest:test_method_binding -->
- [x] `.ms` 端到端测试输出与期望一致。 <!-- v:ms:ms_m2_method_binding -->
- [x] `class A { func f(self) {} }; A().f` → `MsBoundMethodObj`（`self` 已绑定）。 <!-- v:ctest:test_method_binding -->
- [x] `A().f()` 自动将 `A` 实例作为 `self` 传入。 <!-- v:ms:ms_m2_method_binding -->
- [x] `class B extends A {}; B().f()` → 从 `A` 继承的 `f` 被调用。 <!-- v:ms:ms_m2_method_binding -->
- [x] MRO 顺序：`class B extends A {}; class C extends B {}` → `MRO=[C,B,A]`（不含 `object`，见 §0）。 <!-- v:ctest:test_method_binding -->
- [x] 子类覆盖：`class C extends B {}` 重写 `f` 后，`C().f()` 调用 `C` 自身的实现而非 `A`/`B` 的。 <!-- v:ms:ms_m2_method_binding -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/vm/test_method_binding.c`）

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

static void testBoundMethodCall(void) {
  MsValue v = run("class A { func f(self) { return 42 } }\nA().f()");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 42, "A().f() == 42");
}

static void testInheritedMethod(void) {
  MsValue v = run("class A { func f(self) { return 1 } }\nclass B extends A {}\nB().f()");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 1, "B().f() inherits A.f");
}

static void testMroOverride(void) {
  MsValue v = run(
      "class A { func f(self) { return 1 } }\n"
      "class B extends A { func f(self) { return 2 } }\n"
      "class C extends B {}\n"
      "C().f()");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 2, "C().f() resolves to B.f via MRO");
}

int main(void) {
  MS_RUN(testBoundMethodCall);
  MS_RUN(testInheritedMethod);
  MS_RUN(testMroOverride);
  return msTestSummary();
}
```

### `.ms` 端到端测试（`tests/ms/m2/method_binding.ms` + `method_binding.expected`）

```ms
// method binding: bound method self injection / MRO inheritance lookup (T073)

class Animal {
    func speak(self) {
        return "..."
    }
}


class Dog extends Animal {
    func speak(self) {
        return "Woof!"
    }
}


class Cat extends Animal {
    func speak(self) {
        return "Meow!"
    }
}


dog := Dog()
cat := Cat()
print(dog.speak())   // Woof!
print(cat.speak())   // Meow!


class A {
    func hello(self) {
        return "from A"
    }
}


class B extends A {}


class C extends B {}


print(C().hello())   // from A，MRO: C -> B -> A
```

---

## Benchmark

N/A（方法绑定成本在整体 class bench 中体现，参见 `P5-T072-class-instantiation.md` 的 `benchmarks/bench_class.ms`）。

---

## 风险与边界

- **MRO 计算时机**：在 `OP_MAKE_CLASS` 执行时（类定义时，`baseClass` 赋值之后）调用 `msBuildMRO`，结果缓存在 `tp->mstype.mro`。单继承下为线性父类链，无 C3 算法；`type-system.md §3.1` 为未来多继承保留了 C3 扩展空间，但 `OP_MAKE_CLASS` 的 `baseCount` 恒为 0/1（`ms_vm.c:508`），本任务不实现 C3。
- **不引入 `object` 根类**：MRO 链止于 `baseClass == NULL`（见 §0）；默认魔术方法（`__repr__`/`__str__`/`__eq__` 等）由 T074 通过 `msTypeLookupMethodMRO` 未命中时回退到内置实现提供，不依赖全局根类对象。
- **`msFindInit` 保持不变**：T072 的 `msFindInit` 沿 `baseClass` 单向链查 `__init__`，与本任务 `msTypeLookupMethodMRO` 沿 `mro` 列表查找在单继承下顺序完全等价；为避免不必要改动 T072 已验证的代码，本任务不将其切换为 MRO 版本。
- **`OP_CALL_KW` 不支持绑定方法**：`src/vm/ms_vm.c` 的 `OP_CALL_KW`（kwargs 调用）不经过 `dispatchCall`，硬编码只接受 `msClosureType` 的 callee；`obj.method(**kwargs)` 会被误判为 TypeError（“not callable”）。本任务不修复此路径，留待后续任务（kwargs + 绑定方法的组合调用）处理。
- **绑定方法的调用形态受限**：`msNewBoundMethod` 包装的 `func` 目前只能是 `MsClosure`（`instanceGetAttr`/`msTypeLookupMethodMRO` 命中的方法值恒为 `OP_MAKE_CLASS` 用 `msNewClosure` 包装的闭包）；`dispatchBoundMethodCall` 对非闭包值报 TypeError，为未来 `__call__`（T077）可调用对象预留但当前不可达。
- **类对象自身访问方法（`A.f`，非实例）未定义**：`msMetaType` 没有 `tpGetattr` 槽，`A.f` 会走 `OP_GET_ATTR` 的通用回退路径（`msTypeLookupMethod`，检查 `&msMetaType` 自身的 `methods`，恒为 `NULL`），因此当前恒为 AttributeError；本任务不改变这一行为，留待需要时再设计。
- **内置类型方法访问范围**：`testing-ms.md §6` 记载 `obj.method()`（str/list/map 等内置方法）由本任务"解锁"，但本任务的 `instanceGetAttr`/`msTypeLookupMethodMRO` 只服务于用户定义类（`msMetaType` 体系）；内置类型的 `methods` 字典填充与绑定不在本任务范围内，由各内置类型自身的 `tpGetattr` 继续处理（见 P4-T066）。
- **继承深度**：`msBuildMRO` 两趟遍历、按实际长度分配 `MsListObj`，没有硬编码上限，但极端深的继承链仍受限于该类型对象本身在堆上可分配的大小（实践中不构成问题）。
