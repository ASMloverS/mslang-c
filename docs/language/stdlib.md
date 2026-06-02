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
| `pow` | `pow(base, exp[, mod])` | 幂运算；三参时返回 `(base**exp) % mod`（模幂，整数快速算法） |
| `divmod` | `divmod(a, b)` | 返回 `(a // b, a % b)` tuple |
| `any` | `any(iter)` | 任一元素为真则返回 `true` |
| `all` | `all(iter)` | 全部元素为真则返回 `true` |
| `format` | `format(value, spec="")` | 调用 `value.__format__(spec)`，返回格式化字符串 |
| `hash` | `hash(x)` | 调用 `__hash__` |
| `id` | `id(x)` | 对象唯一 ID（地址或伪地址） |
| `callable` | `callable(x)` | 是否可调用 |
| `iter` | `iter(x)` | 调用 `__iter__` |
| `next` | `next(it[, default])` | 调用 `__next__`；无元素时返回 default 或抛 `StopIteration` |
| `chr` | `chr(code)` | 码点 → 单字符字符串 |
| `ord` | `ord(s)` | 单字符字符串 → 码点 |
| `hex` | `hex(n)` | int → `"0x..."` |
| `oct` | `oct(n)` | int → `"0o..."` |
| `bin` | `bin(n)` | int → `"0b..."` |
| `set` | `set(iterable=nil)` | 构造 set；无参返回空 set |
| `frozenset` | `frozenset(iterable=nil)` | 构造 frozenset |
| `bytes` | `bytes(source=nil[, encoding])` | 构造 bytes 对象 |
| `bytearray` | `bytearray(source=nil[, encoding])` | 构造可变字节数组 |
| `vars` | `vars([obj])` | 返回对象属性字典（或当前作用域 map） |
| `dir` | `dir([obj])` | 返回属性/方法名 list（按字母排序） |
| `open` | `open(path, mode="r", encoding="utf-8")` | 打开文件，返回 File 对象 |

### 关键字/语句（非函数值）

以下具有内置行为，但**不是**可传递的函数值，不可用于 `map(make, ...)` 等高阶用法：

| 形式 | 说明 |
|---|---|
| `make(chan[, cap])` | 创建 channel；语言关键字，不可作为值传递（文法见 syntax §1.4 / §2.3 MakeExpr） |
| `assert cond [, "msg"]` | 断言语句；条件为假时抛 `AssertionError`（文法见 syntax §1.4 / §2.2） |
| `close(ch)` | 关闭 channel；语言关键字，不可作为值传递（见 concurrency.md §3.4） |
| `with expr as name { ... }` | 上下文管理器语句；调用 `__enter__`/`__exit__`（目标语法，与 `open` 等配合使用） |

---

## 2. 命名规约

> **命名规约**：全库统一 Google C Style —— 函数与方法用 `snake_case`（如 `fmt.println`、`strings.has_prefix`、`mu.try_lock`）；导出类型用 `PascalCase`（如 `sync.Mutex`、`time.Duration`、`strings.Builder`）；模块级常量用小写（如 `math.pi`、`time.second`）。所有模块文档位于 `docs/language/stdlib/` 目录，后续新增 API 一律遵守本规约。

---

## 3. 字符串内插（f-string）

```ms
name := "world"
s := $"hello {name}!"          // "hello world!"
s := $"1 + 1 = {1 + 1}"       // "1 + 1 = 2"
```

`$"..."` 为语法糖，编译器将 `{expr}` 替换为 `str(expr)` 拼接。格式规范（如 `{x:.4f}`）初版**不支持**，留待后续版本。

---

## 4. 模块索引

> 所有模块文档位于 `docs/language/stdlib/` 目录。导入语法：`import <模块名>`。

### 文本与格式

| 模块 | 说明 | 文档 |
|---|---|---|
| `fmt` | 格式化输出（printf/sprintf/fprintf/errorf） | [stdlib/fmt.md](stdlib/fmt.md) |
| `strings` | 字符串处理（查找/替换/分割/Unicode 辅助） | [stdlib/strings.md](stdlib/strings.md) |
| `textwrap` | 文本折行与缩进 | [stdlib/textwrap.md](stdlib/textwrap.md) |
| `csv` | CSV 读写 | [stdlib/csv.md](stdlib/csv.md) |
| `re` | 正则表达式（RE2 语法） | [stdlib/re.md](stdlib/re.md) |

### 数据结构

| 模块 | 说明 | 文档 |
|---|---|---|
| `collections` | deque / Counter / defaultdict / OrderedDict / namedtuple | [stdlib/collections.md](stdlib/collections.md) |
| `heapq` | 堆队列（优先队列） | [stdlib/heapq.md](stdlib/heapq.md) |
| `bisect` | 二分插入与查找 | [stdlib/bisect.md](stdlib/bisect.md) |
| `array` | 同类型元素的紧凑数组 | [stdlib/array.md](stdlib/array.md) |
| `queue` | 线程安全队列（Queue / LifoQueue / PriorityQueue） | [stdlib/queue.md](stdlib/queue.md) |

### 函数式编程

