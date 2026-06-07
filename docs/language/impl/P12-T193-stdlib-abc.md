# P12-T193 stdlib: abc

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `abc` 模块（对齐 `stdlib/abc.md`）：抽象基类（Abstract Base Class）支持，允许定义接口和协议，在实例化时强制检查抽象方法。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T073 | 方法绑定/MRO（abc 依赖 class 系统） |
| P5-T078 | isinstance/type |

---

## API 清单

```ms
// 抽象基类
class MyABC(abc.ABC):
    @abc.abstractmethod
    func compute(self, x) { pass }  // 抽象方法（子类必须实现）

    @abc.abstractmethod
    @property
    func name(self) { pass }         // 抽象属性

    @abc.abstractclassmethod
    func create(cls) { pass }        // 抽象类方法

    @abc.abstractstaticmethod
    func validate(x) { pass }        // 抽象静态方法

// 子类实现所有抽象方法后可实例化
class Concrete(MyABC):
    func compute(self, x) { return x * 2 }
    @property
    func name(self) { return "Concrete" }
    @classmethod
    func create(cls) { return cls() }
    @staticmethod
    func validate(x) { return x > 0 }

obj := Concrete()  // OK

// 未实现时抛 TypeError
class Partial(MyABC):
    func compute(self, x) { return x }
    // 缺少 name/create/validate
try:
    Partial()
catch e as TypeError:
    print("Can't instantiate abstract class")  // 正确

// ABCMeta 元类（低层）
class MyInterface(metaclass=abc.ABCMeta):
    @abc.abstractmethod
    func process(self) { pass }

// 虚拟子类注册（不需要继承）
class Duck:
    func process(self) { return "quack" }
MyInterface.register(Duck)
isinstance(Duck(), MyInterface)  // true（注册后）

// 内置 ABC 类
// abc.Iterable、abc.Iterator、abc.Sized、abc.Callable、
// abc.Mapping、abc.MutableMapping、abc.Sequence、abc.Set 等
// （基于 __iter__/__len__/__call__ 等协议的虚拟子类判断）

// abc.get_cache_token() → int  // 实现注册后的缓存无效化 token
```

---

## 实现要点

```c
// ABCMeta 元类：
// __new__：扫描类定义，收集所有 __abstractmethods__ 集合
// 继承时：子类 __abstractmethods__ = 父类 __abstractmethods__ - 已实现方法

// __call__（实例化拦截）：
// if len(cls.__abstractmethods__) > 0:
//   raise TypeError("Can't instantiate abstract class X with abstract method(s) Y")

typedef struct MsAbcMeta {
  // 额外元类属性
  MsSetObj* abstractmethods;  // 未实现的抽象方法名集合
  MsSetObj* virtual_subclasses;  // register() 注册的类
  uint64_t  cache_token;
} MsAbcMeta;

// @abstractmethod：
// 在函数对象上设置 __isabstractmethod__ = true
// ABCMeta.__new__ 扫描类属性，收集所有 __isabstractmethod__=true 的方法

// 虚拟子类注册（register）：
// cls._abc_registry.add(subclass)
// 清除 isinstance 缓存

// isinstance 钩子（__subclasshook__）：
// 类定义 __subclasshook__(cls, C) 可自定义 isinstance 行为
// 如 Iterable.__subclasshook__ 检查 C 是否有 __iter__

// 内置 ABC（Iterable/Iterator 等）：
// 基于 __subclasshook__ 鸭子类型检查
// 不需要显式继承，有对应魔术方法即满足
```

---

## 验收标准（checklist）

- [ ] 未实现全部抽象方法的类实例化时抛 TypeError。
- [ ] 实现了所有抽象方法的子类可正常实例化。
- [ ] `register()` 注册的虚拟子类通过 `isinstance` 检查。
- [ ] 多继承时 `__abstractmethods__` 正确合并。
- [ ] `abc.Iterable` 对有 `__iter__` 的类返回 isinstance=true。
- [ ] `@property` 和 `@abstractmethod` 组合正确工作。

---

## 测试用例（.ms）

```ms
import abc

// 基础抽象类
class Shape(abc.ABC):
    @abc.abstractmethod
    func area(self) { pass }
    @abc.abstractmethod
    func perimeter(self) { pass }

class Circle(Shape):
    func __init__(self, r) { self.r = r }
    func area(self) { return 3.14159 * self.r ** 2 }
    func perimeter(self) { return 2 * 3.14159 * self.r }

c := Circle(5)
print(c.area())        // 78.5...
print(c.perimeter())   // 31.4...

// 实例化抽象类报错
try:
    Shape()
catch e as TypeError:
    print(e)  // 含 "area" "perimeter"

// 虚拟子类
class MyList:
    func __iter__(self) { return iter([1,2,3]) }
    func __len__(self) { return 3 }

print(isinstance(MyList(), abc.Iterable))  // true（有 __iter__）
print(isinstance(MyList(), abc.Sized))      // true（有 __len__）

// register
class Plugin:
    func process(self) { return "done" }

class Processor(abc.ABC):
    @abc.abstractmethod
    func process(self) { pass }

Processor.register(Plugin)
print(isinstance(Plugin(), Processor))  // true
```
