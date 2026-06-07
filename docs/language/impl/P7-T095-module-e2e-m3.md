# P7-T095 模块系统端到端 .ms 测试（M3 里程碑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

通过完整的 `.ms` 测试套件验证 P7 模块系统（T086–T094），包括：import 解析、模块缓存、包与子模块、字节码缓存往返、循环导入安全。此任务是 P7 阶段的**里程碑收口**（M3）：所有测试通过后，mslang 具备完整的模块加载能力。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T086 ~ T094 | P7 所有任务 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `modules.md` | §1 导入解析 / §2 模块缓存 / §3 包 |

---

## M3 测试套件（`tests/ms/p7/`）

### `tests/ms/p7/import_basic.ms`

```ms
// 确认模块隔离：模块内变量不污染调用者
// 假设 fixture: tests/fixtures/mymod.ms
//   val := 100
//   func double(x) { return x * 2 }

import mymod
print(mymod.val)         // 100
print(mymod.double(21))  // 42
print(type(mymod))       // module

// from import
from mymod import double
print(double(7))         // 14
```

**期望输出**：
```
100
42
module
14
```

### `tests/ms/p7/module_cache.ms`

```ms
// 确认模块只执行一次
// fixture: tests/fixtures/counted.ms
//   counter := 0
//   counter = counter + 1
//   print("counted.ms loaded, counter =", counter)

import counted
import counted   // 第二次不执行
import counted   // 第三次不执行

print(counted.counter)   // 1（只执行一次）
```

**期望输出**：
```
counted.ms loaded, counter = 1
1
```

### `tests/ms/p7/circular_import.ms`

```ms
// fixture: tests/fixtures/circ_a.ms
//   import circ_b
//   a_val := 10

// fixture: tests/fixtures/circ_b.ms
//   import circ_a
//   b_val := circ_a.a_val   // 此时 a_val 尚未定义 → nil
//   print("b_val:", b_val)

import circ_a
print(circ_a.a_val)  // 10
```

**期望输出**：
```
b_val: nil
10
```

### `tests/ms/p7/package_import.ms`

```ms
// 目录结构（fixture）：
//   tests/fixtures/mypkg/__init__.ms  →  pkg_version := "2.0"
//   tests/fixtures/mypkg/utils.ms     →  func greet(name) { return "hello, " + name }

import mypkg
print(mypkg.pkg_version)   // 2.0

import mypkg.utils
print(mypkg.utils.greet("world"))  // hello, world

from mypkg.utils import greet
print(greet("ms"))  // hello, ms
```

**期望输出**：
```
2.0
hello, world
hello, ms
```

### `tests/ms/p7/bytecode_cache.ms`

```bash
# Shell 测试（不是 .ms 文件）
# 1. 第一次运行（无缓存）：编译 + 执行
time mslang run tests/fixtures/mymod.ms

# 2. 运行 compile（生成 .msc）
mslang compile tests/fixtures/mymod.ms

# 3. 第二次运行（有缓存）：比第一次快
time mslang run tests/fixtures/mymod.ms

# 验证：第二次 < 第一次 × 0.5
```

### `tests/ms/p7/relative_import.ms`

```ms
// fixture: tests/fixtures/pkg/a.ms
//   from .b import val
//   a_val := val * 2

// fixture: tests/fixtures/pkg/b.ms
//   val := 21

// main（从 pkg/ 外导入）:
import pkg.a
print(pkg.a.a_val)   // 42
```

**期望输出**：
```
42
```

---

## 验收标准（checklist）

- [ ] `tests/ms/p7/import_basic.ms` 通过。
- [ ] `tests/ms/p7/module_cache.ms` 通过（计数器 = 1）。
- [ ] `tests/ms/p7/circular_import.ms` 通过（不死循环）。
- [ ] `tests/ms/p7/package_import.ms` 通过。
- [ ] `tests/ms/p7/relative_import.ms` 通过。
- [ ] `mslang compile foo.ms` + `mslang run foo.ms`（缓存命中）加速比 > 2×。
- [ ] `mslang compileall tests/` 无失败。
- [ ] `ModuleNotFoundError` 对未知模块正确报错。

---

## Benchmark

```ms
// benchmarks/bench_import.ms
// 测量模块加载时间（有缓存 vs 无缓存）
// 目标：10 个模块的 import 总耗时 < 5ms（有缓存）

import time
t0 := time.now()
import math
import strings
import os
import io
import sys
import collections
import itertools
import functools
import re
import json
t1 := time.now()
print("10 modules imported in", t1 - t0, "ms")
// 目标 < 5ms（有缓存路径）
```

---

## 风险与边界

- **fixture 目录**：所有 `tests/fixtures/` 目录下的 `.ms` 文件是专供测试用的辅助模块；需要在 `MSLANG_PATH` 或搜索路径中包含 `tests/fixtures/`。
- **M3 里程碑**：M3 通过表示 mslang 已具备生产级模块系统（import + 缓存 + 包），可以开始 stdlib 的实现。
