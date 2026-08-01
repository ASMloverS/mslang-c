# P5-T074 魔术方法分派（__add__ / __len__ / __iter__ 等）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现用户定义类的魔术方法（dunder methods）分派：当 VM 执行算术/比较/容器操作时，优先查找实例所属类的对应魔术方法并调用。将 Python 风格的运算符重载接入 mslang 类系统。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T072 | 实例对象定义（`MsInstanceObj`/`msMetaType`/`instanceGetAttr`，已落地于 `src/runtime/ms_class.c`） |
| P5-T073 | MRO 查找与方法绑定（`msTypeLookupMethodMRO`/`msBuildMRO`/`MsBoundMethodObj`/`dispatchBoundMethodCall`，已落地） |
| P5-T068 | 调用约定（`msClosureCall`/`MsFrame`/frame 池，魔术方法调用复用此机制） |
| P5-T066 | `OP_GET_ATTR`/`OP_SET_ATTR`/`OP_GET_ITEM`/`OP_SET_ITEM`/`OP_DEL_ITEM` 分派与 `MsType` 类型槽约定 |
| P4-T065 | 迭代协议（`tpIter`/`tpNext`、`OP_GET_ITER`/`OP_FOR_ITER`、nil = StopIteration 哨兵） |
| P4-T049 | `msValueTruthy`/`msValueEqual`（真值测试与相等性的既有实现） |
| P4-T060 | map（方法字典，`MsType.methods`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §3.4 魔术方法（特殊方法）表 |
| `type-system.md` | §3.3 MRO 与方法查找、§4 迭代器协议 |
| `vm.md` | 算术/比较/下标/迭代指令分派章节 |
| `gc.md` | §8 精确根枚举 |
| `impl/P5-T072-class-instantiation.md` | 实例对象结构、`msMetaType`、GC 根保护范式 |
| `impl/P5-T073-method-binding-mro.md` | `msTypeLookupMethodMRO` 签名与 miss 约定、压帧调用范式 |

---

## 待实现（C 文件）

### 修改文件

```
src/vm/ms_vm.c             # 新增 dispatchMagicBinary/dispatchMagicUnary 辅助函数；
                           # OP_ADD/SUB/MUL/DIV/MOD/POW/BITOR/BITAND/BITXOR/NEG/POS/INVERT/
                           # EQ/NE/LT/LE/GT/GE/IN/GET_ITEM/SET_ITEM/DEL_ITEM/GET_ITER/FOR_ITER
                           # 接入 MRO 探测分派；OP_RETURN/OP_RETURN_NIL 新增 discardReturn 与
                           # for-iter continuation 特判
include/mslang/ms_vm.h     # MsFrame 新增 discardReturn/forIterPending/forIterOffset 字段
src/runtime/ms_class.c     # msVMInit 处新增全部 dunder 名字符串常量的一次性 intern（同 gInitNameVal 惯例）
include/mslang/ms_class.h  # 上述 dunder 名常量的声明；新增 msIsInstance 内联判定函数
tests/vm/test_magic_methods.c   # C 单测
tests/ms/m2/magic_methods.ms    # .ms 端到端测试
tests/ms/m2/magic_methods.expected
tests/CMakeLists.txt       # 注册 test_magic_methods 与 ms_add_ms_test(magic_methods ...)
```

---

## 实现要点

### 0. 设计决策：不引入可重入调用；魔术方法在 opcode 层面按需分派，不写入 `MsType` 槽

**问题背景**：`MsBinaryFn`/`MsUnaryFn`（`ms_object.h`）要求同步返回 `MsValue`；仓库中不存在、也不引入可从 C 重入 eval 循环的 `msCallFn`（`impl/P5-T072 §6` 已明确「VM 没有可从 C 重入 eval 循环的 `msCallFn`」）。因此**魔术方法不会被安装为 `tp->mstype.tpAdd` 等槽的函数指针**——那样的安装本身就需要一个能同步调用脚本闭包的 C 包装函数，等价于需要可重入调用，问题并未解决，只是换了个位置。

