# P8-T102 内置函数：chr / ord / hex / oct / bin / format

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现字符和格式化内置函数：`chr`（码点→字符）、`ord`（字符→码点）、`hex`/`oct`/`bin`（整数进制格式化）、`format`（格式化字符串，对接 `__format__`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj（UTF-8） |
| P4-T053 | int 类型 |

---

## 实现要点

### 1. `chr(i)` / `ord(c)`

```c
// chr(65) → "A"；chr(0x4e2d) → "中"
static MsValue builtinChr(MsThread* t, MsValue* args, int argc) {
  if (argc != 1 || !MS_IS_INT(args[0]))
    return msRaiseTypeError(t, "chr() argument must be int");
  int64_t cp = MS_AS_INT(args[0]);
  if (cp < 0 || cp > 0x10FFFF)
    return msRaiseValueError(t, "chr() arg not in range(0x110000)");
  // 编码为 UTF-8
  char buf[5]; int len = 0;
  if (cp < 0x80)       { buf[len++] = (char)cp; }
  else if (cp < 0x800) { buf[len++] = 0xC0|(cp>>6); buf[len++] = 0x80|(cp&0x3F); }
  else if (cp < 0x10000){ buf[len++] = 0xE0|(cp>>12); buf[len++] = 0x80|((cp>>6)&0x3F); buf[len++] = 0x80|(cp&0x3F); }
  else { buf[len++] = 0xF0|(cp>>18); buf[len++] = 0x80|((cp>>12)&0x3F); buf[len++] = 0x80|((cp>>6)&0x3F); buf[len++] = 0x80|(cp&0x3F); }
  buf[len] = '\0';
  return msNewStr(buf, len);
}

// ord("A") → 65；ord("中") → 20013
static MsValue builtinOrd(MsThread* t, MsValue* args, int argc) {
  if (argc != 1 || !MS_IS_OBJ(args[0]) || MS_AS_OBJ(args[0])->type != &msStrType)
    return msRaiseTypeError(t, "ord() requires a string of length 1");
  MsStrObj* s = (MsStrObj*)MS_AS_OBJ(args[0]);
  // 必须是单码点字符串
  if (strCpLen(s) != 1)
    return msRaiseTypeError(t, "ord() expected a character, but got a string of length != 1");
  // 解码第一个码点
  uint32_t cp = 0;
  msUTF8Decode(s->data, &cp);
  return MS_INT_VAL((int64_t)cp);
}
```

### 2. `hex` / `oct` / `bin`

```c
static MsValue builtinHex(MsThread* t, MsValue* args, int argc) {
  if (argc != 1 || !MS_IS_INT(args[0]))
    return msRaiseTypeError(t, "hex() argument must be int");
  int64_t v = MS_AS_INT(args[0]);
  char buf[32];
  if (v < 0) snprintf(buf, sizeof(buf), "-0x%" PRIx64, (uint64_t)(-v));
  else       snprintf(buf, sizeof(buf), "0x%" PRIx64, (uint64_t)v);
  return msNewStr(buf, strlen(buf));
}

// oct → "0o..."；bin → "0b..."（类似实现）
```

### 3. `format(value, format_spec="")`

```c
// format(3.14, ".2f") → "3.14"
// format(42, "08x") → "0000002a"
// 调用 value.__format__(format_spec)
static MsValue builtinFormat(MsThread* t, MsValue* args, int argc) {
  if (argc < 1) return msRaiseTypeError(t, "format() requires at least 1 argument");
  MsValue spec = (argc >= 2) ? args[1] : msNewStrIntern("", 0);

  // 调用 __format__ dunder
  MsValue result = msCallDunder(t, args[0], "__format__", &spec, 1);
  if (!MS_IS_NIL(result)) return result;

  // 默认 fallback：repr
  return msValueRepr(args[0]);
}
```

### 4. 内置 `__format__` 实现（int/float/str）

```c
// int.__format__(spec)：支持 d/b/o/x/X/n，宽度，填充
// float.__format__(spec)：支持 f/e/g/E/G，精度
// str.__format__(spec)：支持对齐 < > ^，填充字符，宽度
// 实现为各类型的 tpGetattr 中手工绑定的方法
```

---

## 验收标准（checklist）

- [ ] `chr(65)` → `"A"`；`chr(0x1F600)` → `"😀"`（UTF-8 4字节）。
- [ ] `ord("A")` → `65`；`ord("中")` → `20013`。
- [ ] `ord("ab")` → `TypeError`（长度不为 1）。
- [ ] `hex(255)` → `"0xff"`；`hex(-1)` → `"-0x1"`。
- [ ] `oct(8)` → `"0o10"`；`bin(10)` → `"0b1010"`。
- [ ] `format(3.14, ".2f")` → `"3.14"`。
- [ ] `format(42, "08x")` → `"0000002a"`。

---

## 测试用例（.ms）

```ms
// chr / ord
print(chr(65))      // A
print(chr(0x1F600)) // 😀
print(ord("A"))     // 65
print(ord("😀"))    // 128512

try { ord("ab") } catch TypeError as e { print("err") }  // err

// hex / oct / bin
print(hex(255))   // 0xff
print(hex(-16))   // -0x10
print(oct(8))     // 0o10
print(bin(10))    // 0b1010

// format
print(format(42, "d"))    // 42
print(format(42, "08b"))  // 00101010
print(format(3.14, ".2f"))  // 3.14
print(format("hello", ">10"))  //      hello
```

---

## Benchmark

N/A。

---

## 风险与边界

- **`format` 规格字符串**：完整的格式迷你语言（Python 风格）较复杂；初版实现 `d/b/o/x/X/f/e/g`、宽度、填充、对齐；高级特性（`n`/`%`/`_`分组）留待 stdlib `fmt` 模块补全。
