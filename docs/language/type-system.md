# mslang 类型系统与对象模型

## 1. 对象模型

### 1.1 MsObject 头部

所有堆分配对象共享一个统一头部（C 结构）：

```c
struct MsObject {
  union {
    struct MsType*   type;  // 正常态：指向类型描述符
    struct MsObject* fwd;   // GC_FORWARDED 置位时复用为 to-space 目标地址（与 type 共用 union）
  };
  uint32_t gcFlags;  // GC 标记位、分代位、转发标志
  // 具体对象字段紧随其后
};
```

**gcFlags 布局**（32 位）：

| 位 | 含义 |
|---|---|
| 0 | 标记位（mark-sweep） |
| 1-2 | 分代（0=年轻, 1=中, 2=老） |
| 3 | 已被复制（半区复制 forwarded） |
| 4 | 可终结（有 `__del__`） |
| 5-31 | 保留 |

### 1.2 MsValue（栈上值表示）

VM 操作数栈与局部变量槽使用 `MsValue`，采用**标签联合**（tagged union）：

```c
typedef enum {
  MS_TAG_INT    = 0,   // int64
  MS_TAG_FLOAT  = 1,   // float64
  MS_TAG_BOOL   = 2,   // bool (imm)
  MS_TAG_NIL    = 3,   // nil
  MS_TAG_OBJ    = 4,   // struct MsObject* (heap)
  MS_TAG_ERROR  = 5,   // 错误哨兵（C API 错误传播，对应 MS_ERROR_VALUE）
} MsTag;

// MsValue 为核心值类型，保留 typedef 以简化按值传递的签名
typedef struct {
  MsTag    tag;
  union {
    int64_t          i;
    double           f;
    int              b;
    struct MsObject* obj;
  } as;
} MsValue;
```

小整数（int64）与浮点直接内联在栈上，**不触发堆分配**。`bool` 与 `nil` 亦内联。只有字符串、列表、映射、函数、实例等复合对象才分配到堆上，以 `MS_TAG_OBJ` 携带指针。

### 1.3 MsType（类型描述符）

```c
// 变长对象（MsStr/MsTuple/MsFrame）需提供 varSize 回调计算实际分配大小；
// 定长对象（MsList/MsMap/MsInstance 等）将此槽设为 NULL，使用 objSize。
typedef size_t (*MsSizeFn)(const struct MsObject* obj);

struct MsType {
  const char*  name;       // 类型名，如 "int"/"str"/"Dog"
  size_t       objSize;    // 定长对象：sizeof(具体对象)；变长对象：头部大小（不含柔性数组）
  MsSizeFn     varSize;    // 变长对象实际分配大小回调（NULL = 使用 objSize）
  MsTraverseFn traverse;   // GC 遍历子对象
  MsDestroyFn  destroy;    // 对象析构（GC 调用）
  MsCallFn     call;       // __call__ 默认实现
  // 内置类型槽：tpAdd, tpStr, tpEq, tpHash, tpGetitem, tpSetitem ...
  MsBinaryFn   tpAdd;
  MsBinaryFn   tpSub;
  MsBinaryFn   tpMul;
  MsBinaryFn   tpDiv;
  MsBinaryFn   tpMod;
  MsBinaryFn   tpPow;
  MsBinaryFn   tpEq;
  MsBinaryFn   tpNe;
  MsBinaryFn   tpLt;
  MsBinaryFn   tpLe;
  MsBinaryFn   tpGt;
  MsBinaryFn   tpGe;
  MsUnaryFn    tpNeg;
  MsUnaryFn    tpNot;
  MsUnaryFn    tpPos;      // __pos__
  MsUnaryFn    tpInvert;   // __invert__
  MsUnaryFn    tpHash;
  MsUnaryFn    tpStr;
  MsUnaryFn    tpRepr;
  MsUnaryFn    tpBool;
  MsUnaryFn    tpLen;
  MsBinaryFn   tpGetitem;
  MsTernaryFn  tpSetitem;
  MsUnaryFn    tpIter;
  MsUnaryFn    tpNext;
  // 用户类额外字段
  struct MsObject* baseClass;  // 父类（单继承）
  struct MsObject* mro;        // MRO 列表（预计算）
  struct MsObject* methods;    // 方法与类属性成员字典（struct MsMap）
};
```

---

## 2. 内置类型

### 2.1 int（int64）

- C 表示：`MsTag = MS_TAG_INT`，`as.i`（int64_t）。
- 算术溢出：回绕（无符号语义，符合 C 标准）。
- 字面量：`42`、`0xFF`、`0o77`、`0b1010`，支持 `_` 分隔。
- 内置操作：`+ - * / % ** & | ^ ~ << >>`；`/` 为多态除法：两操作数均为 int 时为整除，任一为 float 时为真除（对应魔术方法 `__div__`）。

### 2.2 float（float64）