**采纳方案**：在每个涉及运算符/下标/迭代的 opcode case 内，**先看原生槽，槽为空且操作数是用户实例时，走 MRO 探测 + 压帧分派**（复用 T072 `dispatchClassCall`、T073 `dispatchBoundMethodCall` 已确立的范式：把 self/实参重新压回 VM 值栈，调用 `msClosureCall` 取得新 `MsFrame*`，交给求值循环的 `DISPATCH()` 切帧执行；方法体跑完 `OP_RETURN` 后自然切回原帧的下一条指令，栈顶就是结果）。用户类的 `tp->mstype.tpAdd` 等槽**永远保持 `NULL`**，不做任何"槽装配"。

判定"一个 `MsValue` 是否为用户类实例"**不能**用 `offsetof` 从 `MS_AS_OBJ(v)->type` 反推出 `MsTypeObj*` 再判它——内置类型的 `head.type` 指向的是**静态**（非堆分配）`struct MsType`，对这类指针做 `offsetof` 回退会算出野指针，解引用属未定义行为。安全做法是复用「`tpAdd` 等槽的函数指针值本身就是类型身份标记」这一既有惯例（`dispatchCall` 用 `MS_AS_OBJ(callee)->type == &msNativeFnType` identity 判定同理）：`OP_MAKE_CLASS` 给每个用户类的 `mstype.traverse` 都赋值为同一个 `instanceTraverse` 函数指针（`ms_class.h` 已声明），内置类型各自使用互不相同的专属 traverse，因此比较 `traverse` 指针身份即可安全判定：

```c
// 文件: include/mslang/ms_class.h
static inline bool msIsInstance(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type->traverse == instanceTraverse;
}
```

**覆盖范围**（凡是"计算一个值→压栈→继续执行下一条指令"这一形状的 opcode，天然与 `OP_RETURN` 的既有语义吻合，本任务实现）：
算术 `__add__/__sub__/__mul__/__div__/__mod__/__pow__`、位运算 `__or__/__and__/__xor__`、一元 `__neg__/__pos__/__invert__`、比较 `__eq__/__ne__/__lt__/__le__/__gt__/__ge__`、成员 `__contains__`、下标 `__getitem__/__setitem__/__delitem__`、迭代 `__iter__/__next__`。

**明确排除范围**（这些路径要求在同一次 C 函数调用内**同步**拿到结果——`if x`/`x and y`/`not x` 的短路判定、`len(x)` 内置函数、map/set 的键哈希与相等性判定、`print`/`str`/`repr` 的字符串化——本任务不实现，需要真正的可重入调用机制或更通用的"延续（continuation）"机制才能做，留给后续任务）：
`__bool__`、`__len__`（含真值测试与 `len()` 内置两种用途）、`__not__`、`__hash__`、`__str__`、`__repr__`。`__call__` 属 T077 范围（`tp->mstype.call` 槽保留给它，T072 §0 已声明此约定）；位移 `__lshift__`/`__rshift__` 目前 `MsType` 无对应槽（`SHIFT_OP` 宏是 int-only），本任务不新增槽，不实现。

**反向运算符规则**（mslang 无 `NotImplemented` 单例，与 Python 不同）：`dispatchMagicBinary`（§3）只负责 MRO 层面的存在性判定——先看 `a` 的 MRO 是否命中正向 dunder（如 `__add__`），若无、且 `b` 与 `a` 类型不同，再看 `b` 的 MRO 是否命中反向 dunder（如 `__radd__`）；一旦 `a` 命中正向 dunder，无论调用其结果如何（包括抛异常）都不会再改试反向——脚本定义的 dunder 没有"声明自己不支持某操作数类型"的信号（无 `NotImplemented`），因此无从判断"拒绝"与"正常报错"的区别。**但**内置类型的原生槽（`tpAdd` 等）沿用既有约定：返回 `MS_ERROR_VALUE` 表示"此原生实现不支持该操作数类型"，这本身就是一种"声明拒绝"的信号，因此 §4 的 `OP_ADD` 等 opcode 在原生槽存在但返回错误时，会继续尝试 `dispatchMagicBinary`（让另一侧的反向 dunder，如内置 `int` 遇到 `Vec` 时的 `Vec.__radd__`，有机会接管）——这一步的"拒绝再重试"只发生在原生槽这一层，不发生在脚本 dunder 这一层。

