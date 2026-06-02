# array — 同类型元素的紧凑数组

```ms
import array
```

## 概述

与 `list` 不同，`array` 仅存储单一类型的值，元素以紧凑的二进制形式保存，
内存占用远小于等价的 `list`。适用于大量同类数值的存储、与 C 库交互，
以及需要直接操作原始字节的场景。

类型码在构造时确定，之后不可更改；写入不兼容类型的值会抛 `TypeError`。

## 常量与类型

### 类型码

| 类型码 | C 类型 | 大小（字节） | mslang 类型 |
|---|---|---|---|
| `'b'` | signed char | 1 | int |
| `'B'` | unsigned char | 1 | int |
| `'h'` | signed short | 2 | int |
| `'H'` | unsigned short | 2 | int |
| `'i'` | signed int | 4 | int |
| `'I'` | unsigned int | 4 | int |
| `'l'` | signed long | 8 | int |
| `'L'` | unsigned long | 8 | int |
| `'f'` | float | 4 | float |
| `'d'` | double | 8 | float |

整数类型码（`'b'`–`'L'`）仅接受整数值；浮点类型码（`'f'`、`'d'`）仅接受浮点值。
写入整数类型时，值须在对应 C 类型的范围内，否则抛 `OverflowError`。

## 函数签名速查

| 成员 | 签名 | 说明 |
|---|---|---|
| `array` | `array(typecode, initializer=nil)` | 构造数组 |
| `append` | `a.append(x)` | 追加单个元素 |
| `extend` | `a.extend(iter)` | 追加可迭代对象的所有元素 |
| `insert` | `a.insert(i, x)` | 在位置 i 插入元素 |
| `pop` | `a.pop(i=-1) → item` | 移除并返回位置 i 的元素 |
| `remove` | `a.remove(x)` | 移除第一个等于 x 的元素 |
| `index` | `a.index(x) → int` | 返回第一个等于 x 的下标 |
| `count` | `a.count(x) → int` | 返回 x 出现的次数 |
| `reverse` | `a.reverse()` | 原地反转 |
| `tobytes` | `a.tobytes() → bytes` | 返回数组的原始字节表示 |
| `frombytes` | `a.frombytes(b)` | 从字节序列追加数据 |
| `tolist` | `a.tolist() → list` | 返回等价的普通列表 |
| `fromlist` | `a.fromlist(lst)` | 从列表追加所有元素 |
| `buffer_info` | `a.buffer_info() → (address, length)` | 返回内存地址和元素个数 |

**属性**

| 属性 | 说明 |
|---|---|
| `typecode` | 只读，构造时指定的类型码字符串 |
| `itemsize` | 只读，每个元素占用的字节数 |

## 详细语义

### 构造

```
array.array(typecode, initializer=nil)
```

`typecode` 须为上表中的合法字符串。`initializer` 可以是：

- 可迭代对象（list、tuple 等）：逐一追加元素，类型须兼容。
- `bytes` 或 `bytearray`：以原始二进制数据初始化，长度须为 `itemsize` 的整数倍。
- `nil`：构造空数组。

### tobytes / frombytes

```
a.tobytes() → bytes
a.frombytes(b)
```

`tobytes` 返回数组的机器表示（字节序为平台原生字节序）。
`frombytes` 将字节序列解析为对应类型并追加；`b` 的长度须为 `itemsize` 的整数倍，
否则抛 `ValueError`。

### tolist / fromlist

```
a.tolist() → list
a.fromlist(lst)
```

`tolist` 将数组内容转换为普通 `list`，整数类型返回 int 列表，浮点类型返回 float 列表。
`fromlist` 等效于逐一调用 `append`，若列表中任意元素类型不兼容则整批操作回滚并抛 `TypeError`。

### buffer_info

```
a.buffer_info() → (address, length)
```

返回 `(address, length)` 元组：`address` 为底层缓冲区的内存地址（int），
`length` 为元素个数（非字节数）。主要用于与 C 扩展交互。

### 索引与切片

支持 `a[i]`、`a[i:j]`（返回同类型 array）、`a[i] = x`、`a[i:j] = other_array`。
切片赋值要求右侧 array 与左侧 `typecode` 相同。

支持 `len(a)`、`for x in a`、`x in a`、`a == b`（逐元素比较）。

## 示例

```ms
import array

// 存储传感器采样值（float64）
samples := array.array("d", [1.1, 2.2, 3.3, 4.4])
samples.append(5.5)
fmt.println(samples.tolist())   // [1.1, 2.2, 3.3, 4.4, 5.5]
fmt.println(samples.itemsize)   // 8

// 整数数组与字节互转
a := array.array("h", [1, 2, 3])     // signed short
raw := a.tobytes()
fmt.println(len(raw))                 // 6（3 × 2 字节）

b := array.array("h")
b.frombytes(raw)
fmt.println(b.tolist())  // [1, 2, 3]

// 紧凑存储 uint8 像素数据
pixels := array.array("B", [255, 128, 0, 64])
pixels[1] = 200
fmt.println(pixels[1])  // 200

// 与 list 互转
lst := pixels.tolist()
lst.append(32)
pixels.fromlist([32])
fmt.println(len(pixels))  // 5
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | 非法 `typecode`；`frombytes` 的字节长度不是 `itemsize` 的整数倍；`remove`/`index` 元素不存在 |
| `TypeError` | 写入与 `typecode` 不兼容的值类型；`fromlist` 列表包含不兼容元素 |
| `OverflowError` | 整数类型写入超出 C 类型范围的值 |
| `IndexError` | `pop`/`insert` 下标越界 |
