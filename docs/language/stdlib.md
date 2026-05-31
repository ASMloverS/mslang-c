# mslang 标准库参考

标准库风格以 **Python 为主**，融合 Go 标准库的强项模块。

## 1. 内置函数（无需 import）

| 函数 | 签名 | 说明 |
|---|---|---|
| `print` | `print(*args, sep=" ", end="\n")` | 输出到 stdout |
| `input` | `input(prompt="")` | 从 stdin 读取一行（去掉末尾 `\n`） |
| `len` | `len(x)` | 长度/元素数 |
| `type` | `type(x)` | 返回 x 的类型对象 |
| `isinstance` | `isinstance(x, T)` | 类型检查（含继承） |
| `str` | `str(x)` | 调用 `__str__`，转为字符串 |
| `int` | `int(x, base=10)` | 转为 int64（字符串解析或截断） |
| `float` | `float(x)` | 转为 float64 |
| `bool` | `bool(x)` | 调用 `__bool__` |
| `repr` | `repr(x)` | 调用 `__repr__`，返回可解析表示 |
| `range` | `range(stop)` / `range(start, stop[, step])` | 惰性整数范围迭代器 |
| `enumerate` | `enumerate(iter, start=0)` | `(index, value)` 迭代器 |
| `zip` | `zip(*iters)` | 并行迭代，最短截断 |
| `map` | `map(fn, iter)` | 惰性映射 |
| `filter` | `filter(fn, iter)` | 惰性过滤 |
| `sorted` | `sorted(iter, key=nil, reverse=false)` | 返回排序后的 list |
| `reversed` | `reversed(x)` | 反向迭代器 |
| `sum` | `sum(iter, start=0)` | 求和 |
| `min` | `min(*args)` / `min(iter, key=nil)` | 最小值 |
| `max` | `max(*args)` / `max(iter, key=nil)` | 最大值 |
| `abs` | `abs(x)` | 绝对值 |
| `round` | `round(x, ndigits=0)` | 四舍五入 |
| `hash` | `hash(x)` | 调用 `__hash__` |
| `id` | `id(x)` | 对象唯一 ID（地址或伪地址） |
| `callable` | `callable(x)` | 是否可调用 |
| `iter` | `iter(x)` | 调用 `__iter__` |
| `next` | `next(it[, default])` | 调用 `__next__`；无元素时返回 default 或抛 StopIteration |
| `chr` | `chr(code)` | 码点 → 单字符字符串 |
| `ord` | `ord(s)` | 单字符字符串 → 码点 |
| `hex` | `hex(n)` | int → `"0x..."` |
| `oct` | `oct(n)` | int → `"0o..."` |
| `bin` | `bin(n)` | int → `"0b..."` |
| `open` | `open(path, mode="r", encoding="utf-8")` | 打开文件，返回 File 对象 |

### 关键字/语句（非函数值）

以下具有内置行为，但**不是**可传递的函数值，不可用于 `map(make, ...)` 等高阶用法：

| 形式 | 说明 |
|---|---|
| `make(chan[, cap])` | 创建 channel；语言关键字，不可作为值传递（文法见 syntax §1.4 / §2.3 MakeExpr） |
| `assert cond [, "msg"]` | 断言语句；条件为假时抛 `AssertionError`（文法见 syntax §1.4 / §2.2） |
| `close(ch)` | 关闭 channel；语言关键字，不可作为值传递（见 concurrency.md §3.4） |

---

## 2. 标准库模块

> **命名规约**：Go 来源模块（`fmt`、`strings`、`sort`、`sync`、`time`、`regexp`、`path`）保留 Go 原名大驼峰风格（函数：`fmt.Println`、类型：`sync.Mutex`）；mslang 原生模块（`gc`、`sys`、`os`、`io`、`json`、`math`、`random`）采用蛇形/小写风格（`gc.collect_young`、`os.getcwd`）。全库以此为准，后续添加 API 须遵守对应模块的风格。

### 2.1 `fmt` — 格式化输出（偏 Go）

```ms
import fmt

fmt.Println("hello", "world")         // 空格分隔，换行
fmt.Printf("%d + %d = %d\n", 1, 2, 3) // 格式字符串（类 C/Go）
s := fmt.Sprintf("x=%v", x)           // 返回格式化字符串
fmt.Fprintf(file, "log: %s\n", msg)   // 写入 file 对象
fmt.Errorf("not found: %s", name)     // 创建 RuntimeError（返回异常对象，不 raise）
```

格式占位符（参考 Go fmt + Python %）：`%d`/`%i` 整数，`%f` 浮点，`%s` 字符串，`%v` 通用（调 `__repr__`），`%p` 指针，`%%` 字面 %，`%q` 带引号字符串。字符串内插请使用 `$"..."` f-string 语法（见 §3）。

### 2.2 `strings` — 字符串处理（偏 Go）

