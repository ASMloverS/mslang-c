# testing — 单元测试框架（Go 风格）

```ms
import testing
```

## 概述

轻量级测试框架，参考 Go `testing` 包语义。测试函数以 `test` 为前缀，接收一个
`testing.T` 实例作为唯一参数，由测试运行器自动发现并调用。内置 `assert` 语句可
与本框架结合使用；`testing` 模块另提供带错误上下文的便捷断言函数。

测试文件通过文件名（`*_test.ms`）或命令行 `--test` 标志被发现：

```
mslang --test [./...] [-run <pattern>] [-v] [-bench <pattern>]
```

- `./...`：递归发现当前目录及子目录下所有 `*_test.ms` 文件。
- `-run <pattern>`：仅运行名称匹配正则的测试（匹配 `test` 前缀后的部分）。
- `-bench <pattern>`：运行名称匹配的基准测试（`bench` 前缀函数）。
- `-v`：详细模式，所有 `t.log` / `t.logf` 输出均可见。

## 常量与类型

| 类型 | 说明 |
|---|---|
| `testing.T` | 测试上下文，由运行器创建并传给每个 `test*` 函数 |
| `testing.B` | 基准测试上下文，由运行器创建并传给每个 `bench*` 函数 |

## 函数签名速查

### testing.T 方法

| 方法 | 签名 | 说明 |
|---|---|---|
| `error` | `t.error(*args)` | 记录失败消息；测试继续 |
| `errorf` | `t.errorf(fmtStr, *args)` | 格式化失败消息；测试继续 |
| `fatal` | `t.fatal(*args)` | 记录失败消息；停止当前测试 |
| `fatalf` | `t.fatalf(fmtStr, *args)` | 格式化失败消息；停止当前测试 |
| `log` | `t.log(*args)` | 记录信息（仅失败或 -v 时可见） |
| `logf` | `t.logf(fmtStr, *args)` | 格式化记录信息 |
| `skip` | `t.skip(*args)` | 标记为跳过；停止当前测试 |
| `skipf` | `t.skipf(fmtStr, *args)` | 格式化跳过消息 |
| `skipped` | `t.skipped() → bool` | 是否已被跳过 |
| `failed` | `t.failed() → bool` | 是否已失败 |
| `name` | `t.name() → str` | 当前测试名称 |
| `run` | `t.run(name, fn)` | 运行子测试；同步执行；返回 bool（true=通过） |
| `parallel` | `t.parallel()` | 标记本测试可与其他 parallel 测试并行 |
| `cleanup` | `t.cleanup(fn)` | 注册测试结束后执行的清理函数（类似 defer） |
| `helper` | `t.helper()` | 标记调用函数为辅助函数（失败输出中隐藏其栈帧） |
| `tempdir` | `t.tempdir() → str` | 创建临时目录，测试结束后自动删除 |

### testing.B 方法

`testing.B` 继承 `testing.T` 的所有方法，并额外提供：

| 方法/属性 | 签名 | 说明 |
|---|---|---|
| `n` | `b.n → int` | 运行器指定的迭代次数（循环体应执行恰好 `b.n` 次） |
| `resetTimer` | `b.resetTimer()` | 重置基准计时器（跳过初始化开销） |
| `startTimer` | `b.startTimer()` | 启动计时器 |
| `stopTimer` | `b.stopTimer()` | 停止计时器 |

### 便捷断言函数

| 函数 | 签名 | 说明 |
|---|---|---|
| `assertEqual` | `testing.assertEqual(t, got, want, msg="")` | got != want 时调用 t.fatal |
| `assertTrue` | `testing.assertTrue(t, cond, msg="")` | cond 为 false 时调用 t.fatal |
| `assertNil` | `testing.assertNil(t, val, msg="")` | val != nil 时调用 t.fatal |
| `assertNotNil` | `testing.assertNotNil(t, val, msg="")` | val == nil 时调用 t.fatal |
| `assertRaises` | `testing.assertRaises(t, excType, fn, msg="")` | fn() 未抛出 excType 时调用 t.fatal |

## 详细语义

### 测试函数约定

