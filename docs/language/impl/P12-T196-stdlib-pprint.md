# P12-T196 stdlib: pprint

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `pprint` 模块（对齐 `stdlib/pprint.md`）：美化打印（pretty-print），对嵌套数据结构自动缩进、折行。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P8-T096 | repr() 内置函数 |
| P12-T141 | strings.Builder |

---

## API 清单

```ms
// 顶层函数
pprint.pprint(obj, stream=nil, indent=1, width=80, depth=nil,
              compact=false, sort_dicts=true, underscore_numbers=false)
// stream=nil → sys.stdout
// indent：嵌套缩进空格数（每层）
// width：目标行宽（80列）
// depth：打印深度（nil=无限）
// compact：紧凑模式（容器中多个元素可放同一行）

pprint.pformat(obj, indent=1, width=80, depth=nil, **kwargs) → str
// 同 pprint 但返回字符串

pprint.isreadable(obj) → bool   // repr(obj) 是否可被 eval 重建
pprint.isrecursive(obj) → bool  // obj 是否包含循环引用

// PrettyPrinter 类（可复用配置）
pp := pprint.PrettyPrinter(indent=4, width=100, depth=3)
pp.pprint(obj)
pp.pformat(obj) → str

// 递归检测
pprint.pprint([1, 2, 3])          // [1, 2, 3]（短，单行）
pprint.pprint(list(range(20)))    // 多行，按宽度折行
pprint.pprint({"a": [1,2,3], "b": {"nested": True}})
// {'a': [1, 2, 3],
//  'b': {'nested': True}}
```

---

## 实现要点

```c
// 核心算法：递归 _format(obj, stream, indent, allowance, context, level)
// allowance：当前行还剩余的宽度
// context：循环引用检测 set（id(obj)）
// level：当前嵌套层级

// 决策逻辑：
// 1. 先尝试 repr(obj)，若 len(repr) <= allowance → 单行输出
// 2. 否则多行输出：
//    list/tuple：[ 或 ( + 换行 + indent + 元素 + 分隔符 + ] 或 )
//    dict：按 sort_dicts 排序键，{ + 换行 + indent + key: val + ...
//    set/frozenset：{...} 多行
//    其他：直接 repr()

// depth 限制：超过 depth 时用 "..." 代替内容

// 循环引用：检测到后输出 <Recursion on TYPE with id=ADDR>

// sort_dicts=true（默认）：dict 键按字典序排列（Python 3.8+ 默认）

// compact=true：在宽度允许时，同一层级的多个元素放一行
// 例如：[1, 2, 3, 4, 5, 6, 7, 8] 可以折成
//        [1, 2, 3, 4,
//         5, 6, 7, 8]

// underscore_numbers=true：
// 数字加下划线分隔（1_000_000）

typedef struct PPCtx {
  int      indent;
  int      width;
  int      depth;
  bool     compact;
  bool     sort_dicts;
  MsSetObj* seen;     // 循环引用检测
  MsWriter buf;
} PPCtx;
```

---

## 验收标准（checklist）

- [ ] 短列表单行输出（宽度允许时）。
- [ ] 长列表多行输出，每层正确缩进。
- [ ] dict 按键排序（sort_dicts=true）。
- [ ] depth 限制：`depth=2` 时第三层用 `{...}` 代替。
- [ ] 循环引用显示 `<Recursion ...>` 而非无限循环/崩溃。
- [ ] `pformat` 返回字符串，与 `pprint` 输出等价。

---

## 测试用例（.ms）

```ms
import pprint

// 短列表 → 单行
pprint.pprint([1,2,3])         // [1, 2, 3]

// 长列表 → 多行
pprint.pprint(list(range(20)))
// [0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
//  10, 11, 12, 13, 14, 15, 16, 17, 18, 19]

// 嵌套 dict
data := {"name": "Alice", "scores": [95, 87, 92], "info": {"age": 30, "city": "NYC"}}
pprint.pprint(data, width=40)
// {'info': {'age': 30, 'city': 'NYC'},
//  'name': 'Alice',
//  'scores': [95, 87, 92]}

// depth 限制
pprint.pprint({"a": {"b": {"c": {"d": 1}}}}, depth=2)
// {'a': {'b': {...}}}

// pformat
s := pprint.pformat([{"key": "value"}, {"another": [1,2,3]}])
print(type(s))   // str
print(s)

// 循环引用
a := []
a.append(a)
pprint.pprint(a)  // [[<Recursion on list with id=...>]]
```