```ms
import strings

strings.contains(s, substr)          // bool
strings.hasPrefix(s, prefix)         // bool
strings.hasSuffix(s, suffix)         // bool
strings.index(s, substr)             // int（-1 若无）
strings.lastIndex(s, substr)
strings.count(s, substr)             // 出现次数
strings.replace(s, old, new, n=-1)  // n=-1 全部替换
strings.split(s, sep, maxsplit=-1)  // list of str
strings.splitlines(s)               // 按行分割
strings.join(sep, parts)            // sep.join(parts)
strings.strip(s, chars=" \t\n\r")  // 去首尾
strings.lstrip(s, chars)
strings.rstrip(s, chars)
strings.lower(s)
strings.upper(s)
strings.title(s)
strings.repeat(s, n)                // s * n
strings.trimPrefix(s, prefix)
strings.trimSuffix(s, suffix)
strings.fields(s)                   // 按空白拆分（忽略多余空白）
strings.format(tmpl, *args, **kw)   // 类 Python str.format
strings.Builder                     // 高效字符串拼接类
```

字符串方法也直接作为方法提供：`s.upper()` / `s.split(",")` 等（委托 `strings` 模块）。

### 2.3 `os` — 操作系统接口

```ms
import os

os.args                              // 命令行参数 list（含脚本名）
os.env                               // 环境变量 map
os.getenv(key, default=nil)
os.setenv(key, value)
os.exit(code=0)

os.getcwd()                          // 当前工作目录
os.chdir(path)
os.listdir(path=".")                 // list of filename
os.stat(path)                        // 返回 StatInfo{size, mtime, isDir, ...}
os.mkdir(path, mode=0o755)
os.makedirs(path, exist_ok=false)
os.remove(path)
os.rename(src, dst)
os.path.join(*parts)
os.path.exists(path)
os.path.isfile(path)
os.path.isdir(path)
os.path.basename(path)
os.path.dirname(path)
os.path.abspath(path)
os.path.splitext(path)               // (name, ext)
os.exec(cmd, *args, env=nil)        // 替换当前进程
```

### 2.4 `io` — I/O 抽象

```ms
import io

// File 对象（open() 返回）
f := open("data.txt", "r")
content := f.read()            // 读全部
line    := f.readline()        // 读一行
lines   := f.readlines()       // list of line
f.write("hello\n")
f.close()
// with 语句（规划）: with open("f") as f { ... }

// 异步 I/O
async func readAsync(path) {
    content := await io.readFile(path)
    return content
}
await io.writeFile(path, data)

io.stdin  // 标准输入 File
io.stdout // 标准输出 File
io.stderr // 标准错误 File

io.copy(dst, src)              // 流复制
io.pipe()                      // (reader, writer) 对
```

### 2.5 `math` — 数学函数

```ms
import math

math.pi          // 3.141592...
math.e           // 2.718281...
math.inf         // 正无穷
math.nan         // NaN

math.sqrt(x)     math.cbrt(x)
math.pow(x, y)   math.exp(x)    math.log(x[, base])
math.log2(x)     math.log10(x)
math.sin(x)      math.cos(x)    math.tan(x)
math.asin(x)     math.acos(x)   math.atan(x)   math.atan2(y, x)
math.sinh(x)     math.cosh(x)   math.tanh(x)
math.ceil(x)     math.floor(x)  math.trunc(x)
math.abs(x)      math.gcd(a, b) math.lcm(a, b)
math.isnan(x)    math.isinf(x)
math.hypot(*coords)
math.factorial(n)
math.comb(n, k)  math.perm(n, k)
math.degrees(r)  math.radians(d)
```

### 2.6 `time` — 时间与计时

```ms
import time

time.now()                           // 返回 Time 对象
time.sleep(seconds)                  // 阻塞当前 goroutine（非整个线程）
time.after(seconds)                  // 返回 channel，seconds 后发送当前时间

t := time.now()
t.unix()         // Unix 时间戳（int64 秒）
t.unixMilli()    // 毫秒
t.format(layout) // 格式化（Go layout 风格："2006-01-02 15:04:05"）
t.add(duration)  // Duration 相加

time.parse(layout, s)      // 解析字符串为 Time
time.Duration              // 类（nanoseconds 内部表示）
time.second                // = time.Duration(1e9)
time.minute                // = time.Duration(60e9)
```

### 2.7 `json` — JSON 编解码

```ms
import json

s := json.encode(obj)              // 序列化为 JSON 字符串
s := json.encodePretty(obj, indent=2)
obj := json.decode(s)              // 反序列化
json.encodeFile(path, obj)
obj := json.decodeFile(path)
```

支持类型：int、float、bool、nil、string、list、map（键必须为 string 或可转 string）。自定义类可实现 `__json__(self)` 返回可序列化对象。

### 2.8 `regexp` — 正则表达式（RE2 语法）

