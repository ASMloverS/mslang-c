# mslang 语言概述

## 定位与设计哲学

mslang 是一门**动态类型脚本语言**，以 `.ms` 为脚本文件后缀。

设计目标：
- **语法层面**贴近 Go：花括号块、`func`/`class` 关键字、`:=` 短变量声明、自动分号插入、`go`+channel 并发原语、`async`/`await`。
- **语义层面**贴近 Python：动态类型、`class` 继承与魔术方法、`try/catch/raise` 异常、Python 风格内置函数与标准库（`print`/`len`/`range`）。
- **实现层面**以纯 **C17** 为宿主，提供字节码虚拟机、分代并发追踪式 GC，以及类似 Python C API 的嵌入/扩展接口。

### 核心设计决策一览

| 维度 | 决策 | 说明 |
|---|---|---|
| 宿主语言 | 纯 C17 | 可移植、易做 C API |
| 类型系统 | 动态类型 | 运行期检查，变量无类型声明 |
| 语法风格 | Go 式花括号 | `func`/`class`/`{}`；自动分号插入 |
| 执行模型 | 字节码 VM | 编译到栈式字节码，CPython 风格 |
| 整数模型 | 固定 int64 | 溢出回绕；无 bigint；**`/` 对 int/int 为整除**（非真除，**与 Python 3 不同**：Python `1/2=0.5`，mslang `1/2=0`），含 float 操作数则真除 |
| 浮点模型 | float64（IEEE 754） | |
| 字符串模型 | UTF-8 字节序列 | 索引按字节，迭代按 Unicode 码点 |
| OOP | Python 式 class | 继承、魔术方法、动态属性 |
| 错误处理 | 异常 try/catch/raise | 异常对象有层次结构 |
| 并发 | 统一调度器 | `go`+channel 绿色线程 + `async func`/`await` 共享同一协程调度器 |
| 内存管理 | 追踪式分代 GC | 精确标记-清除 → 分代 → 年轻代半区复制 → 并发/并行 |
| C API | 句柄/本地根表 | 与移动式 GC 兼容，类似 V8/JNI |
| 标准库 | Python 风格为主 | 融合 Go std 与 Python std；`print` 为内置便捷函数，`fmt.*` 提供格式化/文件输出，两者并存（见 stdlib.md §1/§2.1）；网络模块扁平化（import http / url / socket / net） |
| 模块系统 | 文件/目录路径解析 | `import .utils` 或 `import pkg.sub` |
| 构建系统 | CMake 跨平台 | Windows / Linux / macOS |

---

## 快速示例

```ms
// 变量
x := 42
name := "world"

// 函数
func add(a, b) {
    return a + b
}

// 类
class Animal {
    func __init__(self, name) {
        self.name = name
    }
    func speak(self) {
        print("...")
    }
}

class Dog extends Animal {
    func speak(self) {
        print(self.name + " says: Woof!")
    }
}

// 异常
try {
    x := 1 / 0
} catch (e) {
    print("caught:", e)
}

// 并发（go + channel）
ch := make(chan, 1)
go func() {
    ch <- "hello"
}()
msg := <-ch
print(msg)

// async/await
import http

async func fetchData(url) {
    resp := await http.get(url)
    return resp.body
}

async func main() {
    data := await fetchData("http://example.com")
    print(data)
}
```

---

## 文档结构

| 文件 | 内容 |
|---|---|
| `syntax.md` | 词法规则、文法规范、运算符优先级 |
| `type-system.md` | 对象模型、内置类型、class 系统、魔术方法 |
| `vm.md` | 字节码指令集、MsChunk、调用约定、闭包 |
| `gc.md` | 分代 GC 设计、精确根、写屏障、安全点 |
| `concurrency.md` | 协程调度器、goroutine、channel、async/await |
| `errors.md` | 异常体系、try/catch 语义、VM 展开机制 |
| `execution.md` | CLI 执行模式、`__mscache__` 字节码缓存、`.msc` 格式 |
| `modules.md` | import 解析、模块缓存、循环依赖检测 |
| `stdlib.md` | 内置函数参考与模块索引（详见 stdlib/ 目录） |
| `c-api.md` | 嵌入 API、扩展模块 API、句柄/根表 |
| `c-style.md` | C 编码规范（命名/缩进/注释/文件组织等） |
| `ms-style.md` | .ms 脚本编码规范（命名/缩进/import/空行等） |
