# P5-T070 kwargs（关键字参数 / **kwargs）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

扩展调用约定以支持关键字参数：`OP_CALL_KW`（调用时通过名称传递参数，含单个 `**expr` 运行时 map 作为关键字来源）、`**kwargs` 收集（多余关键字参数打包为 map）。关键字专用参数（kw-only，`*` 后的参数只能通过关键字传递）因 `syntax.md` ParamList 文法（§2.1）尚未定义对应分隔符、解析器亦未实现该分支，本任务不实现，留待 `syntax.md` 补充文法后另立任务跟进（见「风险与边界」）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T069 | vararg 基础（`msClosureCall` 默认值/vararg 顺序、`dispatchCall` 共享调用分发） |
| P4-T060 | map（`**kwargs` 收集结果与 `**expr` 展开来源均为 map） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.6 函数调用与返回（`CALL_KW` 语义：单字节 argc + 栈顶 kwargs map） |
| `syntax.md` | §2.1 ParamList（`**kwargs` 收集文法）、§2.3 ArgList（调用侧 `name=value`/`**expr` 文法） |
| `type-system.md` | §2.8 map（kwargs 载体）、§2.12 function/closure（`hasKwarg` 字段） |

---

## 待实现（C 文件）

```
src/compiler/ms_compiler.c   # compileCallKw 扩展（单个 **expr、无字面量 kwarg 混用时直接作为 kwargs map）；
                              # compileCall 分支调整（doublestar-only 场景走 compileCallKw 而非 compileCallEx）
src/runtime/ms_func.c        # msClosureCallKw：位置参数 + kwargs map 绑定
src/vm/ms_vm.c               # OP_CALL_KW case
```

---

## 实现要点

### 0. 现状与范围（编译器侧，T068 遗留）

`ms_compiler.c` 的 `compileCallKw` **已经**按 `vm.md §3.6` 的模型工作：纯字面量关键字参数
（`f(a=1, b=2)`，无 `*`/`**`）时，逐个把参数名压为字符串常量、值压栈，再 `OP_BUILD_MAP kwCount`
生成 map，最后 `OP_CALL_KW posArgc`——**栈布局为 `[callee, pos_arg0..posArgN-1, kwargsMap]`**，
与本任务原草稿设想的「常量池 kwNames tuple」模型完全不同且已提交锁定。本任务只需实现
`vm.md` 已锁定语义的**运行时**（`OP_CALL_KW` 分发 + 参数绑定），不得改动这一栈布局约定。

`compileCallEx`（`f(*args)` 展开，T069）当前对 `**expr`（`MS_ND_DOUBLESTAR_EXPR`）只有一行
`// TODO T068` 占位：把 expr 压栈但 `OP_CALL_EX` 并不消费它，是未完成分支；`compileCall` 目前
只要检测到任意 `**expr` 就无条件走 `compileCallEx`。本任务将其改为：**若某次调用的 kwargs
列表只含一个 `**expr` 且不含任何字面量 `name=value`**，直接把该 expr 的运行时 map 值作为
`OP_CALL_KW` 的栈顶 kwargs map（跳过 `OP_BUILD_MAP`），走 `compileCallKw` 而非 `compileCallEx`。

**范围之外（本任务不实现）**：`*expr`（位置展开）与关键字参数（字面量或 `**expr`）在同一调用内
混用（如 `f(*lst, a=1)`），以及字面量 kwarg 与 `**expr` 混用（如 `f(a=1, **opts)`）——两者都需要
"运行时合并两个 map/序列"，当前 opcode 集（`vm.md §3` 全表，无 map 合并指令）无法表达，需先扩展
`vm.md`（如新增 map 合并 opcode）才能实现，留给后续任务（对应 `ms_compiler.c` 中既有的
`// TODO T068` 标记）。

### 1. OP_CALL_KW（混合位置 + 关键字参数）

