# P5-T075 super() 代理

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `super()` 与 `MsSuperObj` 代理对象：在方法体内调用 `super()` 返回一个代理对象，属性/方法查找从 MRO 中"定义当前方法的那个类"的下一个类开始（跳过当前类），支持 `super().__init__(args)` 等用法。`super` 不是保留字（与 `self` 同为编译器识别的软关键字，见 syntax.md §1.4 附注），编译器仅在 `super()` 调用形态下特判。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T073 | MRO 查找（`msTypeLookupMethodMRO`/`msBuildMRO`/`MsBoundMethodObj`/`msNewBoundMethod`，已落地于 `src/runtime/ms_class.c`） |
| P5-T072 | 实例/类对象（`MsTypeObj`/`MsInstanceObj`/`msMetaType`/`instanceGetAttr`） |
| P3-T044 | 编译层 `OP_MAKE_CLASS`/`compileClassDecl`/`compileFuncProto`（方法体的编译入口） |
| P5-T068 | 调用约定（`msClosureCall`/`MsFrame`，`self` 固定为方法第一个局部槽） |
| P5-T071 | 闭包对象（`MsClosure`/`msClosureType`，`definingClass` 字段挂在其上） |
| P4-T059 | `MsListObj`（MRO 列表的载体） |
| P4-T060 | `MsMapObj`（方法字典 `MsType.methods`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §3.5 `super()` 与父类调用、§3.1 单继承、§3.3 MRO 与方法查找、§1.3 `MsType` |
| `vm.md` | §3.9 `LOAD_SUPER`（闭包与类）、§9 opcode 命名映射、§4 调用帧 `MsFrame` |
| `gc.md` | §8 精确根枚举（`MsSuperObj`/`MsClosure.definingClass` 的 traverse 要求） |
| `errors.md` | `AttributeError`（未找到）/`RuntimeError`（方法外使用，T080 占位） |
| `syntax.md` | §1.4 关键字附注（`self` 非保留字的软关键字先例）、§2 文法 `ClassDecl`（单继承） |

---

## 待实现（C 文件）

### 修改文件

```
include/mslang/ms_class.h  # 新增 MsSuperObj/msSuperType/msNewSuper 声明
include/mslang/ms_func.h   # MsClosure 新增 definingClass 字段
src/runtime/ms_class.c     # 新增 superGetAttr/msSuperType/msNewSuper；
                            # msTypeLookupMethodMRO 重构为共用 lookupMethodFromIndex
src/runtime/ms_func.c      # msNewClosure 初始化 definingClass = MS_NIL_VAL；
                            # closureTraverse 增加对 definingClass 的 visit
src/vm/ms_vm.c              # OP_MAKE_CLASS 循环内回填每个方法闭包的 definingClass；
                            # 新增 case OP_LOAD_SUPER
src/compiler/ms_scope.h     # MsCompiler 新增 bool isMethod 字段
src/compiler/ms_compiler.c  # compileFuncProto 设置 funcC.isMethod = true；
                            # compileCall 开头特判 callee 为标识符 "super" 的调用
tests/vm/test_super.c            # C 单测
tests/ms/m2/super.ms             # .ms 端到端测试
tests/ms/m2/super.expected
tests/CMakeLists.txt        # 注册 test_super 与 ms_add_ms_test(super ...)
```

---

## 实现要点

### 0. 设计决策："当前类"通过 `MsClosure.definingClass` 传递（不新增帧字段/不用 upvalue）

`vm.md §4` 的 `MsFrame` 无 `cls` 字段，`MsClosure`（P5-T071）也无指回定义类的字段，且方法闭包由 `OP_MAKE_CLASS` 用 `msNewClosure(proto, 0)` 创建（upvalue 数恒为 0），不经过 `OP_MAKE_FUNC` 的 upvalue 描述符路径，因此"编译器插入 `__class__` 隐式 upvalue"与"帧 `cls` 字段"两条路线都需要改动本任务之外的、当前不支持的机制。

本任务采用最小改动方案：给 `struct MsClosure`（`include/mslang/ms_func.h`）新增一个 `MsValue definingClass` 字段，由 `OP_MAKE_CLASS` 在为每个方法调用 `msNewClosure` 之后立即回填为 `MS_OBJ_VAL(tp)`（`tp` 是正在构建的类对象）；非方法闭包（`OP_MAKE_FUNC` 路径，`msNewClosure` 的另一处调用方）保持 `MS_NIL_VAL`。`OP_LOAD_SUPER` 执行时从 `((MsClosure*) frame->closure)->definingClass` 读取"当前类"，从 `frame->slots[0]` 读取 `self`（调用约定 P5-T068 保证方法的第一个局部槽恒为 `self`）。

