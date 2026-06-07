# P12-T188 stdlib: testing

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `testing` 模块（对齐 `stdlib/testing.md`）：mslang 原生测试框架，支持单元测试、基准测试、测试发现，集成 `mslang test` 子命令。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T133 | sys（argv/exit） |
| P12-T142 | time.perf_counter（benchmark） |

---

## API 清单

```ms
// 测试函数（命名以 test_ 开头）
func test_addition() {
    testing.assert(1 + 1 == 2)
    testing.assertEqual(1 + 1, 2)
    testing.assertNotEqual(1, 2)
    testing.assertTrue(true)
    testing.assertFalse(false)
    testing.assertIs(a, b)         // a is b（同一对象）
    testing.assertIsNot(a, b)
    testing.assertIsNil(x)
    testing.assertIsNotNil(x)
    testing.assertIn(elem, container)
    testing.assertNotIn(elem, container)
    testing.assertRaises(ValueError, func() { int("abc") })
    testing.assertAlmostEqual(3.14, math.pi, places=2)
    testing.fail("explicit failure")
}

// Benchmark 函数（命名以 bench_ 开头）
func bench_sort(b) {
    data := list(range(1000, 0, -1))
    b.resetTimer()
    for i in range(b.N) {
        data_copy := list(data)
        data_copy.sort()
    }
    b.reportMetric(float(b.N * 1000), "items/s")
}

// TestCase 类（可选：面向对象风格）
class TestMath(testing.TestCase):
    func setUp(self) { self.x = 5 }
    func tearDown(self) { pass }
    func test_square(self) {
        self.assertEqual(self.x ** 2, 25)
    }

// 子测试
func test_table(t) {
    cases := [("a"+"b", "ab"), ("x"+"y", "xy")]
    for input, expected in cases {
        with t.run(str(input)) as st:
            testing.assertEqual(input, expected)
    }
}

// 运行（CLI）
// mslang test ./...         # 发现并运行所有 test_*.ms 文件
// mslang test -run test_add # 正则过滤
// mslang test -bench .      # 运行 benchmark
// mslang test -v            # verbose（打印每个测试名）
// mslang test -timeout 30s  # 超时

// 编程方式运行
runner := testing.Runner()
runner.discover("./tests/")
result := runner.run()
print(result.passed, result.failed, result.errors)
```

---

## 实现要点

```c
// 测试发现：
// 1. 扫描目录，找 test_*.ms 文件
// 2. 编译执行，收集以 test_ 开头的全局函数
// 3. 对每个测试函数：捕获异常（try/catch），pass/fail 计数

// assert 族函数实现：
// testing.assertEqual(a, b) → if a != b: raise AssertionError(msg)
// msg 包含实际值（repr(a) 和 repr(b)）

// 测试隔离：每个测试函数运行前重置全局状态（不同模块命名空间）

// Benchmark.B 对象：
// N：迭代次数（由框架自动调整直到运行时间 > 1s）
// 算法：从 N=1 开始，若 < 1s 则 N *= benchmarkIncrement（5~10×）
// resetTimer()：重置内部计时器
// reportMetric(v, unit)：自定义 metric

// TestCase 类：
// 使用反射（dir()）发现 test_ 开头的方法
// setUp/tearDown 围绕每个测试执行

// CLI 集成（mslang test 子命令）：
// cmdTest() 函数：解析 -run/-bench/-v/-timeout 标志
// 收集 .ms 文件，每个文件在独立模块上下文中 import
// 并行运行（可选：go 每个测试）

// 输出格式（类 go test）：
// --- PASS: testName (0.001s)
// --- FAIL: testName (0.000s)
//     assertion failed: 1 != 2
// PASS  (5 tests in 0.003s)
// FAIL  (1 failure)

typedef struct MsTObj {
  MsObject header;
  char*    name;
  bool     failed;
  bool     skipped;
  MsListObj* subtests;
  double   elapsed;
} MsTObj;
```

---

## 验收标准（checklist）

- [ ] `testing.assertEqual(1, 1)` 通过，`(1, 2)` 抛 AssertionError 含实际值。
- [ ] `testing.assertRaises(ValueError, ...)` 捕获正确类型。
- [ ] `mslang test` 发现并运行 test_*.ms 文件中的 test_ 函数。
- [ ] Benchmark B.N 自动调整到运行时间合理。
- [ ] TestCase setUp/tearDown 每个测试方法前后调用。
- [ ] `-run "test_add"` 过滤只运行名称匹配的测试。

---

## 测试用例（.ms）

```ms
import testing, math

// 基础断言
func test_math() {
    testing.assertEqual(2 + 2, 4)
    testing.assertAlmostEqual(math.pi, 3.14159, places=5)
    testing.assertRaises(ZeroDivisionError, lambda: 1/0)
}

// Table-driven
func test_add_table(t) {
    cases := [(1,2,3), (0,0,0), (-1,1,0)]
    for a, b, expected in cases {
        with t.run($"{a}+{b}") as st:
            testing.assertEqual(a + b, expected)
    }
}

// TestCase 风格
class TestString(testing.TestCase):
    func setUp(self) {
        self.s = "hello world"
    }
    func test_upper(self) {
        self.assertEqual(self.s.upper(), "HELLO WORLD")
    }
    func test_split(self) {
        self.assertEqual(self.s.split(), ["hello", "world"])
    }

// 运行（通常由 CLI 触发，此处手动）
runner := testing.Runner()
result := runner.runFunctions([test_math, test_add_table])
print(result.passed, result.failed)   // 2 0（均通过）
```