- C 表示：`MsTag = MS_TAG_FLOAT`，`as.f`（double）。
- `1 / 2.0` 产生 float；`1 / 2` 产生 int（整除）。
- `math.inf`、`math.nan` 作为常量提供。

### 2.3 bool

- C 表示：`MsTag = MS_TAG_BOOL`，`as.b`（0 或 1）。
- `true`、`false` 字面量。
- 真值测试（`__bool__`）：`nil`、`false`、`0`、`0.0`、`""`、空 list、空 map 均为假。

### 2.4 nil

- C 表示：`MsTag = MS_TAG_NIL`。
- 函数无 `return` 值时默认返回 `nil`。

### 2.5 string

堆对象，**不可变** UTF-8 字节序列：

```c
struct MsStr {
  struct MsObject head;
  uint32_t        len;   // 字节长度
  uint32_t        hash;  // FNV-1a 缓存，0 表示未计算
  char            data[]; // 柔性数组，内联存储字节
};
```

- `len(s)` 返回字节长度，`s.codepoint_count()` 返回 Unicode 码点数。
- 索引 `s[i]` 返回第 i 字节（int）；切片 `s[a:b]` 返回子字符串（字节范围）。
- `for ch in s` 按 Unicode 码点迭代，`ch` 为单码点字符串。
- 字符串不可变，`+` 创建新字符串。

### 2.6 bytes

堆对象，**可变**字节数组：

```c
struct MsBytes {
  struct MsObject head;
  uint32_t        len;
  uint32_t        cap;
  uint8_t*        data;
};
```

- 字面量 `b"hello"`。
- 支持下标赋值 `buf[i] = 65`。

### 2.7 list

动态数组，元素类型异构：

```c
struct MsList {
  struct MsObject head;
  uint32_t        len;
  uint32_t        cap;
  MsValue*        items;  // 堆分配，GC 根
};
```

- 字面量 `[1, "two", 3.0]`。
- 操作：`append`/`pop`/`insert`/`remove`/切片/迭代/`in` 测试。
- 切片 `l[a:b:step]` 返回新 list。

### 2.8 map（hash map）

```c
struct MsMap {
  struct MsObject    head;
  uint32_t           len;
  uint32_t           cap;      // 必须为 2 的幂
  struct MsMapEntry* entries;
};

struct MsMapEntry {
  MsValue  key;
  MsValue  value;
  uint32_t hash;
  bool     occupied;
};
```

- 字面量 `{"a": 1, "b": 2}`。
- 键需可哈希（int、float、bool、string、nil、tuple of hashable）。
- 键约束：`nan`（`math.nan` / `float("nan")`）不可作 map 键，`m[math.nan] = v` 抛 `TypeError`（NaN 无法定义稳定哈希）；`-0.0` 与 `0.0` 视为同一键（哈希值相等，`==` 为真）。
- 操作：`m[k]`、`m[k] = v`、`del m[k]`、`k in m`、`m.keys()`/`m.values()`/`m.items()`。

### 2.9 tuple

不可变有序序列：

```c
struct MsTuple {
  struct MsObject head;
  uint32_t        len;
  MsValue         items[];
};
```

- 字面量 `(1, 2, 3)` 或 `(x,)`。
- 可用作 map 的键（若元素均可哈希）。

### 2.10 set

可变无序哈希集合：

```c
struct MsSet {
  struct MsObject head;
  uint32_t        len;
  uint32_t        cap;      // 必须为 2 的幂
  struct MsSetEntry* entries;
};

struct MsSetEntry {
  MsValue  item;
  uint32_t hash;
  bool     occupied;
};
```

- 字面量 `{1, 2, 3}`；**消歧**：空 set 必须用 `set()`，`{}` 仍为空 map。
- 元素需可哈希（规则同 map 键：int、float、bool、string、nil、tuple of hashable；`nan` 禁用）。
- 操作：`s.add(x)`、`s.remove(x)`（不存在抛 `KeyError`）、`s.discard(x)`（静默）、`s.pop()`、`s.clear()`、`x in s`(`__contains__`)。
- 集合运算：`s | t`(并)、`s & t`(交)、`s - t`(差)、`s ^ t`(对称差)；对应 `s.union(t)`、`s.intersection(t)`、`s.difference(t)`、`s.symmetric_difference(t)`。
- 关系运算：`s <= t`(子集)、`s < t`(真子集)、`s >= t`(超集)、`s > t`(真超集)。
- 就地运算：`|=`、`&=`、`-=`、`^=`。
- `len(s)` 返回元素数；`for x in s` 按任意顺序迭代。
- `__hash__` 不实现（set 不可哈希，不可作 map 键）。

### 2.11 frozenset

不可变无序哈希集合：