```c
// OP_CALL_KW [1B: posArgc]（vm.md §3.6：A: argc）
// 栈（自底向上）：[callee, pos_arg0..posArgN-1, kwargsMap]
// kwargsMap 由编译器装配（OP_BUILD_MAP，或单个 **expr 的运行时 map 直接充当，见「0.」）
case OP_CALL_KW: {
  uint8_t posArgc = READ_BYTE();
  MsValue kwargsMap = PEEK(0);
  MsValue callee = PEEK(posArgc + 1);
  if (MS_IS_OBJ(callee) && MS_AS_OBJ(callee)->type == &msNativeFnType) {
    return msTypeError(t, "keyword arguments not supported for native functions");  // T080 placeholder
  }
  if (!MS_IS_OBJ(callee) || MS_AS_OBJ(callee)->type != &msClosureType) {
    return msTypeError(t, "not callable");  // T080 placeholder
  }
  MsClosure* cl = (MsClosure*) MS_AS_OBJ(callee);
  MsFrame* newFrame = msClosureCallKw(t, cl, (uint32_t) posArgc, kwargsMap);
  if (!newFrame) return MS_ERROR_VALUE;  // TypeError（T080 placeholder）
  frame = newFrame;
  DISPATCH();
}
```

### 2. msClosureCallKw（参数绑定，ms_func.c）

```c
// 位置参数 + 关键字参数绑定。复用 msClosureCall 的帧分配/局部槽预留/vararg 收尾
// 逻辑（T068/T069），仅将「参数填充」阶段替换为：先按位置填充 slots[0..posArgc-1]，
// 再遍历 kwargsMap 按参数名匹配剩余槽位，最后对仍未填充的槽位应用默认值。
struct MsFrame* msClosureCallKw(struct MsThread* t, MsClosure* cl,
                                 uint32_t posArgc, MsValue kwargsMap) {
  MsFuncProto* proto = cl->proto;
  if (posArgc > proto->arityMax) {
    return NULL;  // TypeError: too many positional arguments（T080 placeholder）
  }

  // ...（分配新帧，复制位置参数到 slots[0..posArgc-1]，与 msClosureCall 一致，此处从略）...

  bool filled[256] = {0};  // arityMax 上限沿用 T068 argc 的 uint8_t 约定
  for (uint32_t i = 0; i < posArgc; i++) filled[i] = true;

  MsValue kwargsExtra = proto->hasKwarg ? msNewMap(4) : MS_NIL_VAL;
  if (proto->hasKwarg) msGCPushRoot(kwargsExtra);

  MsMapIter it = msMapIterBegin(kwargsMap);
  MsValue key, val;
  while (msMapIterNext(&it, &key, &val)) {
    int slot = msFindParamSlot(proto, key);
    if (slot < 0) {
      if (!proto->hasKwarg) {
        if (proto->hasKwarg) msGCPopRoot();
        return NULL;  // TypeError: unexpected keyword argument（T080 placeholder）
      }
      msMapSet(MS_AS_OBJ(kwargsExtra), key, val);
      continue;
    }
    if (filled[slot]) {
      if (proto->hasKwarg) msGCPopRoot();
      return NULL;  // TypeError: got multiple values for argument（T080 placeholder，重复关键字）
    }
    newFrame->slots[slot] = val;
    filled[slot] = true;
  }

  // 默认值填充：仅补齐未被位置/关键字填充的槽位（顺序与 msClosureCall 一致：先默认值后 vararg）
  for (uint32_t i = 0; i < proto->arityMax; i++) {
    if (filled[i]) continue;
    uint32_t defIdx = proto->defaultCount - 1 - (i - proto->arity);
    if (i < proto->arity || defIdx >= proto->defaultCount) {
      if (proto->hasKwarg) msGCPopRoot();
      return NULL;  // TypeError: missing required argument（T080 placeholder）
    }
    newFrame->slots[i] = proto->defaults[defIdx];
  }

  if (proto->hasKwarg) {
    newFrame->slots[proto->kwargsSlot] = kwargsExtra;
    msGCPopRoot();
  }

  // ...（局部槽预留，见 T068，此处从略；本任务不涉及 vararg+kwarg 混用，见「0.」范围之外）...
  return newFrame;
}
```

### 3. MsFuncProto 字段增量 + msFindParamSlot

