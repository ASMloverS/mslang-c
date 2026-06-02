# functools — 高阶函数与函数变换工具

```ms
import functools
```

## 概述

操作或返回其他函数的高阶工具，参考 Python `functools` 模块语义。
用于缓存热点函数的返回值、固定部分参数生成特化版本、构建自定义装饰器，
以及在排序场景中桥接旧式比较函数。

## 常量与类型

本模块不导出常量。`lru_cache` 返回的包装函数附有 `cache_info` 和 `cache_clear`
两个属性方法；`partial` 返回 `partial` 对象。

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `reduce` | `reduce(fn, iter, initial=nil) → value` | 左折叠累积 |
| `partial` | `partial(fn, *args, **kwargs) → partial_fn` | 固定部分参数 |
| `lru_cache` | `lru_cache(maxsize=128) → decorator` | LRU 缓存装饰器 |
| `cmp_to_key` | `cmp_to_key(cmp_fn) → key_fn` | 比较函数转 key 函数 |
| `wraps` | `wraps(wrapped_fn) → decorator` | 复制函数元数据 |

## 详细语义

### reduce

```
functools.reduce(fn, iter, initial=nil) → value
```

对 `iter` 从左到右依次应用 `fn(acc, x)`，将累积值作为下一次调用的第一个参数。

- 提供 `initial` 时，以 `initial` 作为首个 `acc`，`iter` 第一个元素作为首个 `x`。
- 不提供 `initial` 时，以 `iter` 前两个元素作为首次调用的参数。
- `iter` 为空且无 `initial` 时，抛 `TypeError`。
- `iter` 仅一个元素且无 `initial` 时，直接返回该元素（不调用 `fn`）。

```ms
// sum 等价实现
total := functools.reduce(func(a, b) { return a + b }, [1, 2, 3, 4])
// total == 10
```

### partial

```
functools.partial(fn, *args, **kwargs) → partial_fn
```

返回一个新的可调用对象，调用时将预先固定的 `args` 和 `kwargs` 与新传入的参数合并后调用 `fn`。

- 位置参数：预置 `args` 在前，调用时新传入的位置参数追加在后。
- 关键字参数：预置 `kwargs` 可被调用时的同名关键字参数覆盖。
- 返回对象具有 `func`、`args`、`keywords` 三个只读属性。

```ms
add := func(a, b) { return a + b }
add5 := functools.partial(add, 5)
fmt.println(add5(3))   // 8
fmt.println(add5(10))  // 15
```

### lru_cache

```
functools.lru_cache(maxsize=128) → decorator
```

返回一个装饰器，将被装饰函数的返回值按参数缓存。缓存使用 LRU（最近最少使用）策略。

- `maxsize=nil` 时缓存无界（等价于 `functools.cache`）。
- `maxsize` 必须为正整数或 `nil`，否则抛 `ValueError`。
- 所有参数必须可哈希；传入不可哈希参数时抛 `TypeError`。
- 被装饰函数附加两个方法：
  - `fn.cache_info() → (hits, misses, maxsize, currsize)`
  - `fn.cache_clear()` — 清空缓存，重置统计计数器

mslang 无 `@decorator` 语法糖，应用装饰器需显式调用：

```ms
fib := func(n) {
    if n < 2 { return n }
    return fib(n-1) + fib(n-2)
}
fib = functools.lru_cache(nil)(fib)

fmt.println(fib(35))          // 9227465
fmt.println(fib.cache_info()) // (34, 36, nil, 36)
fib.cache_clear()
```

### cmp_to_key

```
functools.cmp_to_key(cmp_fn) → key_fn
```

将旧式三路比较函数 `cmp_fn(a, b)` 转换为 `sorted`/`min`/`max` 接受的 `key` 函数。

`cmp_fn(a, b)` 约定：
- 返回负数：`a < b`
- 返回 0：`a == b`
- 返回正数：`a > b`

```ms
// 按字符串长度排序，同长度按字典序降序
cmp := func(a, b) {
    if len(a) != len(b) { return len(a) - len(b) }
    if a > b { return -1 }
    if a < b { return 1 }
    return 0
}
result := sorted(["banana", "fig", "apple", "kiwi"], key=functools.cmp_to_key(cmp))
fmt.println(result)  // ["fig", "kiwi", "apple", "banana"]
```

### wraps

```
functools.wraps(wrapped_fn) → decorator
```

在构建装饰器时使用，将 `wrapped_fn` 的 `__name__`、`__doc__`、`__module__` 复制到
包装函数上，使调试信息和文档保持准确。

```ms
func my_decorator(fn) {
    wrapper := func(*args, **kwargs) {
        fmt.println("calling", fn.__name__)
        return fn(*args, **kwargs)
    }
    wrapper = functools.wraps(fn)(wrapper)
    return wrapper
}

greet := func(name) { return "hello " + name }
greet = my_decorator(greet)
fmt.println(greet.__name__)   // "greet"
fmt.println(greet("world"))   // calling greet \n hello world
```

## 示例

```ms
import functools

// 1. reduce：阶乘
factorial := func(n) {
    return functools.reduce(func(a, b) { return a * b }, range(1, n+1))
}
fmt.println(factorial(5))  // 120

// 2. partial：构造带默认分隔符的 join 函数
join_comma := functools.partial(strings.join, sep=",")
fmt.println(join_comma(["a", "b", "c"]))  // "a,b,c"

// 3. lru_cache：缓存递归 Fibonacci
fib := func(n) {
    if n < 2 { return n }
    return fib(n-1) + fib(n-2)
}
fib = functools.lru_cache(128)(fib)
fmt.println(fib(40))            // 102334155
info := fib.cache_info()
fmt.println(info)               // (38, 41, 128, 41)

// 4. cmp_to_key：自定义版本号排序
ver_cmp := func(a, b) {
    pa := strings.split(a, ".")
    pb := strings.split(b, ".")
    for i in range(min(len(pa), len(pb))) {
        d := int(pa[i]) - int(pb[i])
        if d != 0 { return d }
    }
    return len(pa) - len(pb)
}
versions := ["1.10.0", "1.9.1", "2.0.0", "1.9.2"]
fmt.println(sorted(versions, key=functools.cmp_to_key(ver_cmp)))
// ["1.9.1", "1.9.2", "1.10.0", "2.0.0"]
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `TypeError` | `reduce` 的 iter 为空且无 initial；`fn` 不可调用；`lru_cache` 的被装饰函数收到不可哈希参数 |
| `ValueError` | `lru_cache` 的 maxsize 为零或负整数 |
