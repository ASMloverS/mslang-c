# sys — 解释器与运行时接口

```ms
import sys
```

## 概述

提供对 mslang 解释器内部状态的访问，包括运行时版本信息、命令行参数、
模块搜索路径、已加载模块缓存、标准流引用，以及若干运行时配置函数。

## 常量与类型

**版本与平台**

| 名称 | 类型 | 说明 |
|---|---|---|
| `sys.version` | `str` | mslang 版本字符串，如 `"1.0.0"` |
| `sys.versionInfo` | `tuple` | `(major, minor, patch)` 整数三元组 |
| `sys.platform` | `str` | 运行平台：`"windows"`、`"linux"` 或 `"darwin"` |

**参数与路径**

| 名称 | 类型 | 说明 |
|---|---|---|
| `sys.argv` | `list[str]` | 命令行参数列表（与 `os.args` 相同） |
| `sys.path` | `list[str]` | 模块搜索路径（源自 `MSLANG_PATH` 环境变量） |
| `sys.modules` | `map[str, module]` | 已加载模块缓存，键为模块名 |

**标准流**

| 名称 | 类型 | 说明 |
|---|---|---|
| `sys.stdin` | `File` | 标准输入（与 `io.stdin` 相同） |
| `sys.stdout` | `File` | 标准输出（与 `io.stdout` 相同） |
| `sys.stderr` | `File` | 标准错误（与 `io.stderr` 相同） |

**数值信息**

| 名称 | 类型 | 说明 |
|---|---|---|
| `sys.byteorder` | `str` | 字节序：`"little"` 或 `"big"` |
| `sys.maxsize` | `int` | int64 最大值：`9223372036854775807` |
| `sys.floatInfo` | `struct` | 浮点数精度信息（见下） |
| `sys.intInfo` | `struct` | 整数内部信息（见下） |

**sys.floatInfo 字段**

| 字段 | 说明 |
|---|---|
| `epsilon` | 最小正浮点数，使 `1.0 + epsilon != 1.0`（约 2.2e-16） |
| `max` | 最大有限浮点数 |
| `min` | 最小正规化浮点数 |
| `dig` | 十进制有效数字位数（15） |
| `mantDig` | 尾数二进制位数（53） |

**sys.intInfo 字段**

| 字段 | 说明 |
|---|---|
| `bitsPerDigit` | 每个内部数字的位数 |
| `sizeofDigit` | 内部数字的字节大小 |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `exit` | `sys.exit(code=0)` | 终止进程（抛 `SystemExit`，可被捕获） |
| `getframe` | `sys.getframe(depth=0) → Frame` | 获取调用帧对象（用于调试） |
| `getrecursionlimit` | `sys.getrecursionlimit() → int` | 获取最大递归深度 |
| `setrecursionlimit` | `sys.setrecursionlimit(n)` | 设置最大递归深度 |
| `getsizeof` | `sys.getsizeof(obj) → int` | 对象占用内存的近似字节数 |
| `intern` | `sys.intern(s) → str` | 字符串驻留（intern）去重 |

## 详细语义

### sys.exit

```
sys.exit(code=0)
```

通过抛出 `SystemExit(code)` 异常终止解释器。与 `os.exit()` 不同，
`SystemExit` 可被 `try/except` 捕获，从而在退出前执行清理逻辑；
`with` 语句的 `__exit__` 也会被正常调用。

`code` 为整数时，作为进程退出码；为字符串时，输出到 `stderr` 后以退出码 1 退出。

---

### sys.getframe

```
sys.getframe(depth=0) → Frame
```

返回调用栈中指定深度的帧对象。`depth=0` 为调用 `getframe` 的函数自身；
`depth=1` 为其调用者，以此类推。主要用于调试、日志和元编程。

**Frame 对象常用属性：**

| 属性 | 说明 |
|---|---|
| `fCode.coFilename` | 源文件路径 |
| `fCode.coName` | 函数名 |
| `fLineno` | 当前行号 |
| `fLocals` | 局部变量字典 |
| `fGlobals` | 全局变量字典 |

`depth` 超过调用栈深度时抛 `ValueError`。

---

### sys.path

模块导入时，解释器按 `sys.path` 中的目录顺序搜索模块文件。
可在运行时追加目录以扩展搜索范围：

```ms
sys.path.append("/opt/mylibs")
import mymodule  // 从 /opt/mylibs 加载
```

---

### sys.modules

已加载模块的缓存映射。`import foo` 时，若 `"foo"` 已在 `sys.modules` 中，
直接返回缓存对象而不重新执行模块文件。
可手动删除键以强制重新加载：

```ms
delete sys.modules["mymod"]
import mymod  // 重新加载
```

---

### sys.intern

```
sys.intern(s) → str
```

将字符串 `s` 驻留（intern）到全局字符串池，返回驻留后的字符串对象。
驻留后的字符串可用 `is` 进行 O(1) 身份比较（而非逐字符比较）。
适用于大量重复字符串的场景（如词法分析器的标识符表）。

---

### sys.getsizeof

```
sys.getsizeof(obj) → int
```

返回对象直接占用的内存字节数（近似值），不递归统计子对象。
结果与具体实现相关，仅供性能分析参考，不应依赖其精确值。

## 示例

```ms
import sys

// 1. 版本检查
if sys.versionInfo[0] < 1 {
    sys.stderr.write("需要 mslang 1.0+\n")
    sys.exit(1)
}

// 2. 命令行参数
if len(sys.argv) < 2 {
    fmt.println($"用法：{sys.argv[0]} <输入文件>")
    sys.exit(1)
}
inputPath := sys.argv[1]

// 3. 动态扩展模块路径
sys.path.append("./vendor")

// 4. 获取调用帧（调试日志）
func logCaller(msg) {
    frame := sys.getframe(1)
    fmt.println($"[{frame.fCode.coFilename}:{frame.fLineno}] {msg}")
}

// 5. 控制递归深度
sys.setrecursionlimit(2000)

// 6. 内存分析
x := [1, 2, 3, 4, 5]
fmt.println($"list 对象大小：{sys.getsizeof(x)} 字节")

// 7. SystemExit 可被捕获以做清理
try {
    sys.exit(0)
} except SystemExit as e {
    fmt.println($"退出码：{e.code}")
    // 执行清理后再次退出，或继续运行
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `SystemExit` | `sys.exit()` 调用时抛出，可被捕获 |
| `ValueError` | `sys.getframe(depth)` 的 `depth` 超过调用栈深度；`setrecursionlimit` 传入过小的值 |
| `TypeError` | `sys.intern` 传入非字符串对象 |
| `RecursionError` | 调用栈深度超过 `sys.getrecursionlimit()` 设定值 |