```c
// include/mslang/ms_func.h 新增字段（由 T043 编译器扩展负责填充，
// 与 T068 既已预留的 hasKwarg/kwOnlyCount 一同扩充 MsFuncProto）：
struct MsFuncProto {
  // ...（T068/T069 既有字段不变：arity/arityMax/defaultCount/defaults/
  //     localCount/upvalueCount/isAsync/hasVararg/hasKwarg/kwOnlyCount）...
  const char** paramNames;     // 参数名数组，长度 arityMax（不含 vararg/kwarg 收集槽自身的名字）
  uint32_t*    paramNameLens;  // 对应字节长度（非 NUL 结尾）
  uint32_t     kwargsSlot;     // **kwargs 收集槽的局部槽索引（proto->hasKwarg 时有效）
};

// 按 proto->paramNames 线性查找 name 对应的参数槽位；无匹配返回 -1。
// O(arityMax) 扫描，参数数量通常 <10，可接受（同原草稿风险评估）。
int msFindParamSlot(MsFuncProto* proto, MsValue name);
```

---

## 验收标准（checklist）

- [ ] `func f(a, b) { return a, b }; f(b=2, a=1)` → a=1, b=2（关键字参数，顺序无关）。
- [ ] `func f(a, b=10) { return a, b }; f(a=5)` → a=5, b=10（关键字命中默认值槽，跳过位置绑定）。
- [ ] `func f(a, **kwargs) { return kwargs }; f(1, x=2, y=3)` → kwargs = `{"x": 2, "y": 3}`。
- [ ] `func f(a, **kwargs) { return a, kwargs }; f(**{"a": 1, "b": 2})` → a=1, kwargs = `{"b": 2}`（单个 `**expr` 独立展开，无字面量 kwarg 混用）。
- [ ] `func f(a, b) {}; f(1, a=2)` → TypeError（重复关键字参数 a：位置已填、关键字再赋值）。
- [ ] `func f(a, b) {}; f(1, 2, c=3)` → TypeError（未知关键字参数 c，函数无 `**kwargs` 收集）。
- [ ] `func f(a, b) {}; f(b=2)` → TypeError（缺少必需参数 a：既未按位置提供，也未按关键字提供）。

---

## 测试用例（.ms）

```ms
func configure(host, port=80, ssl=false, timeout=30) {
    print($"{host}:{port} ssl={ssl} timeout={timeout}")
}


configure("example.com")
// example.com:80 ssl=false timeout=30

configure("example.com", 443, true)
// example.com:443 ssl=true timeout=30

configure("example.com", ssl=true, timeout=90)
// example.com:80 ssl=true timeout=90


func passthrough(first, **kwargs) {
    return kwargs
}


print(passthrough(1, a=2, b=3))
// {"a": 2, "b": 3}

// 单个 **expr 独立展开（无字面量 kwarg 混用）
opts := {"timeout": 90}
configure("api.example.com", **opts)
// api.example.com:80 ssl=false timeout=90
```

---

## Benchmark

N/A（归入 T068 call bench）。

---

## 风险与边界

- **kw-only 参数暂不实现**：`syntax.md` §2.1 ParamList 文法未定义 `*` 作为关键字专用参数分隔符
  （`§1.10` 中 `*`/`**` 亦无此语义），解析器也未实现对应分支；`MsFuncProto.kwOnlyCount` 字段
  （T068 预留）暂不使用。待 `syntax.md` 补充该文法后，另立任务实现（不在本任务范围内）。
- **`**expr` 与其他关键字来源混用暂不实现**：`*expr`（位置展开）与关键字参数同调用混用
  （`f(*lst, a=1)`），以及字面量 kwarg 与 `**expr` 混用（`f(a=1, **opts)`），需要运行时合并
  两个 map/序列，当前 opcode 集无对应指令（`vm.md §3` 无 map 合并 op）；对应
  `ms_compiler.c` 中既有的 `// TODO T068` 标记，留给后续任务（需先扩展 `vm.md`）。
- **kwarg 查找复杂度**：`msFindParamSlot` 对每个关键字做线性扫描（O(k×p)，k=kwarg数，p=参数数）；大多数函数参数少（<10），可接受。
- **`paramNames` 存储**：`MsFuncProto` 需要存储参数名列表（`const char** paramNames, uint32_t* paramNameLens`）供 kwarg 匹配使用；编译器（T043 扩展）需填充此字段。
- **跨文档字段分歧（既存，非本任务引入）**：`type-system.md` §2.12 的 `MsFunction` 结构未列出
  `kwOnlyCount`/`paramNames` 等字段（散落于 T068 `MsFuncProto`）；本任务以 `MsFuncProto` 为准，
  `type-system.md` 的同步留待单独 doc-fix 任务处理。