### 1. 魔术方法与类型槽映射表

对齐 `include/mslang/ms_object.h` 的实际字段名与 `type-system.md §3.4`：

| 操作 / 槽 | 魔术方法 | 反向方法 | 本任务覆盖 |
|---|---|---|---|
| `tpAdd` | `__add__` | `__radd__` | ✅ |
| `tpSub` | `__sub__` | `__rsub__` | ✅ |
| `tpMul` | `__mul__` | `__rmul__` | ✅ |
| `tpDiv` | `__div__` | `__rdiv__` | ✅ |
| `tpMod` | `__mod__` | `__rmod__` | ✅ |
| `tpPow` | `__pow__` | `__rpow__` | ✅ |
| `tpBitor`/`tpBitand`/`tpBitxor` | `__or__`/`__and__`/`__xor__` | — | ✅ |
| `tpNeg` | `__neg__` | — | ✅ |
| `tpPos` | `__pos__` | — | ✅ |
| `tpInvert` | `__invert__` | — | ✅ |
| `tpEq` | `__eq__` | — | ✅ |
| `tpNe` | `__ne__` | — | ✅ |
| `tpLt`/`tpLe`/`tpGt`/`tpGe` | `__lt__`/`__le__`/`__gt__`/`__ge__` | — | ✅ |
| `tpContains` | `__contains__` | — | ✅ |
| `tpGetitem`/`tpSetitem`/`tpDelitem` | `__getitem__`/`__setitem__`/`__delitem__` | — | ✅ |
| `tpIter`/`tpNext` | `__iter__`/`__next__` | — | ✅ |
| `tpBool` | `__bool__` | — | ❌ 见 §0 排除范围 |
| `tpLen` | `__len__` | — | ❌ 见 §0 排除范围 |
| `tpNot` | `__not__` | — | ❌ 见 §0 排除范围 |
| `tpHash` | `__hash__` | — | ❌ 见 §0 排除范围 |
| `tpStr`/`tpRepr` | `__str__`/`__repr__` | — | ❌ 见 §0 排除范围 |
| `call`（非 `tpCall`） | `__call__` | — | ❌ 属 T077 |
| （无对应槽） | `__lshift__`/`__rshift__` | — | ❌ 见 §0 排除范围 |

### 2. `MsFrame` 新增字段（`include/mslang/ms_vm.h`）

```c
// 文件: include/mslang/ms_vm.h（MsFrame 结构体内，紧邻既有 isCtor/boundCall）
bool discardReturn;      // T074: true 时 OP_RETURN/OP_RETURN_NIL 不压回结果
                          // （del 语句形态的魔术方法调用，如 __delitem__）
bool forIterPending;     // T074: true 时下一次 OP_RETURN/OP_RETURN_NIL 按
                          // OP_FOR_ITER 语义处理返回值（nil 则跳转，否则压栈）
int32_t forIterOffset;   // forIterPending 时使用的跳转偏移量
```

`msNewFrame()`/`msClosureCall()` 须显式将新分配或复用帧的这三个字段重置为 `false`/`0`（同 `isCtor`/`boundCall` 已有的重置惯例），避免 frame 池复用时残留状态污染普通调用。

### 3. 共享魔术方法分派辅助函数

