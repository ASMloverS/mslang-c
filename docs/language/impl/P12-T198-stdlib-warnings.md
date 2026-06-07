# P12-T198 stdlib: warnings

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `warnings` 模块：运行时警告系统，支持过滤、自定义处理器、一次性警告（不重复）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T079 | 异常类层次（Warning 是 Exception 的子类） |
| P12-T197 | traceback（警告含调用栈信息） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-warnings.md` | §1 模块 API |

---

## API 清单

```ms
// 发出警告
warnings.warn(message, category=UserWarning, stacklevel=1, source=nil)
// message：str 或 Warning 实例
// category：Warning 子类（UserWarning, DeprecationWarning, ...）
// stacklevel：指示哪一帧是"源"（1=调用 warn 的帧，2=其调用者）

// 警告类别
warnings.Warning            // 基类（Exception 子类）
warnings.UserWarning        // 用户代码警告（默认）
warnings.DeprecationWarning // 已弃用（默认被过滤）
warnings.PendingDeprecationWarning
warnings.RuntimeWarning
warnings.SyntaxWarning
warnings.ResourceWarning    // 资源泄漏
warnings.FutureWarning
warnings.ImportWarning

// 过滤器
warnings.filterwarnings(action, message="", category=Warning,
                         module="", lineno=0, append=false)
// action: "default" "error" "ignore" "always" "module" "once"

warnings.simplefilter(action, category=Warning, lineno=0, append=false)
// 不按 module/lineno 过滤的简化版

warnings.resetwarnings()  // 清除所有过滤器

// 已显示的警告记录（用于 "once" 和 "default" 策略）
warnings.filters      // 当前过滤器列表（可直接修改）

// 上下文管理器（临时修改警告配置）
with warnings.catch_warnings():
    warnings.simplefilter("error")
    // 此块内警告变异常

with warnings.catch_warnings(record=true) as w:
    warnings.simplefilter("always")
    some_function()  // 可能发出警告
// w 是 WarningMessage 列表
print(len(w), w[0].category, w[0].message)

// 自定义处理器
def my_handler(message, category, filename, lineno, file=nil, line=nil):
    print("Warning:", category.__name__, message)
warnings.showwarning = my_handler

// 警告变异常
warnings.filterwarnings("error", category=DeprecationWarning)
```

---

## 实现要点

```c
// 警告过滤器链：list 有序，首个匹配的 filter 决定行为
// 过滤器：(action, message_re, category, module_re, lineno)

// warn() 流程：
// 1. 获取调用帧（stacklevel 决定从哪帧报告）
// 2. 构建 WarningKey = (text, category, filename, lineno)
// 3. 遍历 filters → 找到匹配的 action
//    "error" → raise Warning（或转换为 exception）
//    "ignore" → 直接返回
//    "always" → 每次都显示
//    "default" → 每个(filename, lineno)只显示一次
//    "module" → 每个 module 只显示一次
//    "once" → 整个程序只显示一次
// 4. 调用 warnings.showwarning(message, category, filename, lineno, file, line)
// 5. showwarning 默认写到 sys.stderr

// 已见警告记录：
// gVM.warnRegistry: set of WarningKey（"default" 过滤用）

// catch_warnings 上下文管理器：
// __enter__：保存 warnings.filters + showwarning（快照）
// __exit__：恢复快照

// record=True：
// 临时替换 showwarning 为收集函数，
// 收集到 list[WarningMessage]，退出时恢复

typedef struct MsWarningFilter {
  int      action;   // WARN_ERROR/IGNORE/ALWAYS/DEFAULT/MODULE/ONCE
  char*    message_re;   // 消息正则（nil=匹配全部）
  MsValue  category;     // Warning 类或子类
  char*    module_re;    // 模块名正则（nil=匹配全部）
  int      lineno;       // 0=匹配全部
} MsWarningFilter;
```

---

## 验收标准（checklist）

- [ ] `warnings.warn("msg")` 默认打印到 stderr（带文件/行号）。
- [ ] `simplefilter("ignore")` 后警告静默。
- [ ] `simplefilter("error")` 后警告变 UserWarning 异常。
- [ ] `"default"` 策略：同一位置同类警告只显示一次。
- [ ] `catch_warnings(record=True)` 收集警告到列表。
- [ ] 退出 `catch_warnings` 上下文后，过滤器恢复原状。

---

## 测试用例（.ms）

```ms
import warnings, io, sys

// 基础警告
old_stderr = sys.stderr
buf := io.StringIO()
sys.stderr = buf
warnings.warn("something deprecated", warnings.DeprecationWarning, stacklevel=2)
sys.stderr = old_stderr
print("DeprecationWarning" in buf.getvalue())  // true

// catch_warnings record
with warnings.catch_warnings(record=true) as w:
    warnings.simplefilter("always")
    warnings.warn("test warning", UserWarning)
    warnings.warn("another", RuntimeWarning)
print(len(w))          // 2
print(str(w[0].message))  // "test warning"
print(w[1].category)      // RuntimeWarning

// 过滤器恢复
warnings.simplefilter("error")
try:
    warnings.warn("error!")
catch e as UserWarning:
    print("Caught as exception:", e)
// catch_warnings 退出后恢复
// 若不在 catch_warnings 中，simplefilter 持久有效

// once 策略
with warnings.catch_warnings(record=true) as w:
    warnings.simplefilter("once")
    for _ in range(5):
        warnings.warn("only once", stacklevel=1)
print(len(w))   // 1（只记录一次）
```