测试函数必须满足：

- 函数名以 `test` 为前缀（`test` 后跟首字母大写的名称，如 `testAdd`）。
- 接收唯一参数 `t`（`testing.T` 实例）。
- 定义于 `*_test.ms` 文件中，或在 `--test` 标志启用时的任意文件中。

```ms
func testAdd(t) {
    got := 1 + 2
    testing.assertEqual(t, got, 3)
}
```

### error 与 fatal 的区别

- `t.error` / `t.errorf`：记录失败但**测试继续执行**，适用于可累积多个失败的场景。
- `t.fatal` / `t.fatalf`：记录失败并**立即停止当前测试函数**（类似 panic），适用于
  后续步骤依赖当前步骤成功的场景。

### t.run — 子测试

`t.run(name, fn)` 在当前测试下运行命名子测试，`fn` 接收新的 `T` 实例。子测试
同步运行（除非 `fn` 内部调用了 `t.parallel()`）。

- 子测试失败不会自动导致父测试停止（父测试可检查返回值 bool）。
- `-run` 过滤格式：`TestName/SubTestName`（用 `/` 分隔层级）。

### t.parallel — 并行测试

调用 `t.parallel()` 后，当前测试暂停，等待非并行测试完成后与其他并行测试并发运行。
仅建议对无共享状态的独立测试使用。

### t.cleanup — 清理函数

多次调用 `t.cleanup(fn)` 时，所有已注册的函数在测试结束时以**后进先出**顺序执行，
无论测试是否失败。适合替代手动释放资源的逻辑。

### t.helper — 辅助函数标记

在辅助断言函数内部调用 `t.helper()`，测试框架在输出失败位置时会跳过该辅助函数的
栈帧，直接指向实际调用断言的测试代码行。

### 基准测试约定

基准函数以 `bench` 为前缀（`bench` 后跟首字母大写的名称，如 `benchAdd`），接收 `testing.B` 实例：

```ms
func benchAdd(b) {
    for i in range(b.n) {
        _ := 1 + 2
    }
}
```

运行器会自动调整 `b.n` 直至结果稳定，报告每次操作的纳秒数（ns/op）。

### 便捷断言函数语义

这些函数内部调用 `t.helper()` 以确保失败位置指向调用方。`msg` 为空时使用默认
错误消息。内置 `assert` 语句同样可用，区别在于便捷函数提供更详细的差异信息：

```ms
// 两种等价写法
assert got == 3, "expected 3"
testing.assertEqual(t, got, 3)   // 失败时输出：got=2, want=3
```

## 示例

```ms
import testing
import fmt

// --- 基础测试 ---
func testBasicMath(t) {
    testing.assertEqual(t, 1+1, 2)
    testing.assertEqual(t, 10-3, 7, "subtraction failed")
    testing.assertTrue(t, 5 > 3)
}

// --- 子测试 ---
func testStringOps(t) {
    cases := [
        ["hello" + " " + "world", "hello world"],
        ["abc"[:2], "ab"],
    ]
    for i, c in enumerate(cases) {
        name := fmt.sprintf("case_%d", i)
        t.run(name, func(sub) {
            testing.assertEqual(sub, c[0], c[1])
        })
    }
}

// --- cleanup 与 tempdir ---
func testTempResources(t) {
    dir := t.tempdir()  // 测试结束自动删除
    t.logf("using temp dir: %s", dir)

    conn := openMockConnection()
    t.cleanup(func() {
        conn.close()
        t.log("connection closed")
    })

    testing.assertNotNil(t, conn)
}

// --- assertRaises ---
func testDivisionByZero(t) {
    testing.assertRaises(t, ZeroDivisionError, func() {
        _ := 1 / 0
    })
}

// --- 基准测试 ---
func benchStringConcat(b) {
    b.resetTimer()
    for i in range(b.n) {
        s := "hello" + " " + "world"
        _ = s
    }
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `TypeError` | `t.run` 的 `fn` 不可调用；`t.cleanup` 的 `fn` 不可调用 |
| `AssertionError` | 内置 `assert` 语句条件为 false（非 `testing` 模块抛出） |