```c
// 文件: src/vm/ms_vm.c
// 二元魔术方法分派：先试 a 的 MRO 是否有 forwardName；找不到且 a/b 类型不同
// 时试 b 的 MRO 是否有 reverseName（reverseName 可传 MS_NIL_VAL 表示无反向
// 方法，如比较运算符）。复用 T073 dispatchBoundMethodCall 确立的范式：新
// 压栈 self+arg（不是覆盖已有 callee 槽，因为 a/b 是 BINARY_OP 已弹出的裸
// 操作数，不是 [callee,args] 形态），frame->boundCall = true 使 OP_RETURN
// 按调用方 argc 收栈且不替换返回值。*ok=false 表示两侧都未定义，调用方按
// 既有 TypeError 回退处理。
static MsFrame* dispatchMagicBinary(struct MsThread* t, MsValue a, MsValue b, MsValue forwardName, MsValue reverseName, bool* ok) {
  struct MsType* ta = msTypeOf(a);
  MsValue m = msTypeLookupMethodMRO(&gVM, ta, forwardName);
  MsValue self = a, other = b;
  if (MS_IS_ERROR(m) && !MS_IS_NIL(reverseName) && msTypeOf(a) != msTypeOf(b)) {
    struct MsType* tb = msTypeOf(b);
    m = msTypeLookupMethodMRO(&gVM, tb, reverseName);
    self = b;
    other = a;
  }
  if (MS_IS_ERROR(m)) {
    *ok = false;
    return NULL;
  }
  if (!MS_IS_OBJ(m) || MS_AS_OBJ(m)->type != &msClosureType) {
    *ok = false;  // TypeError: dunder shadowed by non-callable value (T080 placeholder)
    return NULL;
  }
  PUSH(self);
  PUSH(other);
  struct MsClosure* cl = (struct MsClosure*) MS_AS_OBJ(m);
  MsFrame* newFrame = msClosureCall(t, cl, 2);
  if (!newFrame) {
    *ok = false;  // TypeError: dunder arity mismatch (T080 placeholder)
    return NULL;
  }
  newFrame->boundCall = true;
  *ok = true;
  return newFrame;
}

// 一元版本：只压 self。
static MsFrame* dispatchMagicUnary(struct MsThread* t, MsValue a, MsValue name, bool* ok) {
  struct MsType* ta = msTypeOf(a);
  MsValue m = msTypeLookupMethodMRO(&gVM, ta, name);
  if (MS_IS_ERROR(m)) {
    *ok = false;
    return NULL;
  }
  if (!MS_IS_OBJ(m) || MS_AS_OBJ(m)->type != &msClosureType) {
    *ok = false;  // TypeError (T080 placeholder)
    return NULL;
  }
  PUSH(a);
  struct MsClosure* cl = (struct MsClosure*) MS_AS_OBJ(m);
  MsFrame* newFrame = msClosureCall(t, cl, 1);
  if (!newFrame) {
    *ok = false;  // TypeError: arity mismatch (T080 placeholder)
    return NULL;
  }
  newFrame->boundCall = true;
  *ok = true;
  return newFrame;
}
```

`forwardName`/`reverseName`/其余 dunder 名字均为 `msVMInit()` 一次性 intern 的全局 `MsValue`（同 T072 `gInitNameVal` 惯例，见「待实现」中 `ms_class.h`/`ms_class.c` 的改动），不在每次分派时 `msNewStr`。

### 4. 算术 / 位运算 / 一元运算符接入（以 `OP_ADD`、`OP_NEG` 为例）

```c
// 文件: src/vm/ms_vm.c（取代原先直接调用 BINARY_OP(tpAdd) 的 case 体：先尝试
// 原生槽（若有），declined 或本就没有原生槽时都落到 dispatchMagicBinary，
// 由它对两侧分别做 MRO 存在性判定，见 §0 的反向运算符规则）
case OP_ADD: {
  MsValue b = PEEK(0), a = PEEK(1);
  struct MsType* ta = msTypeOf(a);
  if (ta->tpAdd) {
    // 先尝试原生槽（内置类型对不支持的操作数类型返回 MS_ERROR_VALUE，
    // 不是"没有 tpAdd"——不能只在 !ta->tpAdd 时才考虑分派，否则
    // `1 + Vec(2)` 这类"左操作数是内置类型、右操作数定义了 __radd__"的
    // 场景永远进不了下面的魔术方法回退，见风险与边界的 bug 记录）。
    MsValue r = ta->tpAdd(&gVM, a, b);
    if (!MS_IS_ERROR(r)) {
      POP();
      POP();
      PUSH(r);
      DISPATCH();
    }
  }
  POP();
  POP();
  bool ok;
  MsFrame* nf = dispatchMagicBinary(t, a, b, gDunderAdd, gDunderRadd, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  frame = nf;
  DISPATCH();
}
```