`P3-T044-compiler-class.md §风险` 写的"编译层只需正确设置调用帧的 `cls` 字段"这一表述与上述机制不符（`MsFrame` 从未获得过 `cls` 字段），本任务将其作废，改由 `definingClass` 承担同样的职责；不修改 `P3-T044` 文档本身（已完成任务，改动范围超出本次审核对象）。

### 1. `MsSuperObj`

```c
// 文件: include/mslang/ms_class.h
struct MsSuperObj {
  struct MsObject head;  // head.type == &msSuperType
  MsValue startType;     // MsTypeObj*（MS_OBJ_VAL 包装）：MRO 扫描从其下一个类开始
  MsValue instance;      // 绑定的实例（self）
};

extern struct MsType msSuperType;

// startType/instance 已经分别经由 MsClosure.definingClass（GC 根链：frame->closure）
// 和 frame->slots[0]（值栈，GC 根）可达，调用前无需额外 msGCPushRoot。
MsValue msNewSuper(MsValue startType, MsValue instance);
```

`startType`/`instance` 均存为 `MsValue`（而非裸 `struct MsTypeObj*`），与 `MsBoundMethodObj.func`/`self`（P5-T073）、`MsListIterObj.list`（P4-T065）同一约定：traverse 直接 visit 值栈式的 `MsValue` 槽位，为 `gc.md §6/§9` 未来的移动式 GC（半区复制）预留正确的写回路径。

### 2. `super()` 语义与编译期识别

仅支持零参数 `super()`（`type-system.md §3.5` 的唯一定义形态；`LOAD_SUPER` 无操作数，`vm.md §3.9` 明确操作数列为 `—`，无法承载显式 `Type`/`inst` 实参，Python 风格的 `super(Type, inst)` 本任务不实现）。

`super` 不加入 `syntax.md §1.4` 的关键字表（与 `self` 同为"软关键字"，见该节附注），词法/解析阶段仍将其识别为普通标识符；编译器在 `compileCall`（`src/compiler/ms_compiler.c`）开头特判：

```c
// 文件: src/compiler/ms_compiler.c（compileCall 函数体最前）
if (n->call.callee->kind == MS_ND_IDENT && identLen(n->call.callee->ident.name) == 5 &&
    memcmp(n->call.callee->ident.name, "super", 5) == 0) {
  if (!c->isMethod) {
    compilerError(c, n->pos, "super() outside of method");
    return;
  }
  if (n->call.args != NULL || n->call.kwargs != NULL) {
    compilerError(c, n->pos, "super() takes no arguments");
    return;
  }
  msChunkEmitOp(c->chunk, OP_LOAD_SUPER, n->pos.line);
  return;  // super() 本身不产生 OP_CALL：LOAD_SUPER 直接压入最终代理对象
}
```

`c->isMethod`（`src/compiler/ms_scope.h` 新增字段）由 `compileFuncProto`（`compileClassDecl` 的唯一方法编译入口，区别于普通函数走的 `compileFuncToConst`）在 `msCompilerInit` 之后设置：`funcC.isMethod = true;`。裸标识符 `super`（不带调用，如 `x := super`）不特殊处理，按普通标识符走 `resolveVar` 解析为"未定义变量"编译错误，效果上等同不可用；显式支持一等 `super` 值不在本任务范围。

"当前类"取的是**定义当前正在执行的方法的那个类**（`MsClosure.definingClass`，见 §0），不是 `type(self)`：否则在 `C extends B extends A` 中，若 `B.hello` 内 `super()` 的起点被误取为 `type(self) == C`，会重新解析回 `B.hello` 自身造成无限递归。MRO 列表仍取 `type(self)` 的 `mro`（即 `msTypeOf(su->instance)->mro`），只是扫描起点由 `definingClass` 决定。

### 3. `OP_LOAD_SUPER`（VM 运行期）

```c
// 文件: src/vm/ms_vm.c（eval() 的 opcode switch 内，紧邻 OP_MAKE_CLASS 之后）
case OP_LOAD_SUPER: {
  MsClosure* cl = (MsClosure*) frame->closure;
  if (MS_IS_NIL(cl->definingClass)) {
    return MS_ERROR_VALUE;  // RuntimeError: super() outside of method (T080 placeholder;
                             // 编译期 isMethod 检查已拦截绝大多数情形，此处为纵深防御)
  }
  PUSH(msNewSuper(cl->definingClass, frame->slots[0]));
  DISPATCH();
}
```

