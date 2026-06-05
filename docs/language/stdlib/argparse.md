# argparse — 命令行参数解析

```ms
import argparse
```

## 概述

从 `os.args`（或自定义列表）解析命令行参数，自动生成 `--help` 与 `--version`
帮助信息。参考 Python `argparse` 模块设计。支持位置参数、可选参数、子命令、
互斥组以及多种 `action` 类型。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `argparse.ArgumentParser` | 参数解析器主类 |
| `argparse.Namespace` | 解析结果容器；属性即参数值 |
| `argparse.ArgumentError` | 参数定义错误（编程错误），继承自 `ValueError` |

## 函数签名速查

**ArgumentParser 构造与方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `ArgumentParser` | `ArgumentParser(prog=nil, description=nil, epilog=nil, addHelp=true, prefixChars="-", allowAbbrev=true)` | 创建解析器 |
| `addArgument` | `parser.addArgument(nameOrFlags..., **kw)` | 添加参数定义 |
| `parseArgs` | `parser.parseArgs(args=nil) → Namespace` | 解析并返回命名空间 |
| `parseKnownArgs` | `parser.parseKnownArgs(args=nil) → (Namespace, list[str])` | 解析并返回剩余参数 |
| `addSubparsers` | `parser.addSubparsers(dest="command", help=nil) → SubparsersAction` | 创建子命令组 |
| `addMutuallyExclusiveGroup` | `parser.addMutuallyExclusiveGroup(required=false) → MutuallyExclusiveGroup` | 创建互斥参数组 |
| `setDefaults` | `parser.setDefaults(**kw)` | 为任意属性设置默认值 |
| `printHelp` | `parser.printHelp()` | 打印帮助信息到 stdout |

**SubparsersAction 方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `addParser` | `subs.addParser(name, help=nil, description=nil) → ArgumentParser` | 注册一个子命令解析器 |

## 详细语义

### ArgumentParser 构造参数

```
argparse.ArgumentParser(
    prog=nil,
    description=nil,
    epilog=nil,
    addHelp=true,
    prefixChars="-",
    allowAbbrev=true,
) → ArgumentParser
```

- `prog`：程序名称，显示在帮助首行；`nil` 时取 `os.args[0]` 的文件名部分。
- `description`：在参数列表之前显示的简介文本。
- `epilog`：在参数列表之后显示的补充文本。
- `addHelp=true`：自动添加 `-h`/`--help` 参数；设为 `false` 可手动控制。
- `prefixChars`：选项前缀字符集合，默认 `"-"`（即 `-f`/`--flag` 风格）。
- `allowAbbrev=true`：允许唯一前缀缩写（如 `--verb` 匹配 `--verbose`）。

---

### parser.addArgument

**位置参数**（无前缀）：

```
parser.addArgument("name", type=str, help=nil, nargs=nil, default=nil, choices=nil)
```

**可选参数**（有前缀）：

```
parser.addArgument("--flag", "-f",
    type=str, default=nil, help=nil, required=false,
    nargs=nil, action=nil, choices=nil, dest=nil, metavar=nil)
```

**关键参数说明**：

- `type`：将字符串值转换为目标类型的可调用对象（如 `int`、`float` 或自定义函数）；
  转换失败时报错。
- `default`：参数未提供时的默认值；位置参数默认为 `nil`，可选参数默认为 `nil`。
- `required`：仅对可选参数有效；`true` 时未提供则报错。
- `nargs`：

  | 值 | 含义 |
  |---|---|
  | `nil` | 消耗一个值（默认） |
  | `"?"` | 0 或 1 个值；无值时用 `default`，无参数时用 `const` |
  | `"*"` | 0 个或多个值，返回 list |
  | `"+"` | 1 个或多个值，返回 list；0 个时报错 |
  | `N`（int） | 恰好 N 个值，返回 list |

- `action`：

  | 值 | 含义 |
  |---|---|
  | `"store"` | 存储值（默认） |
  | `"store_true"` | 出现时存储 `true`，默认 `false` |
  | `"store_false"` | 出现时存储 `false`，默认 `true` |
  | `"store_const"` | 出现时存储 `const=` 指定的值 |
  | `"append"` | 每次出现将值追加到 list |
  | `"count"` | 每次出现将计数加 1（默认 `nil`，首次为 `1`） |
  | `"version"` | 打印 `version=` 指定的版本字符串后退出 |

- `choices`：允许值列表；传入值不在列表中时报错。
- `dest`：存入 Namespace 的属性名；`nil` 时从参数名自动推导（`--my-flag` → `myFlag`）。
- `metavar`：帮助文本中显示的值占位符名称（不影响 `dest`）。

