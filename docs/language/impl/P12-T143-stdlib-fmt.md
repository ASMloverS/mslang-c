# P12-T143 stdlib: fmt

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `fmt` 模块：类 Go 风格的格式化 I/O 函数（`sprintf`/`printf`/`fprintf`/`sscanf`），以及 f-string（`$"..."`）运行期支持的底层格式化引擎。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj |
| P8-T102 | format() 内置函数（__format__ 协议） |

---

## API 清单

```ms
// 对齐 stdlib/fmt.md（Go-like 风格）
fmt.sprintf(format, *args) → str      // 格式化为字符串
fmt.printf(format, *args)             // 输出到 stdout
fmt.fprintf(file, format, *args)      // 输出到文件
fmt.errorf(format, *args) → Error     // 格式化错误消息
fmt.sscanf(s, format) → list          // 从字符串扫描

// 格式指示符（扩展 Python 格式规格字符串）
// %v  → repr(value)（通用格式）
// %s  → str(value)
// %d  → int（10进制）
// %x  → hex（小写）  %X → 大写
// %o  → oct   %b → bin
// %f  → float（固定小数）  %.2f
// %e  → 科学计数法  %g → 自动选择
// %t  → bool（"true"/"false"）
// %p  → 指针/id
// %%  → 字面 "%"
// %5d → 宽度 5  %-5d → 左对齐  %05d → 零填充

// 字符串插值（f-string 底层）
fmt.interpolate(parts) → str   // 内部 API，f-string 编译为此调用
```

---

## 实现要点

```c
// sprintf 核心实现：
// 扫描格式字符串，遇到 % 处理格式指示符，其余直接追加到 Builder
// 每个格式指示符对应一个 args[argIdx++]

static MsValue fmtSprintf(MsThread* t, MsValue* args, int argc) {
  if (argc < 1) return msRaiseTypeError(t, "sprintf() requires format string");
  MsStrObj* fmt = (MsStrObj*)MS_AS_OBJ(args[0]);
  MsWriter buf = {0};
  int argIdx = 1;
  // 解析格式字符串...
  return msNewStr((char*)buf.data, buf.len);
}

// f-string `$"hello {name}"` 编译为：
//   OP_CONST "hello "
//   [name 表达式]
//   OP_FORMAT (调用 __format__(spec))
//   [concat all parts]
//   → 实际上编译为 fmt.interpolate 调用或直接内联在编译器中
```

---

## 验收标准（checklist）

- [ ] `fmt.sprintf("%d + %d = %d", 1, 2, 3)` → `"1 + 2 = 3"`。
- [ ] `fmt.sprintf("%.2f", 3.14159)` → `"3.14"`。
- [ ] `fmt.sprintf("%-10s|", "hi")` → `"hi        |"`（左对齐）。
- [ ] `fmt.sprintf("%05x", 255)` → `"000ff"`。
- [ ] `fmt.printf` 输出到 stdout（与 print 效果相似但格式化）。
- [ ] `fmt.sscanf("42 hello", "%d %s")` → `[42, "hello"]`。

---

## 测试用例（.ms）

```ms
import fmt

print(fmt.sprintf("%d items cost $%.2f", 3, 7.5))
// 3 items cost $7.50

print(fmt.sprintf("%08b", 42))   // 00101010（8位二进制）
print(fmt.sprintf("%-10s!", "hello"))  // hello     !
print(fmt.sprintf("%+d %+d", 42, -42))  // +42 -42

// f-string（语言层面，不需要 import fmt）
name := "world"
print($"Hello, {name}!")   // Hello, world!
pi := 3.14159
print($"pi ≈ {pi:.2f}")    // pi ≈ 3.14
```

---

## Benchmark

```ms
import fmt, time
n := 1_000_000
t0 := time.now()
for i in range(n) { fmt.sprintf("item %d", i) }
t1 := time.now()
print("1M sprintf:", t1-t0, "ms")  // 目标 < 1s
```