`OP_SUB`/`OP_MUL`/`OP_DIV`/`OP_MOD`/`OP_POW` 按同一模式接入（`tpSub`/`__sub__`/`__rsub__` 等）；`BITWISE_OP` 宏调用点同理（`tpBitor`/`__or__`，无反向方法，`reverseName` 传 `MS_NIL_VAL`）——注意 `BITWISE_OP` 的 int 快速路径（`MS_IS_INT(a) && MS_IS_INT(b)`）保持不变，只在其 `else` 分支（原调用 `ta->slot`处）套用上面"先试原生槽、失败再 `dispatchMagicBinary`"的模式。`dispatchMagicBinary` 对非用户实例的操作数会因 `mro`/`methods` 为 `NULL` 而安全地直接 miss（见 §3 `msTypeLookupMethodMRO` 的既有 `!tp->mro` 判空），因此不需要额外的 `msIsInstance` 前置判断——两侧都不是实例、原生槽也不支持时，`dispatchMagicBinary` 的 `*ok=false` 与既有 TypeError 行为完全一致。一元运算符没有反向方法，`msIsInstance` 前置判断仍然适用（更直接，无需"先试后退"）：

```c
// 文件: src/vm/ms_vm.c
case OP_NEG: {
  MsValue a = PEEK(0);
  struct MsType* ta = msTypeOf(a);
  if (!ta->tpNeg && msIsInstance(a)) {
    POP();
    bool ok;
    MsFrame* nf = dispatchMagicUnary(t, a, gDunderNeg, &ok);
    if (!ok) {
      return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
    }
    frame = nf;
    DISPATCH();
  }
  UNARY_OP(tpNeg);
  DISPATCH();
}
```

`OP_POS`/`OP_INVERT` 同理（`tpPos`/`__pos__`、`tpInvert`/`__invert__`）。

### 5. 比较 / 成员判定接入（以 `OP_EQ` 为例）

`OP_EQ`/`OP_NE` 目前调用 `msValueEqual(a, b)`（`bool` 返回、`vm` 传 `NULL`，`ms_value.c`）；该函数在 map/set 等 C 内部路径仍需要一个同步 `bool` 结果，**保留不动**。与算术运算符不同，相等性比较**没有"缺失方法即 TypeError"的合理默认**——用户类没定义 `__eq__` 时应静默退回既有的身份/结构相等判定（`msValueEqual`），而不是报错（例如 `Point(1, 2) == Point(1, 2)`，`Point` 未定义 `__eq__`，应得到 `false`——两个不同对象——而不是崩给用户一个 TypeError）。因此 `OP_EQ` 的分派失败（`dispatchMagicBinary` 的 `*ok=false`）不像 `OP_ADD` 那样返回错误，而是落回 `msValueEqual`：

```c
// 文件: src/vm/ms_vm.c
case OP_EQ: {
  MsValue b = PEEK(0), a = PEEK(1);
  if (msIsInstance(a)) {
    POP();
    POP();
    bool ok;
    MsFrame* nf = dispatchMagicBinary(t, a, b, gDunderEq, MS_NIL_VAL, &ok);
    if (ok) {
      frame = nf;
      DISPATCH();
    }
    // a 是实例但没有 __eq__：不是错误，退回既有默认相等判定。
    PUSH(MS_BOOL_VAL(msValueEqual(a, b)));
    DISPATCH();
  }
  POP();
  POP();
  PUSH(MS_BOOL_VAL(msValueEqual(a, b)));
  DISPATCH();
}
```

`OP_NE` 同理接入 `tpNe`/`__ne__`（同样的"分派失败就退回 `msValueEqual`"规则，取反）；无缺省"等价于 `not __eq__`"的自动回退——`type-system.md §3.4` 虽如此建议，但那需要在 `__ne__` 未定义时改调 `__eq__` 再取反，属于额外分支，本任务不做，用户类需显式定义 `__ne__`，缺失时走 `msValueEqual` 的既有身份/结构相等回退（与 `OP_EQ` 保持一致）。`OP_LT`/`OP_LE`/`OP_GT`/`OP_GE`（`msValueLt`/`Le`/`Gt`/`Ge` 辅助函数内部）与 `in`（`msContains`）**没有**这种"缺失即回退"的安全默认（大小比较/成员判定没有意义上等价的身份判定），按 `OP_ADD` 的模式接入 `tpLt`/`tpLe`/`tpGt`/`tpGe`/`tpContains`，分派失败即 TypeError；反向方法均传 `MS_NIL_VAL`（比较运算符不支持反向重载，`type-system.md §3.4` 未定义）。