`OP_MAKE_CLASS`（`src/vm/ms_vm.c:706-713` 的方法构建循环）在每次 `msNewClosure` 之后回填 `definingClass`：

```c
      MsValue mClosure = msNewClosure((MsFuncProto*) MS_AS_OBJ(frame->chunk->constants[mFuncIdx]), 0);
      ((MsClosure*) MS_AS_OBJ(mClosure))->definingClass = MS_OBJ_VAL(tp);  // T075
      msMapSet(&gVM, methodsMap, frame->chunk->constants[mNameIdx], mClosure);
```

`src/runtime/ms_func.c` 的 `msNewClosure` 需将新字段初始化为 `MS_NIL_VAL`（非方法闭包，`OP_MAKE_FUNC` 路径，保持"不在方法内"语义）；`closureTraverse` 追加：

```c
  if (!MS_IS_NIL(cl->definingClass)) {
    visit(&cl->definingClass, ctx);
  }
```

### 4. `msSuperType.tpGetattr`（`superGetAttr`）

复用 `msTypeLookupMethodMRO`（P5-T073）的扫描逻辑，抽出带起始索引的共用尾部，避免重复实现 MRO 遍历：

```c
// 文件: src/runtime/ms_class.c（替换现有 msTypeLookupMethodMRO 实现）
// 沿 mroObj（MsListObj*）从下标 startIdx（含）开始依次查各类自身的 methods
// 字典；miss 约定同旧版：返回 MS_ERROR_VALUE（不用 MS_NIL_VAL，nil 是合法
// 属性值）。msTypeLookupMethodMRO（startIdx=0）与 superGetAttr（startIdx=
// startType 之后一位）共用本函数。
static MsValue lookupMethodFromIndex(struct MsVM* vm, struct MsObject* mroObj, uint32_t startIdx, MsValue name) {
  if (!mroObj) {
    return MS_ERROR_VALUE;
  }
  struct MsListObj* mro = (struct MsListObj*) mroObj;
  for (uint32_t i = startIdx; i < mro->len; i++) {
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

MsValue msTypeLookupMethodMRO(struct MsVM* vm, struct MsType* tp, MsValue name) {
  return lookupMethodFromIndex(vm, tp->mro, 0, name);
}

// tpGetattr for msSuperType (T075): scans su->instance's MRO starting right
// after su->startType. startType not found in the MRO (cross-class misuse of
// super) or startType being the MRO's last element both naturally fall out
// as "not found" -- no separate guard needed.
static MsValue superGetAttr(struct MsVM* vm, MsValue v, MsValue name) {
  struct MsSuperObj* su = (struct MsSuperObj*) MS_AS_OBJ(v);
  struct MsType* instType = MS_AS_OBJ(su->instance)->type;
  if (!instType->mro) {
    return MS_ERROR_VALUE;
  }
  struct MsListObj* mro = (struct MsListObj*) instType->mro;
  uint32_t startIdx = mro->len;  // default: startType not found => scan nothing
  for (uint32_t i = 0; i < mro->len; i++) {
    if (MS_AS_OBJ(mro->items[i]) == MS_AS_OBJ(su->startType)) {
      startIdx = i + 1;
      break;
    }
  }
  MsValue m = lookupMethodFromIndex(vm, instType->mro, startIdx, name);
  if (MS_IS_ERROR(m)) {
    return MS_ERROR_VALUE;  // AttributeError (T080 placeholder)
  }
  // m 通过 cur->mstype.methods 可达，cur 通过局部遍历变量 mro->items[i] 可达但
  // 未落到值栈；su->instance 同理。msNewBoundMethod 内部 msGCAlloc 可能触发
  // GC，故显式保护（与 instanceGetAttr，P5-T073 §4，同一范式）。
  msGCPushRoot(su->instance);
  msGCPushRoot(m);
  MsValue bound = msNewBoundMethod(m, su->instance);
  msGCPopRoot();  // m
  msGCPopRoot();  // instance
  return bound;
}

static void superTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsSuperObj* su = (struct MsSuperObj*) obj;
  visit(&su->startType, ctx);
  visit(&su->instance, ctx);
}

struct MsType msSuperType = {
    .name = "super",
    .objSize = sizeof(struct MsSuperObj),
    .traverse = superTraverse,
    .tpGetattr = superGetAttr,
};

MsValue msNewSuper(MsValue startType, MsValue instance) {
  struct MsSuperObj* su = (struct MsSuperObj*) msGCAlloc(&msSuperType, sizeof(struct MsSuperObj));
  su->startType = startType;
  su->instance = instance;
  return MS_OBJ_VAL(su);
}
```

