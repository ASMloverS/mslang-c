# P12-T152 stdlib: functools

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `functools` 模块（对齐 `stdlib/functools.md`）：高阶函数工具，包括 `reduce`、`partial`、`lru_cache`、`wraps`、`cached_property` 等。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T068 | 函数调用约定 |
| P4-T060 | MsMapObj（lru_cache 缓存表） |

---

## API 清单

```ms
// 核心工具
functools.reduce(func, iterable, initial=nil) → value
// 折叠：reduce(add, [1,2,3,4]) = ((1+2)+3)+4 = 10

functools.partial(func, *args, **kwargs) → partial_object
// 部分应用：pre-fill args/kwargs
// partial(int, base=2)("1010") → 10

functools.partialmethod(func, *args, **kwargs)
// 类似 partial，但用于方法（绑定 self 后再应用额外参数）

// 缓存装饰器
@functools.lru_cache(maxsize=128)
// 带 LRU 驱逐策略的记忆化（maxsize=nil=无界）
// 返回对象有 .cache_info()→(hits,misses,maxsize,currsize)
// .cache_clear()
@functools.cache         // 等同 lru_cache(maxsize=nil)

@functools.cached_property  // 计算一次后存为实例属性

// 函数工具
functools.wraps(wrapped)         // 装饰器：复制 __name__/__doc__ 等
functools.update_wrapper(wrapper, wrapped)

// 总排序（从少量比较方法自动推导）
@functools.total_ordering   // 需定义 __eq__ + 一个比较方法

// 组合/管道
functools.compose(*funcs)        // 右到左组合：compose(f,g)(x) = f(g(x))
functools.pipe(*funcs)           // 左到右：pipe(f,g)(x) = g(f(x))

// 比较转为 key
functools.cmp_to_key(cmp_func)  // cmp(a,b)=-1/0/1 → key 对象（sort 使用）
functools.mul                    // 乘法函数（供 reduce/accumulate）
functools.add                    // 加法函数
```

---

## 实现要点

```c
// partial：MsPartialObj 包含 func + 预填充 args + kwargs
typedef struct MsPartialObj {
  MsObject  header;
  MsValue   func;
  MsListObj* args;  // 预填充的位置参数
  MsMapObj*  kwargs; // 预填充的关键字参数
} MsPartialObj;

// 调用时：合并 partial.args + 调用时 args，kwargs 同

// lru_cache：环形双向链表（按访问顺序）+ HashMap
// 淘汰策略：满时删除链表尾（LRU）
// key：参数元组（需可 hash）→ 缓存值
// maxsize=nil：纯 HashMap，不淘汰

// cached_property：描述符，首次 __get__ 时计算并写入 obj.__dict__

// total_ordering：读取已定义的方法，生成剩余 6 个比较魔术方法

// cmp_to_key：返回包装类，__lt__ 等调用 cmp(self.val, other.val)
```

---

## 验收标准（checklist）

- [ ] `reduce(lambda a,b: a*b, range(1,6))` → `120`（阶乘）。
- [ ] `partial(pow, 2)(10)` → `1024`。
- [ ] `lru_cache` 的 Fibonacci 比无缓存快 100×。
- [ ] `lru_cache.cache_info()` 正确统计 hits/misses。
- [ ] `wraps` 复制 `__name__`、`__doc__` 到 wrapper。
- [ ] `cmp_to_key` 配合 `sort` 使用旧式比较函数。

---

## 测试用例（.ms）

```ms
import functools

// reduce
print(functools.reduce(lambda a,b: a+b, [1,2,3,4,5]))  // 15

// partial
double = functools.partial(lambda x,n: x*n, n=2)
print(double(5))  // 10

// lru_cache
@functools.lru_cache(maxsize=256)
func fib(n) {
    if n < 2 { return n }
    return fib(n-1) + fib(n-2)
}
print(fib(50))            // 12586269025
print(fib.cache_info())   // CacheInfo(hits=..., misses=51, maxsize=256, currsize=51)

// wraps
func myDecorator(func) {
    @functools.wraps(func)
    func wrapper(*args) {
        return func(*args)
    }
    return wrapper
}
@myDecorator
func hello() { return "hello" }
print(hello.__name__)   // "hello"（不是 "wrapper"）

// cmp_to_key
data := ["banana","apple","cherry"]
data.sort(key=functools.cmp_to_key(lambda a,b: -1 if a<b else (1 if a>b else 0)))
print(data)  // ["apple","banana","cherry"]
```

---

## Benchmark

```ms
import functools, time

@functools.lru_cache(maxsize=nil)
func ackermann(m, n) {
    if m == 0 { return n + 1 }
    if n == 0 { return ackermann(m-1, 1) }
    return ackermann(m-1, ackermann(m, n-1))
}

t0 := time.now()
print(ackermann(3, 8))   // 2045
t1 := time.now()
print("Ackermann(3,8):", t1-t0, "ms")  // 目标 < 100ms（有缓存）
```
