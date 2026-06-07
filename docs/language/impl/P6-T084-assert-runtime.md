# P6-T084 assert 运行期

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `OP_ASSERT` 指令的运行时语义：若条件为假，抛出 `AssertionError`（含可选消息）。在 Release 构建（`-DNDEBUG`）下，`OP_ASSERT` 为空操作（编译器已不 emit，见 T046）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T079 | `AssertionError` 类 |
| P6-T080 | raise 机制 |
| P3-T046 | assert 编译（`OP_ASSERT` 指令） |

---

## 实现要点

### 1. OP_ASSERT 实现

```c
// 栈：[cond_result, msg（可为 nil）]
// 注意：编译器在 OP_ASSERT 之前已经 emit 了 POP_JUMP_TRUE（条件为真则跳过）
// OP_ASSERT 执行时，说明条件为假，msg 在栈顶

case OP_ASSERT: {
  MsValue msg = POP();   // 消息（nil 或字符串）
  // 条件为假：抛出 AssertionError
  const char* msgStr = "assertion failed";
  if (MS_IS_OBJ(msg) && MS_AS_OBJ(msg)->type == &msStrType) {
    msgStr = ((MsStrObj*)MS_AS_OBJ(msg))->data;
  }
  return msRaiseAssertionError(t, msgStr);
}
```

### 2. Release 构建下的行为

```c
// T046 编译器：
// #ifndef NDEBUG
//   compileExpr(cond)
//   OP_POP_JUMP_TRUE [skip]
//   [msg or OP_NIL]
//   OP_ASSERT
//   [skip]:
// #endif
```

Release 下 `OP_ASSERT` 从未被 emit，无需 VM 层处理。

---

## 验收标准（checklist）

- [ ] `assert 1 == 1` → 不抛出。
- [ ] `assert 1 == 2` → AssertionError（无消息）。
- [ ] `assert 1 == 2, "1 is not 2"` → AssertionError 含消息 "1 is not 2"。
- [ ] AssertionError 可被 catch 捕获。
- [ ] Release 构建（NDEBUG）下 assert 无任何效果。

---

## 测试用例（.ms）

```ms
// 基本 assert
assert 1 + 1 == 2     // 通过
assert len([1,2]) > 0  // 通过

try {
    assert false, "should not be false"
} catch AssertionError as e {
    print(e.message)   // should not be false
}

// 常见用法：函数前置条件
func sqrt(x) {
    assert x >= 0, $"sqrt requires non-negative, got {x}"
    return x ** 0.5
}
print(sqrt(9))   // 3.0
try {
    sqrt(-1)
} catch AssertionError as e {
    print(e.message)   // sqrt requires non-negative, got -1
}
```

---

## Benchmark

N/A（assert 在 Release 下无开销，Debug 下性能无关紧要）。

---

## 风险与边界

- **assert 与 `AssertionError` 类**：若 `AssertionError` 未注册（T079 未完成），`OP_ASSERT` 只能创建简单错误字符串；T084 依赖 T079 完整注册。