`superGetAttr` 命中方法后统一包一层 `MsBoundMethodObj`（与 `instanceGetAttr` 一致），调用侧无需新增 `dispatchCall` 分支：`super().method(...)` 直接复用 P5-T073 已落地的 `msBoundMethodType` 分派路径（`dispatchBoundMethodCall`）。

命中值若非方法（`type-system.md §3.6` 的类属性也存在 `methods` 字典里）目前仍会被当作方法包装成 `MsBoundMethodObj`；本任务不做区分，行为与"通过 `super()` 访问类属性"这一边缘用法一并留待后续任务处理（见「风险与边界」）。

### 5. `msFindInit` 与 `super().__init__()` 的关系（不改动 `msFindInit`）

`msFindInit`（P5-T072）沿 `baseClass` 单向链查 `__init__`，只服务 `OP_CALL` 的类实例化路径（`ClassName(...)`）；`super().__init__()` 走的是本任务的 `superGetAttr` → `MsBoundMethodObj` → `dispatchBoundMethodCall` 路径，两者互不依赖、互不影响，`msFindInit` 保持不变。

---

## 验收标准（checklist）

<!-- v:... 标签供 verify_task.py 自动勾选，见 _template.md -->
- [ ] 编译通过，无警告（`cmake --build build`）。 <!-- v:build -->
- [ ] C 单测通过。 <!-- v:ctest:test_super -->
- [ ] `.ms` 端到端测试输出与期望一致。 <!-- v:ms:ms_m2_super -->
- [ ] `class B extends A { func __init__(self) { super().__init__() } }` → 正确调用 `A.__init__`。 <!-- v:ms:ms_m2_super -->
- [ ] 三层单继承链 `class C extends B extends A`，每级 `hello()` 内调用 `super().hello()`，`C().hello()` 依次经 `C→B→A` 而不递归回自身。 <!-- v:ctest:test_super -->
- [ ] `super()` 出现在方法体外部 → 编译错误。 <!-- v:ctest:test_super -->
- [ ] `startType` 已是 MRO 末端（无更父类可查的方法）→ `AttributeError`（`MS_ERROR_VALUE`）。 <!-- v:ctest:test_super -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/vm/test_super.c`）

```c
// test_super.c
// T075: super() proxy -- MRO-based parent-class method dispatch,
// compile-time "outside of method" rejection, and AttributeError at the
// top of the MRO chain.
//
// Uses test_method_binding.c's runGlobal()/run() split (T073/T074): a bare
// top-level expression's value is unreachable directly (compiler appends
// OP_RETURN_NIL + OP_POP per statement), so happy-path assertions bind to a
// global via runGlobal(); error-path assertions rely on MS_ERROR_VALUE
// propagating straight out of eval() (no OP_POP reached), so run() alone
// suffices there.
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

static MsValue runGlobal(const char* src, const char* name) {
  msVMInit();
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue runResult = msVMRun(r.chunk);
  MS_ASSERT_TRUE(!MS_IS_ERROR(runResult), "runGlobal: program must run without error");
  MsValue key = msNewStr(name, (uint32_t) strlen(name));
  MsValue v = msMapGet(&gVM, gVM.mainThread.globals, key);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static MsValue run(const char* src) {
  msVMInit();
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue v = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static void testSuperMroChain(void) {
  MsValue v = runGlobal(
      "class A { func hello(self) { return \"A\" } }\n"
      "class B extends A { func hello(self) { return \"B,\" + super().hello() } }\n"
      "class C extends B { func hello(self) { return \"C,\" + super().hello() } }\n"
      "r := C().hello()",
      "r");
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "C().hello() returns a str");
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 5 && memcmp(s->data, "C,B,A", 5) == 0, "C().hello() == \"C,B,A\" via 3-level super chain");
}

static void testSuperInitCallsParent(void) {
  MsValue v = runGlobal(
      "class Animal { func __init__(self, name) { self.name = name } }\n"
      "class Dog extends Animal { func __init__(self, name) { super().__init__(name) } }\n"
      "r := Dog(\"Rex\").name",
      "r");
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 3 && memcmp(s->data, "Rex", 3) == 0, "super().__init__() sets self.name via Animal.__init__");
}

static void testSuperOutsideMethodIsCompileError(void) {
  MsCompileResult r = msCompile("super()", 7, "<t>");
  MS_ASSERT_TRUE(r.hadError, "super() outside of any method is a compile error");
  msCompileResultFree(&r);
}

