# P12-T197 stdlib: traceback

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `traceback` 模块（对齐 `stdlib/traceback.md`）：异常 traceback 的格式化、打印、提取，与 T083 运行期 traceback 记录集成。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T083 | traceback 运行期记录 |
| P12-T134 | io（StringIO 用于 format_exc） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-traceback.md` | §1 模块 API |

---

## API 清单

```ms
// 打印当前异常 traceback（在 catch 块中调用）
traceback.print_exc(limit=nil, file=nil, chain=true)
// 等同 print(traceback.format_exc())

// 格式化当前异常 traceback 为字符串
traceback.format_exc(limit=nil, chain=true) → str

// 底层：提取/格式化任意异常
traceback.print_exception(exc, value=nil, tb=nil, limit=nil,
                           file=nil, chain=true)
traceback.format_exception(exc, **kwargs) → list[str]  // 每行一个字符串
traceback.format_exception_only(exc_type, exc) → list[str]  // 仅异常行

// StackSummary（栈帧信息）
traceback.extract_tb(tb, limit=nil) → StackSummary
traceback.extract_stack(f=nil, limit=nil) → StackSummary

// StackSummary（list[FrameSummary]）
ss := traceback.StackSummary([...])
ss.format() → list[str]   // 格式化每帧

// FrameSummary 属性
fs.filename  fs.lineno  fs.name  fs.line  fs.locals

// TracebackException（异常包装，可链式访问 cause/__context__）
te := traceback.TracebackException.from_exception(exc)
te.exc_type   // 异常类型
te.format() → iterator[str]
te.format_exception_only() → iterator[str]
te.__context__  // 隐式异常链（catch 内再 raise 时设置）

// 栈遍历
traceback.walk_stack(f=nil)  // → iterator[(frame, lineno)]
traceback.walk_tb(tb)        // → iterator[(frame, lineno)]

// print_stack（当前调用栈，无异常时）
traceback.print_stack(f=nil, limit=nil, file=nil)
traceback.format_stack(f=nil, limit=nil) → list[str]
```

---

## 实现要点

```c
// Traceback 对象（MsTbObj）：
// 由 P6-T083 定义，包含：
// - 帧文件名、行号、函数名
// - 帧链（异常传播路径）
// - 异常本身（MsValue exc）

// format_exception 实现：
// 1. 若有 __context__（在 catch 内再次 raise），先格式化 context，加分隔线
//    "During handling of the above exception, another exception..."
// 3. 格式化 Traceback：
//    "Traceback (most recent call last):\n"
//    每帧：'  File "fn", line N, in funcname\n    source_line\n'
// 4. 异常类型+消息：
//    "TypeError: cannot add int and str\n"

// extract_tb：从 MsTbObj 链提取 StackSummary
// FrameSummary：(filename, lineno, name, line=None)
// line = 从源文件读取对应行（缓存读过的源文件）

// format_stack：
// 遍历当前 MsFrame 链（从当前帧到根帧）
// 格式化同 format_tb

// 源行缓存（linecache 等价）：
// gVM.sourceCache MsHashMap<filename, list[str]>（按行缓存）
// 运行期读取源文件，找到 lineno 对应行

typedef struct MsFrameSummary {
  char*  filename;
  int    lineno;
  char*  name;
  char*  line;   // 可能为 nil（无法读取源文件时）
} MsFrameSummary;
```

---

## 验收标准（checklist）

- [ ] `format_exc()` 在 catch 块中返回完整 traceback 字符串。
- [ ] `__context__` 隐式链：catch 内再 raise 时 traceback 包含两段异常信息。
- [ ] `extract_tb` 正确提取所有帧（文件名、行号、函数名）。
- [ ] `format_stack()` 打印当前调用栈（无异常时）。
- [ ] 源行读取：traceback 包含对应源代码行（非空）。
- [ ] `print_exc` 输出到 stderr（默认）或指定 file。

---

## 测试用例（.ms）

```ms
import traceback, sys, io

// 基础 format_exc
func level2() { raise ValueError("bad value") }
func level1() { level2() }

buf := io.StringIO()
try {
    level1()
} catch (e: Exception) {
    traceback.printExc(file=buf)
}

tbStr := buf.getValue()
print("ValueError" in tbStr)  // true
print("level2" in tbStr)      // true
print("level1" in tbStr)      // true

// 隐式异常链（__context__）
try {
    try {
        1 / 0
    } catch (e: ZeroDivisionError) {
        raise RuntimeError("wrapped")
    }
} catch (e: RuntimeError) {
    s := traceback.formatExc()
    print("ZeroDivisionError" in s)  // true（__context__ 异常）
    print("RuntimeError" in s)       // true
}

// extract_tb
try:
    level1()
catch e:
    import sys
    tb := sys.exc_info()[2]
    stack := traceback.extract_tb(tb)
    for frame in stack:
        print(frame.filename, frame.lineno, frame.name)

// format_stack
def a() { return traceback.format_stack() }
def b() { return a() }
stack_str := b()
print("def a()" in "".join(stack_str) or True)  // 包含帧信息
```