### 6. 下标读写删除接入

`OP_GET_ITEM`/`OP_SET_ITEM` 与算术运算符同形状（计算值→压栈→继续），接入方式相同（`tpGetitem`/`__getitem__`、`tpSetitem`/`__setitem__`，均无反向方法）。`OP_DEL_ITEM` 是语句形态——原生路径「调用 `tpDelitem`，不压栈」——若改走压帧分派，子帧 `OP_RETURN` 默认会把 `__delitem__` 的返回值压回调用帧，破坏编译器预期的栈平衡（`compileDel` 不产生配套 `OP_POP`）。为此使用新增的 `discardReturn` 字段：

```c
// 文件: src/vm/ms_vm.c
case OP_DEL_ITEM: {
  MsValue key = PEEK(0), obj = PEEK(1);
  struct MsType* tp = msTypeOf(obj);
  if (!tp->tpDelitem && msIsInstance(obj)) {
    POP();
    POP();
    bool ok;
    MsFrame* nf = dispatchMagicBinary(t, obj, key, gDunderDelitem, MS_NIL_VAL, &ok);
    if (!ok) {
      return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
    }
    // discardReturn 标记在调用方帧（frame，即将被切走）上，不是 nf——
    // 与下面 forIterPending 的放置方式一致：调用方决定如何处理返回值，
    // 被调用帧（nf/__delitem__ 自身）只管正常 OP_RETURN，不知情。
    frame->discardReturn = true;
    frame = nf;
    DISPATCH();
  }
  POP();
  POP();
  if (!tp->tpDelitem) {
    return MS_ERROR_VALUE;  // TypeError: does not support item deletion (T080 placeholder)
  }
  MsValue r = tp->tpDelitem(&gVM, obj, key);
  if (MS_IS_ERROR(r)) {
    return r;
  }
  DISPATCH();
}
```

`discardReturn` 的处理见 §8 `OP_RETURN`/`OP_RETURN_NIL` 改造。

### 7. 迭代协议接入（`__iter__`/`__next__`）

`OP_GET_ITER`（`s[0] = s[0].__iter__()`）是「计算值→压栈→继续」形状，接入方式同一元运算符（`tpIter`/`__iter__`，零显式实参，只压 self）。`OP_FOR_ITER` 是「计算值→按值分支跳转或压栈→继续」形状，`OP_RETURN` 的默认「压回结果」语义不够——必须在子帧返回后先判定 `nil` 再决定跳转还是压栈。为此使用新增的 `forIterPending`/`forIterOffset` 字段：

```c
// 文件: src/vm/ms_vm.c
case OP_FOR_ITER: {
  int32_t offset = READ_JUMP_OFFSET();
  MsValue iter = PEEK(0);
  struct MsType* tp = msTypeOf(iter);
  if (!tp->tpNext && msIsInstance(iter)) {
    bool ok;
    MsFrame* nf = dispatchMagicUnary(t, iter, gDunderNext, &ok);
    if (!ok) {
      return MS_ERROR_VALUE;  // TypeError (T080 placeholder): not an iterator
    }
    frame->forIterPending = true;  // 标记在当前帧（即将被切走的调用方），
    frame->forIterOffset = offset;  // 不是 nf——nf 是 __next__ 自己的新帧
    frame = nf;
    DISPATCH();
  }
  if (!tp->tpNext) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder): not an iterator
  }
  MsValue val = tp->tpNext(&gVM, iter);
  if (MS_IS_NIL(val)) {
    (void) POP();
    frame->ip += offset;
  } else {
    PUSH(val);
  }
  DISPATCH();
}
```

### 8. `OP_RETURN`/`OP_RETURN_NIL` 改造：`discardReturn` 与 `forIterPending`