static void testSuperAtMroTopIsAttributeError(void) {
  MsValue v = run(
      "class A { func hello(self) { return super().hello() } }\n"
      "A().hello()");
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "super().hello() at the top of the MRO (no further parent) is an AttributeError");
}

int main(void) {
  MS_RUN(testSuperMroChain);
  MS_RUN(testSuperInitCallsParent);
  MS_RUN(testSuperOutsideMethodIsCompileError);
  MS_RUN(testSuperAtMroTopIsAttributeError);
  return msTestSummary();
}
```

### `.ms` 端到端测试（`tests/ms/m2/super.ms` + `super.expected`）

```ms
// super(): MRO-based parent-class method dispatch (T075)

class A {
    func hello(self) {
        return "A"
    }
}


class B extends A {
    func hello(self) {
        return "B," + super().hello()
    }
}


class C extends B {
    func hello(self) {
        return "C," + super().hello()
    }
}


print(C().hello())   // C,B,A


class Animal {
    func __init__(self, name) {
        self.name = name
    }

    func speak(self) {
        return "..."
    }
}


class Dog extends Animal {
    func __init__(self, name, breed) {
        super().__init__(name)
        self.breed = breed
    }

    func speak(self) {
        return $"Woof! I'm {self.name}"
    }
}


dog := Dog("Rex", "Labrador")
print(dog.name)     // Rex
print(dog.breed)    // Labrador
print(dog.speak())  // Woof! I'm Rex
```

`super.expected`：

```
C,B,A
Rex
Labrador
Woof! I'm Rex
```

---

## .ms 使用示例

```ms
class Animal {
    func __init__(self, name) {
        self.name = name
    }
}


class Dog extends Animal {
    func __init__(self, name, breed) {
        super().__init__(name)   // 调用 Animal.__init__
        self.breed = breed
    }
}
```

---

## Benchmark

N/A（`super()` 的代理分配 + MRO 扫描成本与普通方法调用同量级，已在 `P5-T073-method-binding-mro.md` 的方法绑定 bench 中间接体现，不单独设基准）。

---

## 风险与边界

- **不支持双参数 `super(Type, inst)`**：`type-system.md §3.5` 与 `LOAD_SUPER`（无操作数）均只定义零参数形态；Python 风格的显式双参数留待后续任务在文法与 opcode 层扩展后处理。
- **不支持多继承 / C3 线性化**：`syntax.md §2` 的 `ClassDecl` 文法 `extends` 后仅允许单个 identifier；`OP_MAKE_CLASS` 的 `baseCount` 恒为 0/1（`P5-T073 §风险` 已落地结论）。`super()` 的 MRO 扫描在单继承下退化为线性父类链。
- **`super().method(**kwargs)` 不支持**：与 P5-T073 相同的既有限制——`OP_CALL_KW` 不经过 `dispatchCall`，硬编码只接受 `msClosureType` 的 callee，绑定方法（含 `super()` 返回的绑定方法）与 kwargs 调用的组合当前不可用。
- **裸 `super`（不带调用）不特殊处理**：仅 `super()` 调用形态被编译器识别；`x := super` 退化为"未定义变量"编译错误。将 `super` 支持为可传递的一等代理对象值需要额外设计，不在本任务范围。
- **`super()` 方法外调用为编译期错误，而非运行期 `RuntimeError`**：采用"编译器特判标识符"路线后，方法体外的 `super()` 在解析出的 AST 上下文里即可判定，编译期直接报错更符合"尽早失败"原则；VM 侧的 `MS_IS_NIL(cl->definingClass)` 检查作为纵深防御保留，理论上编译期检查生效后不可达。
- **通过 `super()` 访问类属性（非方法）**：`type-system.md §3.6` 类属性与方法同存于 `MsType.methods`，`superGetAttr` 命中非方法值时仍会包装成 `MsBoundMethodObj`（错误行为）；本任务不处理这一边缘情形，留待类属性/实例属性遮蔽规则任务（P5-T076）一并设计。
- **类对象自身访问 `super`（如在 `classmethod` 语境下）未定义**：本任务的 `super()` 只在实例方法体内有意义（`frame->slots[0]` 必须是实例），当前语言未设计类方法（`classmethod`）机制，不涉及。
- **`P3-T044-compiler-class.md §风险` 的 "`cls` 字段" 表述作废**：该任务已完成、不属于本次审核对象，不直接修改其文档；本任务改用 `MsClosure.definingClass`（见 §0），效果与该表述描述的意图一致，仅传递机制不同。
