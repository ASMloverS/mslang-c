# P3-T044 class 编译 + MAKE_CLASS / 方法表

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `ND_CLASS_DECL` 节点的字节码编译：为每个方法编译函数 chunk、构建方法名常量列表、emit `OP_MAKE_CLASS` 指令。VM（T072）在运行时据此创建 `MsType` 对象，并通过 MRO 链支持继承。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T043 | 函数编译（方法 = 函数） |
| P2-T034 | `ND_CLASS_DECL` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §5 class / `OP_MAKE_CLASS` |
| `type-system.md` | §3 MsType / MRO / 方法查找 |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileClassDecl
```

---

## 实现要点

### 1. 编译流程

```
OP_MAKE_CLASS  [2B: nameIdx]  [1B: methodCount]  [1B: baseCount]
    [2B: methodNameIdx] [2B: methodFuncIdx]  × methodCount  （方法名→函数原型）
```

实现步骤：
1. 若有基类，编译基类表达式并压栈（单继承：0 或 1 个）。
2. 编译所有成员：
   - `ND_FUNC_DECL` → 方法（编译为函数 chunk）
   - `ND_VAR_DECL`/`ND_ASSIGN` → 类属性（编译为常量，在 `__init__` 前设置）
3. emit `OP_MAKE_CLASS`，后跟名称、方法数量、每个方法的名称/proto 索引。
4. 若有命名类（非匿名），emit SET_LOCAL/SET_GLOBAL 绑定类名。

```c
static void compileClassDecl(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  uint16_t nameIdx = addStringConst(c, n->class_decl.name,
                                      (uint32_t)strlen(n->class_decl.name));

  // 编译基类（单继承：0 或 1 个）
  int baseCount = 0;
  if (n->class_decl.base != NULL) {
    compileExpr(c, n->class_decl.base);
    baseCount = 1;
  }

  // 收集方法与类属性
  struct MethodEntry { uint16_t nameIdx; uint16_t protoIdx; };
  struct MethodEntry methods[256]; int methodCount = 0;
  // 类属性存入常量池（key=nameIdx, val=defaultIdx）
  // 简化：类属性在 __init__ 中处理，此处只编译方法

  for (MsNodeList* l = n->class_decl.body; l; l = l->next) {
    MsNode* member = l->node;
    if (member->kind == ND_FUNC_DECL || member->kind == ND_ASYNC_FUNC) {
      // 编译为函数 chunk（复用 compileFuncDecl，但不 bind 到作用域）
      uint16_t protoIdx = compileFuncToConst(c, member);
      uint16_t mNameIdx = addStringConst(c, member->func_decl.name,
                                               (uint32_t)strlen(member->func_decl.name));
      methods[methodCount].nameIdx  = mNameIdx;
      methods[methodCount].protoIdx = protoIdx;
      methodCount++;
    }
    // ND_VAR_DECL 等类属性：初版放入特殊 class_attrs 常量，TODO
  }

  // emit OP_MAKE_CLASS
  emit(c, OP_MAKE_CLASS, line);
  // 2B: nameIdx
  emit(c, (uint8_t)(nameIdx >> 8), line);
  emit(c, (uint8_t)(nameIdx & 0xFF), line);
  // 1B: methodCount
  emit(c, (uint8_t)methodCount, line);
  // 1B: baseCount
  emit(c, (uint8_t)baseCount, line);
  for (int i = 0; i < methodCount; i++) {
    emit(c, (uint8_t)(methods[i].nameIdx >> 8), line);
    emit(c, (uint8_t)(methods[i].nameIdx & 0xFF), line);
    emit(c, (uint8_t)(methods[i].protoIdx >> 8), line);
    emit(c, (uint8_t)(methods[i].protoIdx & 0xFF), line);
  }

  // 绑定类名到作用域
  if (n->class_decl.name) {
    emitSetVar(c, n->class_decl.name,
                   (uint32_t)strlen(n->class_decl.name), line);
    emit(c, OP_POP, line);
  }
}
```

### 2. `OP_MAKE_CLASS` VM 语义（文档 spec）

VM 执行 `OP_MAKE_CLASS` 时：
1. 若 `baseCount=1`，从栈顶弹出基类对象（单继承）。
2. 创建 `MsType` 对象，填入名称、父类（T073 计算 MRO）。
3. 将 methodCount 个方法（从常量池取 proto，包装为 `MsFunc`）注册到 `MsType.methods` 哈希表。
4. 结果（类对象）压栈。

---

## 验收标准（checklist）

- [ ] `"class Foo {}"` → 外层 chunk 含 `OP_MAKE_CLASS`，参数 nameIdx=("Foo")，methodCount=0，baseCount=0。
- [ ] `"class Foo { func f(self) { } }"` → methodCount=1，方法名="f"。
- [ ] `"class Bar extends Foo {}"` → baseCount=1，基类 `Foo` 被压栈。
- [ ] 方法 chunk 正确编译（`return nil` 末尾，参数按局部槽分配）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_class_compile.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static void testClassEmpty(void) {
  MsCompileResult r = msCompile("class Foo {}", 12, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasMakeClass = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++)
    if (r.chunk->code[i] == OP_MAKE_CLASS) hasMakeClass = true;
  MS_ASSERT_TRUE(hasMakeClass, "has MAKE_CLASS");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testClassEmpty);
  return msTestSummary();
}
```

### .ms 使用示例（T072/T073 后验证）

```ms
class Animal {
    func __init__(self, name) {
        self.name = name
    }
    func speak(self) {
        print($"{self.name} says ...")
    }
}

class Dog extends Animal {
    func speak(self) {
        print($"{self.name} says Woof!")
    }
}

d := Dog("Rex")
d.speak()   // Rex says Woof!
print(isinstance(d, Dog))    // true
print(isinstance(d, Animal)) // true
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **类属性**：类级别的赋值（`sound := "Woof"` 在 class body）初版延迟到 `__init__` 或 class body 扫描时处理；简化为"所有类体内的非函数语句在类创建时执行"（类似 Python class body 执行语义）。
- **方法中 `super()`**：`super()` 在 VM 层（T075）通过 `OP_GET_SUPER` 实现；编译层只需正确设置调用帧的 `cls` 字段。
- **MRO**：C3 线性化在 VM 运行时（T073）计算；编译层不需要 MRO 知识。