---

### parser.parseArgs

```
parser.parseArgs(args=nil) → Namespace
```

`args=nil` 时使用 `os.args[1:]`。解析成功返回 `Namespace`，通过 `.attribute` 访问各
参数值；`vars(args)` 可将其转为 `map`。

遇到 `--help` 或 `--version` 时打印信息并调用 `os.exit(0)`（即 `SystemExit`）。
解析出错时打印错误信息并调用 `os.exit(2)`。

---

### parser.parseKnownArgs

```
parser.parseKnownArgs(args=nil) → (Namespace, list[str])
```

与 `parseArgs` 相同，但未能识别的参数不报错，而是作为第二返回值的字符串列表返回。
适合需要透传参数给子进程的场景。

---

### 子命令

```
subs := parser.addSubparsers(dest="command", help=nil)
subInit := subs.addParser("init", help="初始化项目")
subInit.addArgument("--path", default=".")
```

解析后 `namespace.command` 保存被使用的子命令名称（字符串）；子命令特有参数也
合并进同一 `Namespace`。若未指定子命令且没有设为 `required`，`namespace.command`
为 `nil`。

---

### 互斥组

```
group := parser.addMutuallyExclusiveGroup(required=false)
group.addArgument("--verbose", action="store_true")
group.addArgument("--quiet", action="store_true")
```

同一互斥组中的参数最多只能出现一个；`required=true` 时至少需要一个。

## 示例

```ms
import argparse

// 1. 基础：位置参数 + 可选参数
parser := argparse.ArgumentParser(
    prog="mytool",
    description="示例命令行工具",
)
parser.addArgument("input", help="输入文件路径")
parser.addArgument("--output", "-o", default="out.txt", help="输出文件路径")
parser.addArgument("--count", "-n", type=int, default=10, help="处理行数")
parser.addArgument("--verbose", "-v", action="store_true", help="详细输出")

args := parser.parseArgs()
fmt.println($"输入: {args.input}, 输出: {args.output}, 行数: {args.count}")
if args.verbose {
    fmt.println("详细模式已开启")
}

// 2. store_true / count
p2 := argparse.ArgumentParser()
p2.addArgument("--debug", action="store_true")
p2.addArgument("-v", action="count", dest="verbosity")
a2 := p2.parseArgs(["--debug", "-v", "-v", "-v"])
fmt.println(a2.debug)      // true
fmt.println(a2.verbosity)  // 3

// 3. 子命令（git 风格）
root := argparse.ArgumentParser(prog="vcs")
subs := root.addSubparsers(dest="cmd")

// "commit" 子命令
cmdCommit := subs.addParser("commit", help="提交变更")
cmdCommit.addArgument("-m", "--message", required=true, help="提交信息")
cmdCommit.addArgument("--amend", action="store_true")

// "log" 子命令
cmdLog := subs.addParser("log", help="查看历史")
cmdLog.addArgument("--max-count", type=int, default=20, dest="maxCount")

args3 := root.parseArgs(["commit", "-m", "init"])
if args3.cmd == "commit" {
    fmt.println($"提交：{args3.message}，修正：{args3.amend}")
}

// 4. 互斥组
p4 := argparse.ArgumentParser()
g := p4.addMutuallyExclusiveGroup(required=true)
g.addArgument("--json", action="store_true")
g.addArgument("--text", action="store_true")
a4 := p4.parseArgs(["--json"])
fmt.println(a4.json)   // true
fmt.println(a4.text)   // false

// 5. parseKnownArgs：透传未知参数
p5 := argparse.ArgumentParser()
p5.addArgument("--config", default="config.toml")
known, rest := p5.parseKnownArgs(["--config", "my.toml", "--extra", "val"])
fmt.println(known.config)  // my.toml
fmt.println(rest)          // ["--extra", "val"]

// 6. vars(args) 转 map
p6 := argparse.ArgumentParser()
p6.addArgument("--host", default="localhost")
p6.addArgument("--port", type=int, default=8080)
m := vars(p6.parseArgs([]))
fmt.println(m)  // {"host": "localhost", "port": 8080}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `argparse.ArgumentError` | 参数定义存在冲突或非法（编程错误，继承自 `ValueError`） |
| `SystemExit(0)` | 遇到 `--help` 或 `--version`，打印信息后正常退出 |
| `SystemExit(2)` | 解析错误（缺少必填参数、类型转换失败、互斥冲突等），打印错误后退出 |
