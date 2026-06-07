# P12-T133 stdlib: sys

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `sys` 内置模块：提供解释器版本、命令行参数、搜索路径、退出函数等运行期系统信息。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T090 | 内置模块注册 |
| P11-T130 | 扩展模块 API |

---

## API 清单

```ms
// sys 模块 API（对齐 stdlib/sys.md）
sys.argv        // list[str]  命令行参数（argv[0]=脚本路径）
sys.path        // list[str]  模块搜索路径（可运行时修改）
sys.version     // str        "mslang 0.1.0"
sys.platform    // str        "linux" | "darwin" | "windows"
sys.executable  // str        解释器可执行文件绝对路径
sys.maxint      // int        INT64_MAX
sys.stdin       // file       标准输入（file 对象）
sys.stdout      // file       标准输出
sys.stderr      // file       标准错误

sys.exit(code=0)         // 退出进程（抛 SystemExit 异常）
sys.getrecursionlimit()  // → int
sys.setrecursionlimit(n) // 设置最大递归深度
sys.getsizeof(obj)       // → int（对象近似内存大小）
sys.intern(s)            // → str（强制 intern 字符串）
sys.getrefcount(obj)     // → int（调试用，始终 >= 1）
```

---

## 实现要点

```c
// sys 模块在 msVMInit 中初始化（T090 已有骨架）
// 关键点：sys.argv 在 CLI 入口（T004）设置
// sys.path 与 gSearchPath 同步（列表元素是字符串）

// sys.exit(code)：抛出 SystemExit 异常（特殊异常，VM eval 循环最外层捕获）
static MsValue sysExit(MsThread* t, MsValue* args, int argc) {
  int64_t code = (argc >= 1 && MS_IS_INT(args[0])) ? MS_AS_INT(args[0]) : 0;
  // SystemExit 在 handle_error 中被特殊处理：不打印 traceback，直接 exit(code)
  return msRaise(t, msExcSystemExit, "%lld", (long long)code);
}
```

---

## 验收标准（checklist）

- [ ] `sys.argv[0]` = 脚本路径。
- [ ] `sys.platform` 在三平台返回正确字符串。
- [ ] `sys.exit(1)` 退出进程，返回码 1。
- [ ] `sys.path.append("/tmp")` 后模块搜索路径生效。
- [ ] `sys.setrecursionlimit(100)` 限制递归深度。

---

## 测试用例（.ms）

```ms
import sys
print(sys.version)      // mslang 0.1.0
print(sys.platform)     // linux / darwin / windows
print(type(sys.argv))   // list
print(sys.maxint > 0)   // true

sys.path.append("/tmp/mylib")
print("/tmp/mylib" in sys.path)  // true

try { sys.exit(0) } catch SystemExit as e { print("exit:", e.code) }
```

---

## 风险与边界

- **SystemExit**：`SystemExit` 是 `BaseException` 的直接子类（非 `Exception`），`catch Exception` 不会捕获它；只有 `catch BaseException` 或裸 `catch SystemExit` 能捕获。
