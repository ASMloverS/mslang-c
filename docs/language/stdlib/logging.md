# logging — 日志记录

```ms
import logging
```

## 概述

结构化日志记录框架，参考 Python `logging` 模块。支持日志级别过滤、多处理器
（Handler）并发输出、格式化（Formatter）以及树状日志器（Logger）层级继承。
根日志器（root logger）是所有命名日志器的祖先，提供全局默认行为。

库代码建议添加 `NullHandler` 而非配置真实处理器，将输出策略的控制权交给应用层。

## 常量与类型

**日志级别**

| 常量 | 值 | 说明 |
|---|---|---|
| `logging.NOTSET` | 0 | 未设置（继承父级级别） |
| `logging.DEBUG` | 10 | 调试信息 |
| `logging.INFO` | 20 | 普通信息 |
| `logging.WARNING` | 30 | 警告（默认有效级别） |
| `logging.ERROR` | 40 | 错误 |
| `logging.CRITICAL` | 50 | 严重错误 |

**类型**

| 名称 | 说明 |
|---|---|
| `logging.Logger` | 日志器类；通过 `get_logger` 获取，勿直接实例化 |
| `logging.Handler` | 处理器基类 |
| `logging.StreamHandler` | 向流（stream）输出日志 |
| `logging.FileHandler` | 向文件输出日志 |
| `logging.NullHandler` | 丢弃所有日志（用于库代码） |
| `logging.Formatter` | 日志格式化器 |
| `logging.LogRecord` | 单条日志记录（高级用途） |

## 函数签名速查

**模块级便捷函数（操作根日志器）**

| 函数 | 签名 | 说明 |
|---|---|---|
| `debug` | `debug(msg, *args, **kw)` | 输出 DEBUG 级别日志 |
| `info` | `info(msg, *args, **kw)` | 输出 INFO 级别日志 |
| `warning` | `warning(msg, *args, **kw)` | 输出 WARNING 级别日志 |
| `error` | `error(msg, *args, **kw)` | 输出 ERROR 级别日志 |
| `critical` | `critical(msg, *args, **kw)` | 输出 CRITICAL 级别日志 |
| `exception` | `exception(msg, *args)` | 输出 ERROR 日志并附加当前异常堆栈 |
| `log` | `log(level, msg, *args)` | 按指定级别输出日志 |
| `get_logger` | `get_logger(name=nil) → Logger` | 获取或创建命名日志器 |
| `basic_config` | `basic_config(level=WARNING, format=nil, filename=nil, filemode="a", stream=nil, handlers=nil)` | 一次性配置根日志器 |

**Logger 方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `setLevel` | `logger.setLevel(level)` | 设置有效级别 |
| `addHandler` | `logger.addHandler(handler)` | 添加处理器 |
| `removeHandler` | `logger.removeHandler(handler)` | 移除处理器 |
| `hasHandlers` | `logger.hasHandlers() → bool` | 是否有处理器（含父级） |
| `debug` | `logger.debug(msg, *args, **kw)` | DEBUG 日志 |
| `info` | `logger.info(msg, *args, **kw)` | INFO 日志 |
| `warning` | `logger.warning(msg, *args, **kw)` | WARNING 日志 |
| `error` | `logger.error(msg, *args, **kw)` | ERROR 日志 |
| `critical` | `logger.critical(msg, *args, **kw)` | CRITICAL 日志 |
| `exception` | `logger.exception(msg, *args)` | ERROR + 当前异常堆栈 |
| `log` | `logger.log(level, msg, *args)` | 按指定级别输出 |

**Handler 方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `setLevel` | `h.setLevel(level)` | 设置处理器过滤级别 |
| `setFormatter` | `h.setFormatter(fmt)` | 设置格式化器 |
| `emit` | `h.emit(record)` | 处理单条 LogRecord（高级） |
| `close` | `h.close()` | 释放资源（文件句柄等） |

## 详细语义

### logging.get_logger

```
logging.get_logger(name=nil) → Logger
```

按名称返回日志器。相同名称的调用始终返回同一对象（注册表单例）。`name=nil` 返回
根日志器。

名称以 `.` 分隔表示层级关系：`"app.db"` 是 `"app"` 的子级，`"app"` 是根日志器的
子级。子级的日志若 `propagate=true`（默认），会向上传播给父级的处理器处理。

