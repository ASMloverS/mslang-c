# P12-T189 stdlib: uuid / enum / copy

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现三个小型工具模块：`uuid`（UUID 生成，对齐 `stdlib/uuid.md`）、`enum`（枚举类型）、`copy`（对象复制）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T166 | secrets（uuid4 使用 os.urandom） |
| P5-T072 | class 系统（enum 继承） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-uuid-enum-copy.md` | §1 模块 API |

---

## API 清单

```ms
// uuid
uuid.uuid1() → UUID     // 基于 MAC 地址 + 时间（版本 1）
uuid.uuid3(namespace, name) → UUID  // MD5 命名（版本 3）
uuid.uuid4() → UUID     // 随机（版本 4，最常用）
uuid.uuid5(namespace, name) → UUID  // SHA-1 命名（版本 5）
uuid.UUID(hex=nil, bytes=nil, fields=nil, int=nil, version=nil)

// UUID 命名空间常量
uuid.NAMESPACE_DNS    // 6ba7b810-9dad-11d1-80b4-00c04fd430c8
uuid.NAMESPACE_URL
uuid.NAMESPACE_OID
uuid.NAMESPACE_X500

// UUID 属性
u.hex         // "12345678123412341234123456789abc"（32 hex，无连字符）
u.bytes       // bytes（16 字节，big-endian）
u.int         // int（128 位整数）
u.version     // 1-5
u.variant     // "specified in RFC 4122"
str(u)        // "12345678-1234-1234-1234-123456789abc"
repr(u)       // "UUID('12345678-...')"
u == u2  u < u2   // 比较

// enum
class Color(enum.Enum):
    RED   = 1
    GREEN = 2
    BLUE  = 3

Color.RED           // Color.RED
Color.RED.name      // "RED"
Color.RED.value     // 1
Color(1)            // Color.RED（按值查找）
Color["RED"]        // Color.RED（按名查找）
list(Color)         // [Color.RED, Color.GREEN, Color.BLUE]

class Status(enum.IntEnum):   // 整数枚举（可参与 int 运算）
    ACTIVE = 1
    INACTIVE = 0

class Flags(enum.Flag):       // 位标志（支持 | & ~）
    READ = 1
    WRITE = 2
    EXEC = 4
Flags.READ | Flags.WRITE  // Flags.READ|Flags.WRITE（value=3）

// @enum.unique 装饰器：检查值唯一性
@enum.unique
class Direction(enum.Enum):
    NORTH = "N"
    SOUTH = "S"

// copy
copy.copy(obj) → obj      // 浅复制
copy.deepcopy(obj) → obj  // 深复制（递归）
// 定制：__copy__ / __deepcopy__
```

---

## 实现要点

```c
// uuid4：os.urandom(16) → 设置版本位(4)和变体位 → UUID

// uuid1：使用 MAC 地址（getifaddrs / GetAdaptersInfo）+ 100ns 时间戳（1582年起）
// MAC 随机化：若无法获取真实 MAC，用 os.urandom(6) | 0x010000000000（多播位）

// uuid3/5：MD5/SHA-1(namespace_bytes + name_bytes)，截取 16 字节设置版本/变体

// enum 实现：元类 EnumMeta
// 类定义时拦截属性设置，将 Class.ATTR = val 转为 EnumMember(name, val)
// __iter__：返回所有成员列表
// __call__(value)：按值查找，O(n) 遍历或 value→member 字典

// Flag：支持 __or__ __and__ __xor__ __invert__
// 复合 Flag（组合值）：按需创建或查找

// copy.copy：
// 内置类型（int/str/bool/nil）→ 直接返回（不可变）
// list → list(obj)（浅复制）
// dict → dict.copy()
// 其他：__copy__() 或 浅复制实例属性

// copy.deepcopy：
// 使用 memo dict（id→copy）防止循环引用
// 递归深拷贝所有子对象
```

---

## 验收标准（checklist）

- [ ] `uuid.uuid4()` 每次生成不同 UUID，格式为 8-4-4-4-12 hex。
- [ ] `UUID(str(u)) == u`（round-trip）。
- [ ] `Color.RED.value == 1`，`Color(1) is Color.RED`。
- [ ] `Flags.READ | Flags.WRITE` 正确位运算。
- [ ] `copy.deepcopy({"a": [1,2]})` 深复制（修改副本不影响原）。
- [ ] `@enum.unique` 对重复值抛 ValueError。

---

## 测试用例（.ms）

```ms
import uuid, enum, copy

// uuid4
u1 := uuid.uuid4()
u2 := uuid.uuid4()
print(u1 != u2)         // true（极高概率）
print(len(str(u1)))     // 36（含 4 个连字符）
print(u1.version)       // 4
print(uuid.UUID(str(u1)) == u1)  // true

// enum
class Planet(enum.Enum):
    MERCURY = 1; VENUS = 2; EARTH = 3
print(Planet.EARTH.name)   // "EARTH"
print(Planet.EARTH.value)  // 3
print(Planet(2))           // Planet.VENUS
print(list(Planet))        // [MERCURY, VENUS, EARTH]

class Permission(enum.Flag):
    R = 4; W = 2; X = 1

rwx := Permission.R | Permission.W | Permission.X
print(rwx.value)          // 7
print(Permission.R in rwx)  // true

// copy
original := {"a": [1,2,3], "b": {"nested": true}}
shallow := copy.copy(original)
deep := copy.deepcopy(original)

shallow["a"].append(4)
print(original["a"])   // [1,2,3,4]（共享引用）

deep["a"].append(5)
print(original["a"])   // [1,2,3,4]（不受影响）
print(deep["a"])       // [1,2,3,5]（独立副本）
```