在既有 `isCtor`/`boundCall` 分支之后，`PUSH(result)` 之前插入判定（`t->topFrame` 此时已切换为调用方帧，即 §6/§7 中设置了 `discardReturn`/`forIterPending` 的那个帧）：

```c
// 文件: src/vm/ms_vm.c（OP_RETURN 与 OP_RETURN_NIL 内，msFreeFrame(frame) 之后，
// 原来的 `PUSH(result); frame = t->topFrame; DISPATCH();` 替换为：）
if (t->topFrame->forIterPending) {
  t->topFrame->forIterPending = false;
  if (MS_IS_NIL(result)) {
    (void) POP();  // 弹出 OP_FOR_ITER 留在栈顶、未被消费的迭代器
    t->topFrame->ip += t->topFrame->forIterOffset;
  } else {
    PUSH(result);
  }
} else if (!t->topFrame->discardReturn) {
  PUSH(result);
}
t->topFrame->discardReturn = false;
frame = t->topFrame;
DISPATCH();
```

---

## 验收标准（checklist）

<!-- v:... 标签供 verify_task.py 自动勾选，见 _template.md -->
- [x] 编译通过，无警告（`cmake --build build`）。 <!-- v:build -->
- [x] C 单测通过。 <!-- v:ctest:test_magic_methods -->
- [x] `.ms` 端到端测试输出与期望一致。 <!-- v:ms:ms_m2_magic_methods -->
- [x] `class Vec { func __add__(self, o) { return Vec(self.x + o.x, self.y + o.y) } }; Vec(1, 2) + Vec(3, 4)` → 新实例，`x=4, y=6`。 <!-- v:ms:ms_m2_magic_methods -->
- [x] `class Vec { func __eq__(self, o) { return self.x == o.x and self.y == o.y } }; Vec(1, 2) == Vec(1, 2)` → `true`。 <!-- v:ms:ms_m2_magic_methods -->
- [x] `a + b`（`a` 无 `__add__`，`b` 有 `__radd__`）→ 调用 `b.__radd__(a)`。 <!-- v:ctest:test_magic_methods -->
- [x] `class Range3 { func __iter__(self) { return self } func __next__(self) { ... } }` → 可被 `for` 循环迭代直至 `__next__` 返回 `nil`。 <!-- v:ms:ms_m2_magic_methods -->
- [x] `class Box { func __getitem__(self, k) { ... } func __setitem__(self, k, v) { ... } func __delitem__(self, k) { ... } }` → `box[k]`/`box[k] = v`/`del box[k]` 均分派到对应魔术方法。 <!-- v:ms:ms_m2_magic_methods -->
- [x] 两侧都无对应魔术方法（正向与反向均缺失）→ TypeError（`MS_ERROR_VALUE`，T080 占位）。 <!-- v:ctest:test_magic_methods -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/vm/test_magic_methods.c`）

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

static void testAddDispatch(void) {
  MsValue v = run(
      "class Vec { func __init__(self, x) { self.x = x } func __add__(self, o) { return self.x + o.x } }\n"
      "Vec(1) + Vec(2)");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "Vec(1) + Vec(2) dispatches __add__");
}

static void testReverseAdd(void) {
  MsValue v = run(
      "class R { func __init__(self, x) { self.x = x } func __radd__(self, o) { return o + self.x } }\n"
      "1 + R(2)");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "1 + R(2) falls back to __radd__");
}

static void testNoDunderIsTypeError(void) {
  MsValue v = run("class Plain {}\nPlain() + Plain()");
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "no __add__/__radd__ on either side is a TypeError");
}

int main(void) {
  MS_RUN(testAddDispatch);
  MS_RUN(testReverseAdd);
  MS_RUN(testNoDunderIsTypeError);
  return msTestSummary();
}
```

### `.ms` 端到端测试（`tests/ms/m2/magic_methods.ms` + `magic_methods.expected`）

