# P5-T070 kwargs（关键字参数 / **kwargs）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

扩展调用约定以支持关键字参数：`OP_CALL_KW`（调用时通过名称传递参数）、`**kwargs` 收集（多余关键字参数打包为 map）、关键字专用参数（`*` 后的参数只能通过关键字传递）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T069 | vararg 基础 |
| P4-T060 | map（**kwargs 为 map） |
| P4-T061 | tuple（kwarg 名称 tuple） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §4.2 CALL_KW 调用约定 |
| `vm.md` | §4.3 CALL_EX + kwargs 合并 |
| `syntax.md` | §3 关键字参数语法 |

---

## 待实现（C 文件）

```
src/vm/ms_vm.c    # OP_CALL_KW / CALL_EX（kwargs 分支）
```

---

## 实现要点

### 1. OP_CALL_KW（混合位置 + 关键字参数）

```c
// OP_CALL_KW [1B: posArgc] [2B: kwNamesTupleIdx]
// 栈：[callee, pos_arg0..posArgN, kw_val0..kw_valK]
// kwNamesTupleIdx：常量池中的 MsTuple（str 名称列表）
case OP_CALL_KW: {
    uint8_t  posArgc     = READ_BYTE();
    uint16_t kwNamesIdx  = READ_U16();
    MsValue  callee      = PEEK(posArgc + /* kwCount from tuple */);
    MsTupleObj* kwNames  = (MsTupleObj*)MS_AS_OBJ(frame->chunk->consts[kwNamesIdx]);
    uint32_t    kwCount  = kwNames->len;

    // 将关键字参数绑定到对应参数槽
    MsClosureObj* cl    = (MsClosureObj*)MS_AS_OBJ(callee);
    MsFuncProto*  proto = cl->proto;

    // 解析参数映射：根据 kwNames 将 kw_val 放到正确槽位
    MsValue* slots  = t->sp - kwCount - posArgc;  // callee 之后的位置
    MsValue  kwArgs[256]; uint8_t kwSlots[256];    // 关键字参数的目标槽

    for (uint32_t i = 0; i < kwCount; i++) {
        MsValue kwName = kwNames->items[i];
        // 在 proto->paramNames 中查找对应槽位
        int slot = msFindParamSlot(proto, kwName);
        if (slot < 0) {
            if (proto->hasKwarg) {
                // 放入 **kwargs（T070）
                // ...
            } else {
                return msTypeError(t, "unexpected keyword argument '%s'", ...);
            }
        } else {
            slots[slot] = *(t->sp - kwCount + i);
        }
    }
    // 继续完成调用帧创建...
}
```

### 2. **kwargs 收集

```c
// proto->hasKwarg = true 时，**kwargs 槽在 arityMax 后
// 未被显式参数名匹配的关键字参数打包为 map

MsValue kwargsMap = msNewMap(4);
for each unmatched kwarg:
    msMapSet(kwargsMap, kwName, kwVal);
frame->slots[proto->kwargsSlot] = kwargsMap;
```

### 3. 关键字专用参数（kw-only）

```c
// func f(a, *, b=0, c):  b 和 c 只能通过关键字传递
// proto->kwOnlyCount > 0 时，slots[arity..arity+kwOnlyCount-1] 是 kw-only 参数
// 若 OP_CALL（无 kwarg）调用：kw-only 无默认值 → TypeError
```

---

## 验收标准（checklist）

- [ ] `func f(a, b): f(b=2, a=1)` → a=1, b=2（关键字参数）。
- [ ] `func f(a, **kwargs): f(1, x=2, y=3)` → kwargs={"x":2,"y":3}。
- [ ] `func f(a, *, b): f(1, b=2)` → 正确；`f(1, 2)` → TypeError（b 是 kw-only）。
- [ ] `f(**{"a": 1, "b": 2})` → 等价 `f(a=1, b=2)`。
- [ ] 重复关键字参数 `f(1, a=2)` → TypeError。
- [ ] 未知关键字（无 **kwargs）→ TypeError。

---

## 测试用例（.ms）

```ms
func configure(host, port=80, *, ssl=false, timeout=30) {
    print($"{host}:{port} ssl={ssl} timeout={timeout}")
}
configure("example.com")
// example.com:80 ssl=false timeout=30
configure("example.com", 443, ssl=true)
// example.com:443 ssl=true timeout=30

func passthrough(**kwargs) { return kwargs }
print(passthrough(a=1, b=2))  // {"a": 1, "b": 2}

// 展开 kwargs
opts := {"timeout": 60}
configure("api.example.com", **opts)
// api.example.com:80 ssl=false timeout=60
```

---

## Benchmark

N/A（归入 T068 call bench）。

---

## 风险与边界

- **kwarg 查找复杂度**：`msFindParamSlot` 对每个关键字做线性扫描（O(k×p)，k=kwarg数，p=参数数）；大多数函数参数少（<10），可接受。
- **`paramNames` 存储**：`MsFuncProto` 需要存储参数名列表（`const char** paramNames, uint32_t* paramNameLens`）供 kwarg 匹配使用；编译器（T043 扩展）需填充此字段。
