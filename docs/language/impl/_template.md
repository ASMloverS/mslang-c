# P{n}-T{nnn} {任务标题}

> **状态**：⬜ 未开始 | 🚧 进行中 | ✅ 已完成 | ⏸️ 阻塞

---

## 任务目标 / 背景

（一句话目标描述：实现什么，解决什么问题，为何在此时做。）

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P?-T??? | 依赖原因 |

（无依赖则写"无"。）

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §X.Y 节标题 |
| `vm.md` | §X.Y 节标题 |

（精确指向 `docs/language/` 下实际存在的章节。）

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/xxx/xxx.c
include/mslang/xxx.h
```

### 关键结构体

```c
// 文件: include/mslang/xxx.h
struct XxxStruct {
  // 字段列表（对齐设计文档）
};
```

### 关键函数签名

```c
// 函数签名与简短说明
ReturnType functionName(MsVM* vm, ParamType param);
```

---

## 实现要点

1. **要点一**：算法选择、数据结构、与设计文档的对应关系。
2. **要点二**：关键边界条件与特殊情况。
3. **演进说明**（若为演进式任务）：本任务为 `{功能}` 的初始版本，后续 P?-T??? 将演进到 `{目标版本}`，此处按 `{限制}` 实现即可。

---

## 验收标准（checklist）

- [ ] 编译通过，无警告（`cmake --build build`）。
- [ ] 所有 C 单测通过（`ctest -R xxx`）。
- [ ] 所有 golden 文件比对通过。
- [ ] 端到端 `.ms` 测试脚本输出与期望一致（若 VM 已可用）。
- [ ] 具体功能点 1 …
- [ ] 具体功能点 2 …

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/xxx/test_xxx.c`）

```c
#include "ms_test.h"
#include "mslang/xxx.h"

static void testBasicCase(void) {
  // 安排
  // 执行
  // 断言
  MS_ASSERT_EQ(actual, expected, "描述");
}

int main(void) {
  MS_RUN(testBasicCase);
  // …
  return msTestSummary();
}
```

### golden 对比（`tests/golden/xxx/input.ms` → `xxx.expected`）

```
// input.ms
{输入脚本或 tokens/parse 子命令参数}
```

```
// xxx.expected（期望输出）
{期望输出文本}
```

### `.ms` 脚本测试（`tests/ms/xxx/test_xxx.ms`）

```ms
// 测试脚本
// 期望输出（行注释形式）
```

---

## .ms 使用示例

（面向用户视角的惯用写法。VM 前任务（P0–P3）标 N/A 或给出对应 CLI 子命令示例。）

```ms
// 示例代码
```

---

## Benchmark

（VM 后（P4-T067 起）用 `.ms` microbench；VM 前用 C microbench（如 tokens/sec）；无意义处标 N/A。）

```ms
// benchmarks/xxx/bench_xxx.ms
import time

start := time.perfCounter()
// 热点代码
elapsed := time.perfCounter() - start
print($"耗时: {elapsed * 1000:.2f} ms")
```

**指标参考**：{吞吐量 / 延迟 / 内存目标，或 N/A}。

---

## 风险与边界

- **未覆盖**：…（留待哪个后续任务处理）
- **已知坑**：…
- **平台差异**：…（Windows/Linux/macOS 注意事项）