- 字面量：无专用字面量，使用 `frozenset({1, 2, 3})` 或 `frozenset([1, 2, 3])`。
- 与 set 相同的查询操作（`in`、`len`、迭代、集合运算），但不提供修改操作（`add`/`remove` 等）。
- 实现 `__hash__`，可作 map 的键（只要元素均可哈希）。
- 集合运算返回 frozenset（两操作数均为 frozenset 时）或 set。

### 2.12 function / closure

```c
struct MsFunction {
  struct MsObject   head;
  struct MsChunk*   chunk;        // 字节码
  struct MsStr*     name;
  uint8_t           arity;        // 形参个数（不含可变参数）
  bool              hasVararg;
  bool              isAsync;      // 是否 async func
  uint8_t           upvalueCount;
  struct MsUpvalue* upvalues[];   // 捕获的 upvalue
};
```

- 匿名函数/闭包共用同一结构。
- `isAsync=true` 时调用返回 `Future` 而非直接执行。

### 2.13 class 与 instance

见第 3 节。

---

## 3. Class 系统

### 3.1 声明

```ms
class Animal {
    func __init__(self, name) {
        self.name = name
    }
    func speak(self) {
        raise NotImplementedError("speak")
    }
    func __str__(self) {
        return "<Animal:" + self.name + ">"
    }
}

class Dog extends Animal {
    func speak(self) {
        print(self.name, "says: Woof!")
    }
}
```

- 单继承；`extends` 指定父类。
- 方法查找遵循 MRO（线性化，C3 算法保留扩展空间，当前单继承退化为链式）。
- `self` 为方法首参的命名约定，语言不强制此名（但社区约定遵守）。

### 3.2 Instance 结构

```c
struct MsInstance {
  struct MsObject head;   // head.type 指向 class 描述符（struct MsType*）
  struct MsMap*   attrs;  // 实例属性字典（动态）
};
```

属性查找顺序：① 实例 attrs → ② 类方法表（MRO 顺序）。

### 3.3 MRO 与方法查找

```
lookup(instance, name):
  if name in instance.attrs: return instance.attrs[name]
  for klass in instance.type.mro:
    if name in klass.methods: return klass.methods[name].bind(instance)
  raise AttributeError
```

### 3.4 魔术方法（特殊方法）

| 方法 | 触发时机 |
|---|---|
| `__init__(self, ...)` | `ClassName(...)` 构造 |
| `__del__(self)` | GC 终结前（不保证及时） |
| `__str__(self)` | `str(x)` / `print(x)` |
| `__repr__(self)` | `repr(x)` / REPL 显示 |
| `__bool__(self)` | 真值测试 `if x` |
| `__len__(self)` | `len(x)` |
| `__hash__(self)` | 用作 map 键 |
| `__eq__(self, other)` | `==` |
| `__ne__(self, other)` | `!=`（默认 `not __eq__`） |
| `__lt__`, `__le__`, `__gt__`, `__ge__` | 比较 |
| `__add__`, `__sub__`, `__mul__`, `__div__`, `__mod__`, `__pow__` | 算术 |
| `__and__`, `__or__`, `__xor__`, `__lshift__`, `__rshift__` | 位运算 |
| `__neg__`, `__pos__`, `__invert__` | 一元运算 |
| `__getitem__(self, key)` | `x[key]` |
| `__setitem__(self, key, value)` | `x[key] = value` |
| `__delitem__(self, key)` | `del x[key]` |
| `__contains__(self, item)` | `item in x` |
| `__iter__(self)` | `for v in x` |
| `__next__(self)` | 迭代器推进 |
| `__call__(self, ...)` | `x(...)` |
| `__enter__(self)` | `with x` 进入（保留） |
| `__exit__(self, ...)` | `with x` 退出（保留） |
| `__await__(self)` | `await x`（使对象成为 awaitable） |

### 3.5 `super()` 与父类调用

```ms
class B extends A {
    func __init__(self, x) {
        super().__init__(x)
        self.extra = 1
    }
}
```

`super()` 在方法内部返回父类的代理对象，属性查找从 MRO 的下一个类开始。

### 3.6 类属性 vs 实例属性

- **类属性**：在 `class {}` 块顶层赋值（不在方法内），存于 `struct MsType.methods`（方法与类属性成员字典）。
- **实例属性**：通过 `self.name = ...` 赋值，存于 `struct MsInstance.attrs`。
- 若实例属性与类属性同名，实例属性优先（遮蔽）。

---

## 4. 迭代器协议

实现 `__iter__(self)` 返回迭代器对象（通常 `return self`），实现 `__next__(self)` 推进迭代。当迭代结束时 `raise StopIteration`。set 与 frozenset 同样实现此协议；迭代顺序不保证。

内置类型均实现此协议（list、map、string、range、channel）。

---

## 5. 类型内省

```ms
type(x)         // 返回 x 的类型对象（struct MsType* 的脚本侧表示）
isinstance(x, T) // x 是否为 T 或其子类的实例
```
