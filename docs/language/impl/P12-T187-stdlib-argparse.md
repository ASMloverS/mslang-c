# P12-T187 stdlib: argparse

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `argparse` 模块（对齐 `stdlib/argparse.md`）：命令行参数解析，支持子命令、类型转换、自动 --help。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T133 | sys.argv |

---

## API 清单

```ms
// 创建解析器
parser := argparse.ArgumentParser(
    prog=nil,           // 程序名（默认 sys.argv[0]）
    description=nil,    // 描述
    epilog=nil,         // 结尾说明
    add_help=true,      // 自动添加 -h/--help
    formatter_class=nil
)

// 添加参数
parser.add_argument("name")                        // 位置参数
parser.add_argument("-f", "--file", type=str)      // 可选参数
parser.add_argument("-v", "--verbose", action="store_true")  // 开关
parser.add_argument("-n", type=int, default=10)
parser.add_argument("--choices", choices=["a","b","c"])
parser.add_argument("--items", nargs="+")          // 1+ 个值
parser.add_argument("--pairs", nargs=2)            // 恰好 2 个
parser.add_argument("--opt", nargs="?", const="val")  // 0 或 1 个
parser.add_argument("files", nargs="*")            // 0+ 个位置参数

// action 类型：
// "store"（默认）  "store_true"  "store_false"  "store_const"
// "append"（多次指定则追加到列表）
// "count"（统计出现次数，-vvv → 3）
// "version"（打印版本并退出）

// 解析
ns := parser.parse_args()              // 解析 sys.argv[1:]
ns := parser.parse_args(["--file","a.txt"])  // 解析给定列表

// Namespace 对象
ns.file     // 访问参数值
ns.verbose  // True/False
getattr(ns, "name")  // 动态访问
vars(ns) → dict      // 转为字典

// 子命令
subparsers := parser.add_subparsers(dest="command")
cmd_a := subparsers.add_parser("run", help="run something")
cmd_a.add_argument("--target")
cmd_b := subparsers.add_parser("build")

// 互斥组
group := parser.add_mutually_exclusive_group()
group.add_argument("--verbose", action="store_true")
group.add_argument("--quiet", action="store_true")

// 错误处理
parser.error(message)    // 打印错误 + usage，退出 2
parser.exit(status=0, message=nil)  // 直接退出

// 帮助格式化
parser.format_help() → str
parser.format_usage() → str
parser.print_help(file=sys.stdout)
```

---

## 实现要点

```c
// ArgumentParser 内部：
// - positionals: list[Argument]（按添加顺序）
// - optionals: dict[flag, Argument]（"-f", "--file" 均映射到同一 Argument）
// - subparsers: SubparsersAction|nil

// parse_args 流程：
// 1. 将 argv 分类：以 "-" 开头为 optional，否则为 positional
// 2. 遍历 optional：匹配短/长选项，按 nargs 消费参数
// 3. 剩余按 positionals 顺序分配
// 4. 检查 required（无 default 且未提供）→ error
// 5. 类型转换：type(str_val)

// nargs 处理：
// None/"?"/"*"/"+"/int
// None：消费一个
// "?"：消费 0 或 1 个（有 const 时 0 个=const）
// "*"：消费任意个（包括 0）
// "+"：消费 1+ 个
// int：消费恰好 N 个

// 帮助文本生成：
// usage 行：prog [options] positionals
// 选项表：左列 flag，右列 help，自动对齐
// 若描述过长，在 term_width（默认 80）处折行

// 子命令：
// 第一个 positional 匹配 subparsers 名称 → 委托子解析器
// 剩余 argv 传给子解析器 parse_args

typedef struct MsArgumentObj {
  MsObject header;
  MsListObj* option_strings;  // ["-f", "--file"] 或 []（位置参数）
  char*    dest;      // 存储到 ns.dest
  char*    action;    // "store" / "store_true" / ...
  MsValue  nargs;     // nil / int / "?" / "*" / "+"
  MsValue  const_;
  MsValue  default_;
  MsValue  type_fn;   // callable（类型转换）
  MsListObj* choices; // nil 或 list
  bool     required;
  char*    help;
} MsArgumentObj;
```

---

## 验收标准（checklist）

- [ ] `--help` 打印帮助后退出（exit code 0）。
- [ ] 位置参数、可选参数、布尔开关正确解析。
- [ ] `nargs="+"` 收集一个或多个值为 list。
- [ ] `type=int` 自动转换类型，无效值抛 error。
- [ ] 子命令 `parse_args(["run","--target","x"])` → `ns.command="run", ns.target="x"`。
- [ ] 互斥组同时指定两个选项时抛 error。

---

## 测试用例（.ms）

```ms
import argparse

parser := argparse.ArgumentParser(prog="myapp", description="Test CLI")
parser.add_argument("input", help="input file")
parser.add_argument("-o", "--output", default="out.txt")
parser.add_argument("-v", "--verbose", action="store_true")
parser.add_argument("-n", type=int, default=10)

// 模拟命令行
ns := parser.parse_args(["data.csv", "-o", "result.txt", "-v", "-n", "5"])
print(ns.input)    // "data.csv"
print(ns.output)   // "result.txt"
print(ns.verbose)  // true
print(ns.n)        // 5（int，已转换）

// 子命令
p2 := argparse.ArgumentParser()
subs := p2.add_subparsers(dest="cmd")
run_p := subs.add_parser("run")
run_p.add_argument("--port", type=int, default=8080)
build_p := subs.add_parser("build")
build_p.add_argument("target")

ns2 := p2.parse_args(["run", "--port", "9090"])
print(ns2.cmd)   // "run"
print(ns2.port)  // 9090

ns3 := p2.parse_args(["build", "main"])
print(ns3.cmd)    // "build"
print(ns3.target) // "main"
```