```ms
import regexp

re := regexp.compile(pattern)
re := regexp.mustCompile(pattern)   // 编译失败 panic

re.match(s)                         // bool（是否匹配）
re.find(s)                          // 第一个匹配子串（或 nil）
re.findAll(s, n=-1)                 // list of 匹配子串
re.groups(s)                        // list of (match, group1, group2, ...)
re.replace(s, repl)                 // 字符串替换（repl 可含 $1 反引用）
re.replaceFunc(s, fn)               // fn(match) → 替换字符串
re.split(s, n=-1)                   // 按正则分割
```

### 2.9 `sort` — 排序

```ms
import sort

sort.sort(lst)                      // 就地排序（需元素可比较）
sort.sort(lst, key=func(x){...})
sort.sort(lst, reverse=true)
sort.stable(lst, key=nil)           // 稳定排序
sorted_lst := sort.sorted(lst)      // 返回新 list（同内置 sorted）
sort.search(lst, target)            // 二分查找，返回插入点
```

### 2.10 `random` — 随机数

```ms
import random

random.seed(n)
random.random()                     // [0.0, 1.0)
random.randint(a, b)                // [a, b] 整数
random.choice(lst)                  // 随机元素
random.shuffle(lst)                 // 就地打乱
random.sample(lst, k)              // 无放回采样
random.uniform(a, b)               // [a, b] 浮点
random.gauss(mu, sigma)            // 正态分布
```

### 2.11 `sync` — 同步原语

```ms
import sync

mu := sync.Mutex()
mu.lock()
mu.unlock()
mu.tryLock()                        // bool

rw := sync.RWMutex()
rw.rLock() / rw.rUnlock()
rw.lock()  / rw.unlock()

wg := sync.WaitGroup()
wg.add(n)
wg.done()
wg.wait()                           // 阻塞直到计数归零

once := sync.Once()
once.do(func() { ... })            // 只执行一次

// 原子操作
sync.atomic.load(ref)
sync.atomic.store(ref, val)
sync.atomic.add(ref, delta)        // 返回旧值
sync.atomic.compareSwap(ref, old, new) // bool
```

### 2.12 `path` / `filepath` — 路径处理

```ms
import path

path.join(*parts)
path.base(p)          // 文件名部分
path.dir(p)           // 目录部分
path.ext(p)           // 扩展名（含点）
path.clean(p)         // 规范化
path.rel(base, target)// 相对路径
path.abs(p)           // 绝对路径（依赖 os.getcwd）
path.match(pattern, p)// glob 模式匹配
path.glob(pattern)    // 返回匹配文件 list
```

### 2.13 `net` — 网络（规划）

```ms
import net

// TCP
conn := await net.dial("tcp", "host:port")
conn.write(data)
data := await conn.read(n)
conn.close()

// HTTP 客户端（net/http）
import net.http
resp := await http.get(url)
resp.status    // int
resp.body      // string
resp.json()    // 解析 JSON body

// HTTP 服务端
srv := http.newServer(":8080")
srv.handle("/", func(req, resp) {
    resp.write("Hello!")
})
await srv.listenAndServe()
```

### 2.14 `sys` — 解释器接口

```ms
import sys

sys.version        // mslang 版本字符串
sys.platform       // "windows"/"linux"/"darwin"
sys.argv           // 等同 os.args
sys.path           // MSLANG_PATH list（可追加）
sys.modules        // 已加载模块缓存 map
sys.stdout / sys.stdin / sys.stderr
sys.exit(code=0)   // 等同 os.exit
sys.getframe()     // 当前调用帧信息（调试用）
```

### 2.15 `gc` — 垃圾回收控制

```ms
import gc

gc.collect()                // 强制 Minor + Major GC
gc.collect_young()          // 仅 Minor GC
gc.disable()
gc.enable()
gc.is_enabled()
stats := gc.stats()         // {allocated, collected, young_collections, ...}
gc.set_threshold(young_kb=4096, old_kb=65536)
```

---

---

## 3. 初版限制与规划中功能

| 功能 | 状态 | 说明 |
|---|---|---|
| `net` / `net.http` | 规划 | 网络模块整体为规划中，初版不提供 |
| `with` 语句（上下文管理器） | 规划 | `with open("f") as f { ... }` 初版不支持；`__enter__`/`__exit__` 槽预留 |
| f-string 格式规范（`{x:.4f}` 等） | 规划 | f-string 基础内插初版支持，格式规范后续版本加入 |
| 模块热重载 | 规划 | 详见 modules.md §11 |

---

## 4. 字符串内插（f-string）

```ms
name := "world"
s := $"hello {name}!"          // "hello world!"
s := $"1 + 1 = {1 + 1}"       // "1 + 1 = 2"
```

`$"..."` 为语法糖，编译器将 `{expr}` 替换为 `str(expr)` 拼接。格式规范（如 `{x:.4f}`）初版**不支持**，留待后续版本。