| 模块 | 说明 | 文档 |
|---|---|---|
| `itertools` | 惰性迭代器组合工具 | [stdlib/itertools.md](stdlib/itertools.md) |
| `functools` | 高阶函数（reduce / partial / lru_cache / wraps） | [stdlib/functools.md](stdlib/functools.md) |

### 二进制与编码

| 模块 | 说明 | 文档 |
|---|---|---|
| `base64` | Base16/32/64 编解码（含 hex） | [stdlib/base64.md](stdlib/base64.md) |
| `struct` | 二进制数据打包/解包 | [stdlib/struct.md](stdlib/struct.md) |

### 数值与统计

| 模块 | 说明 | 文档 |
|---|---|---|
| `math` | 数学函数与常量 | [stdlib/math.md](stdlib/math.md) |
| `random` | 伪随机数 | [stdlib/random.md](stdlib/random.md) |
| `statistics` | 统计函数（均值/中位数/标准差等） | [stdlib/statistics.md](stdlib/statistics.md) |
| `decimal` | 任意精度十进制浮点 | [stdlib/decimal.md](stdlib/decimal.md) |
| `fractions` | 有理数 | [stdlib/fractions.md](stdlib/fractions.md) |

### 哈希与加密

| 模块 | 说明 | 文档 |
|---|---|---|
| `hashlib` | 哈希摘要（MD5/SHA256/SHA3 等） | [stdlib/hashlib.md](stdlib/hashlib.md) |
| `hmac` | 基于哈希的消息认证码 | [stdlib/hmac.md](stdlib/hmac.md) |
| `secrets` | 密码学安全随机数 | [stdlib/secrets.md](stdlib/secrets.md) |

### 压缩与归档

| 模块 | 说明 | 文档 |
|---|---|---|
| `gzip` | gzip 压缩/解压 | [stdlib/gzip.md](stdlib/gzip.md) |
| `zipfile` | ZIP 归档读写 | [stdlib/zipfile.md](stdlib/zipfile.md) |
| `tarfile` | TAR 归档读写 | [stdlib/tarfile.md](stdlib/tarfile.md) |

### 日期与时间

| 模块 | 说明 | 文档 |
|---|---|---|
| `time` | 时钟、睡眠、计时（Python 风格）；调度扩展：`time.after` | [stdlib/time.md](stdlib/time.md) |
| `datetime` | 日期/时间/时区/时间差对象 | [stdlib/datetime.md](stdlib/datetime.md) |
| `calendar` | 日历辅助工具 | [stdlib/calendar.md](stdlib/calendar.md) |

### 操作系统与进程

| 模块 | 说明 | 文档 |
|---|---|---|
| `os` | 文件系统、环境变量、路径（含 `os.path`） | [stdlib/os.md](stdlib/os.md) |
| `io` | I/O 抽象、异步 I/O | [stdlib/io.md](stdlib/io.md) |
| `sys` | 解释器接口 | [stdlib/sys.md](stdlib/sys.md) |
| `subprocess` | 子进程管理 | [stdlib/subprocess.md](stdlib/subprocess.md) |
| `signal` | 信号处理 | [stdlib/signal.md](stdlib/signal.md) |
| `shutil` | 高级文件/目录操作 | [stdlib/shutil.md](stdlib/shutil.md) |
| `tempfile` | 临时文件与目录 | [stdlib/tempfile.md](stdlib/tempfile.md) |

### 网络

| 模块 | 说明 | 文档 |
|---|---|---|
| `net` | TCP/UDP 拨号与监听 | [stdlib/net.md](stdlib/net.md) |
| `socket` | 低级 BSD 套接字 | [stdlib/socket.md](stdlib/socket.md) |
| `http` | HTTP 客户端与服务端（异步） | [stdlib/http.md](stdlib/http.md) |
| `url` | URL 解析与编码 | [stdlib/url.md](stdlib/url.md) |

### 并发

| 模块 | 说明 | 文档 |
|---|---|---|
| `sync` | 互斥锁 / RWMutex / WaitGroup / Once / 原子操作 | [stdlib/sync.md](stdlib/sync.md) |
| `context` | 取消/超时上下文传播（Go 风格） | [stdlib/context.md](stdlib/context.md) |

### 序列化

| 模块 | 说明 | 文档 |
|---|---|---|
| `json` | JSON 编解码 | [stdlib/json.md](stdlib/json.md) |

### 工具

| 模块 | 说明 | 文档 |
|---|---|---|
| `logging` | 日志记录（Python 风格） | [stdlib/logging.md](stdlib/logging.md) |
| `argparse` | 命令行参数解析（Python 风格） | [stdlib/argparse.md](stdlib/argparse.md) |
| `testing` | 单元测试框架（Go 风格） | [stdlib/testing.md](stdlib/testing.md) |
| `uuid` | UUID 生成 | [stdlib/uuid.md](stdlib/uuid.md) |
| `enum` | 枚举类型 | [stdlib/enum.md](stdlib/enum.md) |
| `copy` | 浅拷贝与深拷贝 | [stdlib/copy.md](stdlib/copy.md) |
| `sort` | 排序工具 | [stdlib/sort.md](stdlib/sort.md) |
| `gc` | 垃圾回收控制 | [stdlib/gc.md](stdlib/gc.md) |
