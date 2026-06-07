# P12-T186 stdlib: logging

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `logging` 模块（对齐 `stdlib/logging.md`）：Python 风格层级日志系统，支持多 Handler、格式化、日志级别过滤。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T142 | time（日志时间戳） |
| P12-T134 | io 模块（文件 handler） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-logging.md` | §1 模块 API |

---

## API 清单

```ms
// 模块级便捷函数（使用根 logger）
logging.debug(msg, *args, **kwargs)
logging.info(msg, *args, **kwargs)
logging.warning(msg, *args, **kwargs)
logging.error(msg, *args, **kwargs)
logging.critical(msg, *args, **kwargs)
logging.exception(msg, *args, exc_info=true)  // 自动附加 traceback
logging.log(level, msg, *args)

// 级别常量
logging.DEBUG    = 10
logging.INFO     = 20
logging.WARNING  = 30
logging.ERROR    = 40
logging.CRITICAL = 50

// 基础配置
logging.basicConfig(level=WARNING, format=nil, filename=nil,
                    filemode="a", stream=nil, handlers=nil)

// Logger 对象
logger := logging.getLogger(name="root")
logger := logging.getLogger("myapp")      // 命名 logger
logger := logging.getLogger("myapp.db")  // 子 logger（.分隔层级）
logger.setLevel(logging.DEBUG)
logger.addHandler(handler)
logger.removeHandler(handler)
logger.debug(msg)  ...  // 同模块级函数
logger.isEnabledFor(level) → bool
logger.propagate = true   // 是否向父 logger 传播

// Handler 类型
h := logging.StreamHandler(stream=sys.stderr)  // 写入流
h := logging.FileHandler(filename, mode="a", encoding="utf-8")
h := logging.NullHandler()               // 无操作
h.setLevel(logging.ERROR)               // handler 级别过滤
h.setFormatter(fmt)

// Formatter
fmt := logging.Formatter(
    format="%(asctime)s %(name)s %(levelname)s %(message)s",
    datefmt=nil
)
// 字段：%(asctime)s %(name)s %(levelname)s %(levelno)d
//       %(message)s %(filename)s %(lineno)d %(funcName)s
//       %(process)d %(thread)d %(pathname)s

// LogRecord（日志记录对象）
record.name  record.levelno  record.levelname
record.msg   record.args     record.message
record.created   record.filename   record.lineno
record.exc_info  record.exc_text   record.stack_info
```

---

## 实现要点

```c
// Logger 树：name → Logger（用 "." 分隔父子层级）
// 根 logger：getLogger("root") 或 getLogger(nil)
// 子 logger 未设 level 时，向上查找第一个有效 level

// propagate：若 true，log 记录传播到父 logger 的 handlers
// 防止重复：只传播，不重复执行自己的 handlers（或设 propagate=false）

// Handler.emit(record)：格式化 + 写入目标
// 线程/协程安全：Handler 内部 Mutex（多协程写同一文件）

// Formatter.format(record)：
// 先调用 formatTime(record) 填 asctime
// 然后 %(field)s 替换（类似简化版 sprintf）
// 若有 exc_info：格式化 traceback 追加

// basicConfig：
// 若根 logger 无 handler，则添加 StreamHandler(stderr)
// 设置 level、format

// 惰性格式化：logging.debug("msg %s", obj) 中 obj 的 str() 只在 DEBUG 开启时调用
// 通过 LogRecord.getMessage() 按需格式化

typedef struct MsLoggerObj {
  MsObject   header;
  char*      name;
  int        level;    // NOTSET=0 时向父查找
  bool       propagate;
  MsListObj* handlers;  // list[Handler]
  MsLoggerObj* parent;
} MsLoggerObj;
```

---

## 验收标准（checklist）

- [ ] `logging.basicConfig(level=logging.DEBUG)` 后 `logging.debug("msg")` 输出到 stderr。
- [ ] 级别过滤：DEBUG 消息不出现在 INFO 级别 logger 的输出中。
- [ ] 子 logger 传播到根 logger。
- [ ] FileHandler 写入文件，重启后追加（mode="a"）。
- [ ] Formatter 格式字符串正确替换。
- [ ] `logging.exception` 自动附加当前异常的 traceback。

---

## 测试用例（.ms）

```ms
import logging, sys, io

// 基础配置
buf := io.StringIO()
logging.basicConfig(level=logging.DEBUG, stream=buf,
    format="%(levelname)s - %(message)s")

logging.debug("debug msg")
logging.info("info msg")
logging.warning("warn msg")

buf.seek(0)
lines := buf.readlines()
print(len(lines))   // 3（全部级别均 >= DEBUG）

// 命名 logger
logger := logging.getLogger("myapp")
logger.setLevel(logging.WARNING)
h := logging.StreamHandler(sys.stderr)
h.setFormatter(logging.Formatter("%(name)s: %(message)s"))
logger.addHandler(h)
logger.propagate = false  // 不传播到根

logger.debug("this won't appear")   // 低于 WARNING，过滤
logger.warning("this will appear")  // stderr: "myapp: this will appear"

// exception logging
try:
    1 / 0
catch:
    logging.exception("division error")  // 包含 traceback

// 子 logger 传播
db_logger := logging.getLogger("myapp.db")
db_logger.error("db error")  // 传播到 myapp handler
```