---

### logging.basic_config

```
logging.basic_config(level=WARNING, format=nil, filename=nil,
                     filemode="a", stream=nil, handlers=nil)
```

对根日志器做一次性初始化配置。若根日志器已有处理器，调用无效（幂等）。

- `filename`：指定时添加 `FileHandler`；否则添加 `StreamHandler`（目标 `stream`，
  默认 `stderr`）。
- `handlers`：直接提供 Handler 列表，会忽略 `filename`/`stream`。
- `format`：Formatter 格式字符串；`nil` 使用默认格式。
- `level`：设置根日志器的有效级别。

---

### logging.Formatter

```
logging.Formatter(fmt=nil, datefmt=nil)
```

`fmt` 为格式字符串，使用 `%(key)s` 风格占位符：

| 占位符 | 含义 |
|---|---|
| `%(name)s` | 日志器名称 |
| `%(levelname)s` | 级别名称（如 `WARNING`） |
| `%(levelno)d` | 级别数值 |
| `%(message)s` | 格式化后的消息 |
| `%(asctime)s` | 时间字符串（由 `datefmt` 控制） |
| `%(filename)s` | 源文件名 |
| `%(lineno)d` | 行号 |
| `%(funcName)s` | 函数名 |
| `%(created)f` | 创建时间戳（Unix epoch） |

默认 `fmt`：`"%(levelname)s:%(name)s:%(message)s"`

`datefmt` 传给 `time.strftime`；`nil` 时使用 ISO 8601 风格。

---

### 日志传播与级别过滤

消息是否输出由两道关卡决定：

1. **Logger 级别**：`logger.level` 若非 `NOTSET`，低于该级别的消息直接丢弃。
   `NOTSET` 表示向上查询父级级别。
2. **Handler 级别**：每个 Handler 还有自己的级别过滤，通过 `setLevel` 设置。

`propagate=true` 时，经过本 Logger 过滤后的记录还会传递给父级的 Handler（不再受
父级 Logger 级别过滤，只受父级 Handler 级别过滤）。将 `propagate=false` 可阻止
传播，避免在父级重复输出。

---

### LogRecord 属性（高级）

| 属性 | 类型 | 说明 |
|---|---|---|
| `.name` | str | 日志器名称 |
| `.levelname` | str | 级别名称 |
| `.levelno` | int | 级别数值 |
| `.message` | str | 格式化后的消息 |
| `.msg` | str | 原始消息模板 |
| `.args` | tuple | 消息格式化参数 |
| `.asctime` | str | 时间字符串 |
| `.filename` | str | 源文件名 |
| `.lineno` | int | 行号 |
| `.funcName` | str | 函数名 |
| `.created` | float | Unix 时间戳 |

## 示例

```ms
import logging

// 1. 最简配置：输出 INFO 及以上到 stderr
logging.basic_config(level=logging.INFO)
logging.info("服务启动")
logging.warning("磁盘使用率超过 80%%")
logging.error("数据库连接失败")

// 2. 输出到文件，自定义格式
logging.basic_config(
    level=logging.DEBUG,
    filename="app.log",
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logging.debug("加载配置完成")

// 3. 命名日志器 + 文件处理器 + 格式化器
logger := logging.get_logger("app.db")
logger.setLevel(logging.DEBUG)

fh := logging.FileHandler("db.log")
fh.setLevel(logging.WARNING)
fh.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s"))

logger.addHandler(fh)
logger.warning("查询超时，耗时 3.2s")
logger.debug("执行 SQL: SELECT ...")  // 低于 fh 级别，不写入文件

// 4. 捕获异常并记录堆栈
try {
    x := 1 / 0
} catch Exception as e {
    logger.exception("除零错误")
}

// 5. 库代码：添加 NullHandler 避免"无处理器"警告
lib_logger := logging.get_logger("mylib")
lib_logger.addHandler(logging.NullHandler())

// 6. 阻止传播（子日志器独立输出，不再冒泡到根）
child := logging.get_logger("app.worker")
child.propagate = false
child.addHandler(logging.StreamHandler())
child.setLevel(logging.INFO)
child.info("worker 已就绪")
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `setLevel` 传入无效级别值 |
| `OSError` | `FileHandler` 打开或写入文件失败 |
