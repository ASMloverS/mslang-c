# P3-T044 class 编译 + MAKE_CLASS / 方法表

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `MS_ND_CLASS_DECL` 节点的字节码编译：为每个方法编译函数 chunk、构建 class 描述符常量、emit `OP_MAKE_CLASS` 指令。VM（T072）在运行时据此创建 `MsType` 对象，并通过 MRO 链支持继承。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T043 | 函数编译（方法 = 函数） |
| P2-T034 | `MS_ND_CLASS_DECL` 节点 |

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
OP_MAKE_CLASS  [3B: classConstIdx]
```

`classConstIdx`：外层常量池中指向 class 描述符（含类名、方法名→proto 映射、baseCount 等）的索引。基类对象（若有）在 emit 前已压栈。

实现步骤：
1. 若有基类，编译基类表达式并压栈（单继承：0 或 1 个）。
2. 编译所有成员：
   - `MS_ND_FUNC_DECL`/`MS_ND_ASYNC_FUNC` → 方法（编译为函数 chunk，加入描述符）
   - `MS_ND_VAR_DECL`/`MS_ND_ASSIGN` → 类属性：初版暂不支持，遇到则报编译错误（TODO）
3. 将 class 描述符加入常量池，得到 `classConstIdx`；emit `OP_MAKE_CLASS [3B: classConstIdx]`。
4. 若有命名类（非匿名），emit SET_LOCAL/SET_GLOBAL 绑定类名。

```c
static void compileClassDecl(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  // 编译基类（单继承：0 或 1 个）
  int baseCount = 0;
  if (n->classDecl.base != NULL) {
    compileExpr(c, n->classDecl.base);
    baseCount = 1;
  }

  // 收集方法，构建 class 描述符常量
  struct MethodEntry { uint16_t nameIdx; uint16_t protoIdx; };
  struct MethodEntry methods[256]; int methodCount = 0;

  for (MsNodeList* l = n->classDecl.body; l; l = l->next) {
    MsNode* member = l->node;
    if (member->kind == MS_ND_FUNC_DECL || member->kind == MS_ND_ASYNC_FUNC) {
      if (methodCount >= 255) {
        compilerError(c, member->pos, "too many methods (max 255)");
        return;
      }
      // 编译为函数 chunk（复用 compileFuncDecl，但不 bind 到作用域）
      uint16_t protoIdx = compileFuncToConst(c, member);
      uint16_t mNameIdx = addStringConst(c, member->funcDecl.name,
                                         (uint32_t)strlen(member->funcDecl.name));
      methods[methodCount].nameIdx  = mNameIdx;
      methods[methodCount].protoIdx = protoIdx;
      methodCount++;
    }
    // MS_ND_VAR_DECL 等类属性：初版暂不支持，遇到则报 TODO 错误
  }

  // 将 class 描述符加入常量池，得到 3 字节 classConstIdx
  //    注：T049 之前无 MsClassDesc，先使用裸结构（不被 GC 管理）
  uint32_t classConstIdx = addClassDescConst(c, n->classDecl.name, baseCount,
                                              methods, methodCount);

  // emit OP_MAKE_CLASS [3B: classConstIdx]（基类已在前面压栈）
  emitAX(c->chunk, OP_MAKE_CLASS, classConstIdx, line);

  // 绑定类名到作用域
  if (n->classDecl.name) {
    emitSetVar(c, n->classDecl.name,
                   (uint32_t)strlen(n->classDecl.name), line);
    emit(c->chunk, OP_POP, line);
  }
}
```

### 2. `OP_MAKE_CLASS` VM 语义（文档 spec）

VM 执行 `OP_MAKE_CLASS` 时：
1. 从常量池取出 `classConstIdx` 对应的 class 描述符（含名称、方法名→proto 映射、baseCount）。
2. 若描述符 `baseCount=1`，从栈顶弹出基类对象（单继承）。
3. 创建 `MsType` 对象，填入名称、父类（T073 计算 MRO）。
4. 将描述符中的方法（proto 包装为 `MsFunc`）注册到 `MsType.methods` 哈希表（类属性同此字典）。
5. 结果（类对象）压栈。

---

## 验收标准（checklist）

- [ ] `"class Foo {}"` → 外层 chunk 含 `OP_MAKE_CLASS`，参数 nameIdx=("Foo")，methodCount=0，baseCount=0。
- [ ] `"class Foo { func f(self) { } }"` → methodCount=1，方法名="f"。
- [ ] `"class Bar extends Foo {}"` → baseCount=1，基类 `Foo` 被压栈。
- [ ] 方法 chunk 正确编译（末尾自动 emit `OP_RETURN_NIL`，参数按局部槽分配）。

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

- **类属性**：type-system.md §3.6 规定类属性存于 `MsType.methods` 字典。初版暂不支持类体内顶层赋值（`MS_ND_VAR_DECL`/`MS_ND_ASSIGN`），遇到此类节点报编译错误（TODO，T075 后补全）。
- **方法中 `super()`**：`super()` 在 VM 层（T075）通过 `OP_LOAD_SUPER` 实现；编译层只需正确设置调用帧的 `cls` 字段。
- **MRO**：C3 线性化在 VM 运行时（T073）计算；编译层不需要 MRO 知识。