```ms
// magic methods: operator/subscript/iteration dispatch via dunder lookup (T074)

class Vector {
    func __init__(self, x, y) {
        self.x = x
        self.y = y
    }

    func __add__(self, other) {
        return Vector(self.x + other.x, self.y + other.y)
    }

    func __mul__(self, n) {
        return Vector(self.x * n, self.y * n)
    }

    func __eq__(self, other) {
        return self.x == other.x and self.y == other.y
    }
}


v1 := Vector(1, 2)
v2 := Vector(3, 4)
v3 := v1 + v2
print(v3.x, v3.y)          // 4 6
v4 := v1 * 3
print(v4.x, v4.y)          // 3 6
print(v1 == Vector(1, 2))  // true


class Countdown {
    func __init__(self, n) {
        self.n = n
    }

    func __iter__(self) {
        return self
    }

    func __next__(self) {
        if self.n <= 0 {
            return nil
        }
        self.n = self.n - 1
        return self.n + 1
    }
}


for x in Countdown(3) {
    print(x)                // 3, 2, 1（各占一行）
}
```

---

## Benchmark

N/A（魔术方法调用路径复用普通方法调用的压帧机制，性能归入 `P5-T072-class-instantiation.md` 的 `benchmarks/bench_class.ms`；`msIsInstance` 身份判定与 MRO 查找的额外开销可在该 bench 基础上叠加 `__add__` 循环观察，本任务不单列）。

---

## 风险与边界

- **不支持可重入调用路径**（§0 已确立的核心边界）：`__bool__`/`__len__`（真值测试与 `len()` 内置）、`__not__`、`__hash__`（map/set 键）、`__str__`/`__repr__`（`print`/`stringify`）均**不在本任务范围**——这些路径要求在同一次 C 函数调用内同步拿到结果（`msValueTruthy`/`msBuiltinLen`/`hashValue`/`stringify` 都是普通 C 函数，不是可挂起-恢复的 opcode case），本任务采用的"压帧+`OP_RETURN`延续"机制无法覆盖。用户类若只定义 `__bool__`/`__len__`/`__str__`/`__repr__`，其行为退回 T072/T074 之前的既有缺省（真值恒为 `true`、`len()` 报 TypeError、`print` 打印 `?`）。后续若要支持，需要先实现真正的可重入求值入口或更通用的 opcode 延续机制，非本任务能力范围内的追加工作。
- **`__call__` 不在本任务**：`tp->mstype.call`（非 `tpCall`）槽保留给 T077，本任务不装配。
- **位移运算符不支持重载**：`__lshift__`/`__rshift__` 在 `MsType` 中没有对应槽，`SHIFT_OP` 宏维持 int-only，本任务不新增槽、不接入。
- **热路径开销**：内置类型间的算术运算（`int + int` 等）原生槽一次命中即返回，开销不变；但每次原生槽缺失或"declined"（返回 `MS_ERROR_VALUE`）都会额外调用一次 `dispatchMagicBinary`/`dispatchMagicUnary`（内部 `msTypeLookupMethodMRO` 沿 MRO 链做 map 查找），对涉及用户类的热路径代码有影响。可用**内联缓存**（inline cache，缓存上次命中的类型与方法）优化，本任务不实现。
- **`__eq__` 与 hash 一致性**：`__hash__` 不在本任务范围（见上），因此定义了 `__eq__` 的实例目前依然不可作 map/set 键（`tpHash` 恒为 `NULL`）；这与是否定义 `__eq__` 无关，是既有默认行为的自然延续，非本任务引入的新限制。
- **反向运算符语义与 Python 不同**：mslang 无 `NotImplemented` 单例；脚本定义的正向 dunder 一旦被 MRO 命中就不会再改试反向（无论调用结果如何），"拒绝后重试反向"只对内置类型的原生槽成立（原生槽返回 `MS_ERROR_VALUE` 视为拒绝信号，见 §0）。
- **`OP_EQ`/`OP_NE` 与 `msValueEqual` 分叉**：opcode 层新增的 MRO 分派路径与 `msValueEqual`（供 map/set 等 C 内部路径使用的同步 `bool` 版本）现在是两套独立实现，行为需保持一致（内置类型的比较结果）但不共享代码路径；`msValueEqual` 本身不改动。
- **子类覆盖优先级**：`msTypeLookupMethodMRO` 沿 MRO 顺序查找，子类重写的魔术方法自动优先于父类（继承自 T073 的既定行为），本任务不需要额外处理。
