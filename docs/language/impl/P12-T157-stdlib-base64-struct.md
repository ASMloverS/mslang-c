# P12-T157 stdlib: base64 / struct

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `base64`（二进制→文本编码，对齐 `stdlib/base64.md`）和 `struct`（二进制数据结构打包/解包）模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T058 | MsBytesObj |
| P4-T057 | MsStrObj |

---

## API 清单

```ms
// base64
base64.b64encode(data) → bytes     // 标准 Base64 编码（含 = 填充）
base64.b64decode(s) → bytes        // 解码（容忍空白）
base64.urlsafe_b64encode(data) → bytes  // URL 安全（- 替代 + / 替代 /）
base64.urlsafe_b64decode(s) → bytes
base64.b32encode(data) → bytes
base64.b32decode(s) → bytes
base64.b16encode(data) → bytes     // hex 大写
base64.b16decode(s) → bytes
base64.encodebytes(data) → bytes   // 每 76 字节插入换行（MIME）
base64.decodebytes(s) → bytes      // 忽略换行

// struct
struct.pack(format, *v) → bytes    // 按格式打包为字节
struct.unpack(format, buffer) → tuple  // 从字节解包
struct.pack_into(format, buffer, offset, *v)  // 写入 bytearray
struct.unpack_from(format, buffer, offset=0)  // 从 offset 解包
struct.calcsize(format) → int      // 格式字节大小
struct.iter_unpack(format, buffer) // 重复解包，返回迭代器

// 格式字符：
// x 填充  c 单字节  b/B 有/无符号字节
// h/H 短整 i/I 整型  l/L 长整  q/Q 64位
// f 单精度  d 双精度  s 字节串  p pascal 串  ? bool
// 字节序前缀：< 小端  > 大端  ! 网络  = 本机  @ 本机对齐

struct.Struct(format)   // 预编译格式对象（.pack/.unpack/.size）
```

---

## 实现要点

```c
// base64 编码表：固定查表
// b64encode：每 3 字节 → 4 个 base64 字符，末尾 = 填充
// b64decode：每 4 个字符 → 3 字节，忽略 = 和空白
// urlsafe：字母表中 + → - / → _

// struct 格式解析：
// 1. 首字符确定字节序（<>!=@）
// 2. 遍历格式字符，每个字符对应 C 类型 + 大小
// 3. pack：逐个提取 MsValue，按字节序写入 bytes
// 4. unpack：逐个读取字节，转换为 MsValue 组成 tuple

// 字节序转换：bswap16/32/64（或 htons/ntohs 等）
// struct.Struct 预计算 size，避免重复解析

struct FmtEntry {
  char    code;      // 格式字符
  uint8_t size;      // 字节大小
  bool    signed_;   // 有符号
  bool    isFloat;
};

// 对齐处理（@ 前缀时）：插入填充字节使对齐到 size
```

---

## 验收标准（checklist）

- [ ] `base64.b64encode(b"hello")` → `b"aGVsbG8="`。
- [ ] `base64.b64decode("aGVsbG8=")` → `b"hello"`。
- [ ] `struct.pack(">ih", 1, 2)` → 6 字节（4+2，大端）。
- [ ] `struct.unpack(">ih", b"\x00\x00\x00\x01\x00\x02")` → `(1, 2)`。
- [ ] `struct.calcsize("4sI")` → `8`（4 字节字符串 + 4 字节 uint）。
- [ ] `base64.urlsafe_b64encode` 输出不含 `+` 或 `/`。

---

## 测试用例（.ms）

```ms
import base64, struct

// base64 round-trip
original := b"Hello, World! \x00\xff"
encoded := base64.b64encode(original)
print(encoded)
decoded := base64.b64decode(encoded)
print(decoded == original)  // true

// URL-safe
url_enc := base64.urlsafe_b64encode(b"\xfb\xff")
print(url_enc)  // b"-_8=" (无 + 或 /)

// struct 打包网络包头
header := struct.pack("!HHI", 1234, 5678, 0xdeadbeef)
print(len(header))  // 8

t := struct.unpack("!HHI", header)
print(t)  // (1234, 5678, 3735928559)

// struct.Struct 预编译
fmt := struct.Struct("<3f")
data := fmt.pack(1.0, 2.0, 3.0)
print(fmt.unpack(data))  // (1.0, 2.0, 3.0)
print(fmt.size)          // 12

// iter_unpack
buf := struct.pack("3I", 1, 2, 3) + struct.pack("3I", 4, 5, 6)
for t in struct.iter_unpack("3I", buf) {
    print(t)
}
// (1,2,3)
// (4,5,6)
```
