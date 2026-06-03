# enum — 枚举类型

```ms
import enum
```

## 概述

提供符号枚举类型，基于 mslang class 系统实现，参考 Python `enum` 模块语义。
枚举成员是命名的常量，是唯一（单例）且不可变的值——两次访问 `Color.RED` 返回
同一对象，身份检查（`is` 等价的 `==`）可靠。

枚举类通过继承 `enum.Enum`（或其子类）声明，类体中的赋值语句定义成员。

## 常量与类型

| 类型 | 说明 |
|---|---|
| `enum.Enum` | 通用枚举基类；成员值可为任意类型 |
| `enum.IntEnum` | 整数枚举基类；成员与 `int` 可相互比较 |

`enum.auto()` 为辅助函数，非类型。

## 函数签名速查

| 函数/方法 | 签名 | 说明 |
|---|---|---|
| `auto` | `enum.auto() → sentinel` | 自动递增赋值（从 1 开始） |
| `unique` | `enum.unique(cls) → cls` | 验证无重复值；有重复则抛 ValueError |
| 按值查找 | `MyEnum(val) → member` | 按值查找成员 |
| 按名查找 | `MyEnum["NAME"] → member` | 按名称查找成员 |
| 迭代 | `for m in MyEnum` | 按定义顺序迭代所有成员 |

## 详细语义

### 声明枚举

```ms
import enum

class Color extends enum.Enum {
    RED   = 1
    GREEN = 2
    BLUE  = 3
}
```

类体中的每个赋值定义一个枚举成员。成员名称必须是合法标识符且在类内唯一。

### EnumMember 属性

每个枚举成员是 `EnumMember` 实例，具有以下只读属性：

| 属性 | 类型 | 说明 |
|---|---|---|
| `.name` | `str` | 成员名称（如 `"RED"`） |
| `.value` | `any` | 声明时赋予的值 |

`str(Color.RED)` → `"Color.RED"`
`repr(Color.RED)` → `"<Color.RED: 1>"`

### 成员查找

**按值查找**（调用枚举类）：

```ms
Color(1)     // → Color.RED
Color(99)    // → ValueError: 99 is not a valid Color
```

**按名称查找**（下标语法）：

```ms
Color["RED"]    // → Color.RED
Color["NONE"]   // → KeyError: "NONE"
```

### 相等与身份

枚举成员是单例：同一成员的每次访问返回相同对象。`Enum.__eq__` 基于对象身份（identity），而不按值比较，因此 `Color.RED is Color.RED` 与 `Color.RED == Color.RED` 均为 `true`，而 `Color.RED == 1` 为 `false`。

```ms
Color.RED == Color.RED   // true（同一单例对象）
Color.RED is Color.RED   // true（身份相同）
Color.RED == 1           // false（Enum 成员不等于其 .value；IntEnum 除外）
```

如需与原始值比较，请使用 `.value`：

```ms
Color.RED.value == 1     // true
```

### 迭代

`for m in Color` 按成员**定义顺序**迭代所有成员：

```ms
for m in Color {
    fmt.println(m.name, m.value)
}
// RED 1
// GREEN 2
// BLUE 3
```

### enum.IntEnum

继承 `enum.IntEnum` 时，成员值必须为整数，且成员与 `int` 可直接比较：

```ms
class Permission extends enum.IntEnum {
    READ    = 4
    WRITE   = 2
    EXECUTE = 1
}

fmt.println(Permission.READ == 4)    // true
fmt.println(Permission.READ > 2)     // true
flags := Permission.READ | Permission.WRITE   // 位运算（值为 int）
```

`IntEnum` 适用于位掩码、系统调用常量等需要与 int 互操作的场景。

### enum.auto()

在成员声明中使用 `enum.auto()` 自动分配递增整数（从 1 开始）：

```ms
class Direction extends enum.Enum {
    NORTH = enum.auto()  // 1
    SOUTH = enum.auto()  // 2
    EAST  = enum.auto()  // 3
    WEST  = enum.auto()  // 4
}
```

`auto()` 的具体值不应被程序逻辑依赖，仅用于区分成员。

### enum.unique

mslang 无 `@decorator` 语法，使用函数调用形式应用 `unique`：

```ms
class Status extends enum.Enum {
    ACTIVE   = 1
    INACTIVE = 2
}
Status = enum.unique(Status)   // 若有重复值则抛 ValueError
```

`enum.unique` 原地验证并返回同一类对象；通常紧接类声明后调用。

### 可哈希性

所有枚举成员均可哈希，可用作 map 键：

```ms
labels := {
    Color.RED:   "红色",
    Color.GREEN: "绿色",
    Color.BLUE:  "蓝色",
}
fmt.println(labels[Color.RED])   // "红色"
```

## 示例

```ms
import enum
import fmt

// 基础枚举
class Planet extends enum.Enum {
    MERCURY = 1
    VENUS   = 2
    EARTH   = 3
    MARS    = 4
}

fmt.println(Planet.EARTH)           // Planet.EARTH
fmt.println(Planet.EARTH.name)      // "EARTH"
fmt.println(Planet.EARTH.value)     // 3
fmt.println(repr(Planet.MARS))      // "<Planet.MARS: 4>"

// 按值查找
p := Planet(3)
fmt.println(p == Planet.EARTH)      // true

// 迭代
for planet in Planet {
    fmt.println(planet.name, planet.value)
}

// auto() 枚举
class Weekday extends enum.Enum {
    MON = enum.auto()
    TUE = enum.auto()
    WED = enum.auto()
    THU = enum.auto()
    FRI = enum.auto()
    SAT = enum.auto()
    SUN = enum.auto()
}
fmt.println(Weekday.FRI.value)      // 5

// IntEnum 与位掩码
class Flag extends enum.IntEnum {
    READ    = 4
    WRITE   = 2
    EXECUTE = 1
}
mask := Flag.READ | Flag.WRITE
fmt.println(mask)                   // 6（int 值）
fmt.println(mask & Flag.READ == Flag.READ)  // true

// unique 防止重复值
class Coin extends enum.Enum {
    PENNY   = 1
    CENT    = 1  // 与 PENNY 重复
}
Coin = enum.unique(Coin)   // 抛出 ValueError: duplicate values in Coin: CENT -> PENNY
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `MyEnum(val)` 值不存在于枚举中 |
| `ValueError` | `enum.unique(cls)` 检测到重复的成员值 |
| `KeyError` | `MyEnum["NAME"]` 名称不存在于枚举中 |
| `TypeError` | `IntEnum` 成员赋予非整数值 |
