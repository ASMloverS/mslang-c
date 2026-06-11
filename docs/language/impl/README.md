# mslang 实现路线图与任务索引

本文件是 mslang 解释器实现的**唯一总索引**，按阶段分组列出全部 201 个任务，每个任务对应 `impl/` 目录下的一个 Markdown 文档，包含详尽的设计引用、C 代码规格、测试用例与 benchmark 规格。

---

## 阶段依赖图

```
P0 工程地基
  └─▶ P1 词法 (Lexer)
        └─▶ P2 语法/AST (Parser)
              └─▶ P3 编译器 (Compiler)
                    └─▶ P4 VM + 核心类型 + 简易GC   ←── M1 里程碑 (Hello World)
                          ├─▶ P5 函数/闭包/class     ←── M2 里程碑 (函数与类)
                          │     ├─▶ P6 异常           ←── M3 里程碑 (异常+模块+缓存)
                          │     │     └─▶ P7 模块+字节码缓存
                          │     └─▶ P8 内置函数       ←── M4 里程碑 (内置函数完备)
                          ├─▶ P9  并发               ←── M5 里程碑 (并发)
                          ├─▶ P10 GC演进             ←── M6 里程碑 (完整GC)
                          ├─▶ P11 C API              ←── M7 里程碑 (C API)
                          └─▶ P12 stdlib ×50+        ←── M8 里程碑 (stdlib完备)
```

---

## 里程碑说明

| 里程碑 | 完成条件 | 对应任务 |
|---|---|---|
| **M1** | `mslang run hello.ms` 可输出，基础算术/字符串/容器可用 | P4-T067 完成 |
| **M2** | 函数调用/闭包/class/继承/魔术方法全部可用 | P5-T078 完成 |
| **M3** | 异常完整 + 模块导入 + 字节码缓存 | P7-T095 完成 |
| **M4** | 所有内置函数（`len`/`sorted`/`range`…）可用 | P8-T105 完成 |
| **M5** | `go`/channel/`async`/`await`/`select` 可用 | P9-T114 完成 |
| **M6** | 三代分代 GC + 并发标记 + 写屏障完整 | P10-T125 完成 |
| **M7** | C 嵌入/扩展 API 完整，可写 C 扩展模块 | P11-T132 完成 |
| **M8** | 全部 50+ stdlib 模块实现 | P12-T201 完成 |

---

## 状态 Emoji 图例

| emoji | 含义 |
|---|---|
| ⬜ | 未开始 |
| 🚧 | 进行中 |
| ✅ | 已完成 |
| ⏸️ | 阻塞（等待依赖或决策） |

---

## P0 — 工程地基（T001–T005）

> CMake 跨平台构建、通用工具、C 单测框架、CLI 骨架、公共头文件。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T001 | 项目骨架与 CMake 跨平台构建 | [P0-T001-project-scaffold-cmake.md](P0-T001-project-scaffold-cmake.md) | 无 | ✅ |
| T002 | 通用工具：内存/动态数组/错误码/FNV-1a | [P0-T002-common-utils.md](P0-T002-common-utils.md) | T001 | ✅ |
| T003 | 极简 C 单测框架 `ms_test.h` + golden runner | [P0-T003-test-framework.md](P0-T003-test-framework.md) | T001 | ✅ |
| T004 | CLI 骨架：子命令与标志解析 | [P0-T004-cli-skeleton.md](P0-T004-cli-skeleton.md) | T002 | ✅ |
| T005 | 公共头与前置类型（`mslang.h` umbrella） | [P0-T005-public-headers.md](P0-T005-public-headers.md) | T002 | ✅ |

---

## P1 — 词法分析（T006–T016）

> Lexer：Token 定义、字面量、关键字、运算符、ASI、`tokens` 子命令。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T006 | Token 定义与 lexer 框架（位置/行号/错误恢复） | [P1-T006-lexer-token-framework.md](P1-T006-lexer-token-framework.md) | T005 | ⬜ |
| T007 | 标识符与关键字识别 | [P1-T007-lexer-identifiers-keywords.md](P1-T007-lexer-identifiers-keywords.md) | T006 | ⬜ |
| T008 | 整数字面量（dec/hex/oct/bin/`_`分隔） | [P1-T008-lexer-int-literals.md](P1-T008-lexer-int-literals.md) | T006 | ⬜ |
| T009 | 浮点字面量（IEEE 754，指数形式） | [P1-T009-lexer-float-literals.md](P1-T009-lexer-float-literals.md) | T006 | ⬜ |
| T010 | 字符串字面量与转义序列 | [P1-T010-lexer-string-literals.md](P1-T010-lexer-string-literals.md) | T006 | ⬜ |
| T011 | f-string 词法 `$"…{expr}…"` | [P1-T011-lexer-fstring.md](P1-T011-lexer-fstring.md) | T010 | ⬜ |
| T012 | bytes 字面量 `b"…"` | [P1-T012-lexer-bytes-literals.md](P1-T012-lexer-bytes-literals.md) | T006 | ⬜ |
| T013 | 运算符与界符完整集 | [P1-T013-lexer-operators-delimiters.md](P1-T013-lexer-operators-delimiters.md) | T006 | ⬜ |
| T014 | 注释（`//`）与空白处理 | [P1-T014-lexer-comments-whitespace.md](P1-T014-lexer-comments-whitespace.md) | T006 | ⬜ |
| T015 | 自动分号插入（ASI）规则 | [P1-T015-lexer-asi.md](P1-T015-lexer-asi.md) | T006 | ⬜ |
| T016 | `tokens` 子命令 + 词法 golden 测试套件 | [P1-T016-lexer-tokens-command-golden.md](P1-T016-lexer-tokens-subcommand-golden.md) | T006–T015, T004 | ⬜ |

---

## P2 — 语法分析 / AST（T017–T036）

> 递归下降 + Pratt 解析器，覆盖全部语句与表达式；`parse` 子命令 + AST golden。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T017 | AST 节点定义与内存管理 | [P2-T017-ast-node-definitions.md](P2-T017-ast-node-definitions.md) | T005 | ⬜ |
| T018 | Pratt 解析器框架（优先级表、nud/led） | [P2-T018-parser-pratt-framework.md](P2-T018-parser-pratt-framework.md) | T016, T017 | ⬜ |
| T019 | 一元/二元/幂/位/比较/逻辑表达式 | [P2-T019-parser-arithmetic-expressions.md](P2-T019-parser-expressions-unary-binary.md) | T018 | ⬜ |
| T020 | 三目表达式 `a if cond else b` | [P2-T020-parser-ternary.md](P2-T020-parser-if-expr.md) | T018 | ⬜ |
| T021 | 调用/属性访问/下标/后缀（`++`/`--`） | [P2-T021-parser-calls-attrs-subscripts.md](P2-T021-parser-call-attr-index.md) | T018 | ⬜ |
| T022 | list/set/map 字面量消歧 | [P2-T022-parser-list-set-map-literals.md](P2-T022-parser-container-literals.md) | T018 | ⬜ |
| T023 | tuple 字面量 `(a, b, ...)` | [P2-T023-parser-tuple-literals.md](P2-T023-parser-tuple.md) | T018 | ⬜ |
| T024 | 函数字面量与匿名闭包 | [P2-T024-parser-func-literals-closures.md](P2-T024-parser-func-literal.md) | T018 | ⬜ |
| T025 | `make(chan)` 与 `<-ch` 接收表达式 | [P2-T025-parser-make-recv-exprs.md](P2-T025-parser-make-recv.md) | T018 | ⬜ |
| T026 | `var`/`:=`/赋值（复合/`++`/`--`）语句 | [P2-T026-parser-var-assign.md](P2-T026-parser-var-assign.md) | T018 | ⬜ |
| T027 | `if`/`else` 语句 | [P2-T027-parser-if-else.md](P2-T027-parser-if-else.md) | T018 | ⬜ |
| T028 | `for` 三种形式 + range 消歧 | [P2-T028-parser-for-loops.md](P2-T028-parser-for.md) | T018 | ⬜ |
| T029 | `switch`/`case`/`fallthrough`/`default` | [P2-T029-parser-switch.md](P2-T029-parser-switch.md) | T018 | ⬜ |
| T030 | `return`/`break`/`continue`/`pass`/`del` | [P2-T030-parser-flow-control-stmts.md](P2-T030-parser-jump-stmts.md) | T018 | ⬜ |
| T031 | `try`/`catch`/`finally`/`raise` | [P2-T031-parser-try-catch-finally.md](P2-T031-parser-try-raise.md) | T018 | ⬜ |
| T032 | `go` 语句与 `select` | [P2-T032-parser-go-select.md](P2-T032-parser-go-select.md) | T018 | ⬜ |
| T033 | `with` 上下文管理器 | [P2-T033-parser-with.md](P2-T033-parser-with.md) | T018 | ⬜ |
| T034 | `func`/`class`/方法声明 + ParamList（默认值/vararg/kwarg） | [P2-T034-parser-func-class-decls.md](P2-T034-parser-func-class-decl.md) | T018 | ⬜ |
| T035 | `import`/DottedName/`as` | [P2-T035-parser-import.md](P2-T035-parser-import.md) | T018 | ⬜ |
| T036 | Program 顶层 + `parse` 子命令 + AST golden | [P2-T036-parser-program-golden.md](P2-T036-parser-program-golden.md) | T017–T035, T004 | ⬜ |

---

## P3 — 编译器（T037–T048）

> 单遍 AST→字节码编译器；`MsChunk` 生成；`disasm` 子命令 + golden。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T037 | `MsChunk`：emit/常量池/行号表 | [P3-T037-compiler-chunk-emit.md](P3-T037-compiler-chunk.md) | T036 | ⬜ |
| T038 | 作用域/局部槽/符号表 | [P3-T038-compiler-scope-symbols.md](P3-T038-compiler-scope-symbols.md) | T037 | ⬜ |
| T039 | 表达式编译（算术/比较/短路 `and`/`or`） | [P3-T039-compiler-expressions.md](P3-T039-compiler-expressions.md) | T038 | ⬜ |
| T040 | 变量访问编译（local/global/upvalue） | [P3-T040-compiler-variable-access.md](P3-T040-compiler-variables.md) | T038 | ⬜ |
| T041 | 容器构建指令（list/map/tuple/set/slice） | [P3-T041-compiler-containers.md](P3-T041-compiler-containers.md) | T038 | ⬜ |
| T042 | 控制流编译 + 跳转回填（if/for/switch/break/continue） | [P3-T042-compiler-control-flow.md](P3-T042-compiler-control-flow.md) | T038 | ⬜ |
| T043 | 函数编译 + `MAKE_CLOSURE`/upvalue 解析 | [P3-T043-compiler-functions-closures.md](P3-T043-compiler-functions.md) | T038 | ⬜ |
| T044 | class 编译 + `MAKE_CLASS`/方法表 | [P3-T044-compiler-classes.md](P3-T044-compiler-class.md) | T038 | ⬜ |
| T045 | 调用编译（`CALL`/`CALL_EX`/`CALL_KW`/`CALL_ASYNC`） | [P3-T045-compiler-calls.md](P3-T045-compiler-calls.md) | T038 | ⬜ |
| T046 | 异常编译（`PUSH/POP_EXCEPT`/`RAISE`/finally 内联） | [P3-T046-compiler-exceptions.md](P3-T046-compiler-exceptions.md) | T038 | ⬜ |
| T047 | `with`/`del`/`assert` 编译 | [P3-T047-compiler-with-del-assert.md](P3-T047-compiler-with-del-assert.md) | T038 | ⬜ |
| T048 | `disasm` 反汇编器 + disasm golden 测试 | [P3-T048-compiler-disasm-golden.md](P3-T048-disasm-golden.md) | T037–T047, T004 | ⬜ |

---

## P4 — VM 求值 + 核心类型 + 简易 GC（T049–T067）

> 建立可运行的最小解释器：值模型、基础 GC、eval loop、核心类型；M1 里程碑。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T049 | `MsValue`/`MsObject`/`MsType` 完整定义 + tagged union | [P4-T049-value-object-type-definitions.md](P4-T049-value-object-type.md) | T005 | ⬜ |
| T050 | **简易 GC**：单线程 STW 标记-清除 + 基础分配器 | [P4-T050-gc-simple-mark-sweep.md](P4-T050-gc-simple-stw.md) | T049 | ⬜ |
| T051 | `MsFrame`/`MsThread` + 求值循环骨架 | [P4-T051-vm-frame-eval-loop.md](P4-T051-vm-eval-loop.md) | T048, T050 | ⬜ |
| T052 | 栈操作/常量加载/局部变量指令 | [P4-T052-vm-stack-ops-constants-locals.md](P4-T052-vm-stack-const-local.md) | T051 | ⬜ |
| T053 | int 算术（多态除法/溢出回绕/位运算） | [P4-T053-vm-int-arithmetic.md](P4-T053-vm-int-arithmetic.md) | T052 | ⬜ |
| T054 | float 算术（IEEE 754 语义） | [P4-T054-vm-float-arithmetic.md](P4-T054-vm-float-arithmetic.md) | T052 | ⬜ |
| T055 | bool/nil + 真值测试（`__bool__`） | [P4-T055-vm-bool-nil.md](P4-T055-vm-bool-nil.md) | T052 | ⬜ |
| T056 | 比较指令（`EQ`/`NE`/`LT`…）+ `is`/`in`/`not` | [P4-T056-vm-comparisons.md](P4-T056-vm-comparison.md) | T052 | ⬜ |
| T057 | `str`：UTF-8/不可变/索引/切片/迭代/hash | [P4-T057-vm-str.md](P4-T057-vm-str.md) | T052 | ⬜ |
| T058 | `bytes`：可变字节数组/下标赋值 | [P4-T058-vm-bytes.md](P4-T058-vm-bytes.md) | T052 | ⬜ |
| T059 | `list`：动态数组/方法/切片/`in` | [P4-T059-vm-list.md](P4-T059-vm-list.md) | T052 | ⬜ |
| T060 | `map`：开放寻址 hash map/键约束/NaN 禁用 | [P4-T060-vm-map.md](P4-T060-vm-map.md) | T052 | ⬜ |
| T061 | `tuple`：不可变序列/hashable | [P4-T061-vm-tuple.md](P4-T061-vm-tuple.md) | T052 | ⬜ |
| T062 | `set`：集合运算/关系比较/就地操作 | [P4-T062-vm-set.md](P4-T062-vm-set.md) | T052 | ⬜ |
| T063 | `frozenset`：不可变集合/hashable | [P4-T063-vm-frozenset.md](P4-T063-vm-frozenset.md) | T062 | ⬜ |
| T064 | `range` 惰性迭代器 | [P4-T064-vm-range.md](P4-T064-vm-range.md) | T052 | ⬜ |
| T065 | 迭代协议：`GET_ITER`/`FOR_ITER`/`StopIteration` | [P4-T065-vm-iteration-protocol.md](P4-T065-vm-iteration-protocol.md) | T052 | ⬜ |
| T066 | 属性/下标指令分派（类型槽 tpGetitem 等） | [P4-T066-vm-attrs-subscripts.md](P4-T066-vm-attr-index-dispatch.md) | T052 | ⬜ |
| T067 | **M1：`.ms` 端到端打通** + 基线 `.ms` 测试套件 | [P4-T067-vm-end-to-end.md](P4-T067-vm-e2e-m1.md) | T051–T066 | ⬜ |

---

## P5 — 函数 / 闭包 / class（T068–T078）

> 完整的调用约定、闭包 upvalue、class 系统、魔术方法、MRO；M2 里程碑。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T068 | 调用约定/参数绑定/默认值求值 | [P5-T068-func-call-convention.md](P5-T068-call-convention.md) | T067 | ⬜ |
| T069 | vararg（`...args`）收集 | [P5-T069-func-varargs.md](P5-T069-vararg.md) | T068 | ⬜ |
| T070 | kwarg（`**kwargs`）收集与关键字参数 | [P5-T070-func-kwargs.md](P5-T070-kwargs.md) | T068 | ⬜ |
| T071 | 闭包 upvalue open/close 运行期语义 | [P5-T071-func-closures-upvalues.md](P5-T071-closures-upvalue.md) | T068 | ⬜ |
| T072 | 实例化/`__init__`/实例属性 `self.x = …` | [P5-T072-class-instantiation-init.md](P5-T072-class-instantiation.md) | T067 | ⬜ |
| T073 | 方法绑定 + MRO 线性化查找 | [P5-T073-class-method-binding-mro.md](P5-T073-method-binding-mro.md) | T072 | ⬜ |
| T074 | 魔术方法分派（算术/比较/容器/迭代） | [P5-T074-class-magic-methods.md](P5-T074-magic-methods.md) | T073 | ⬜ |
| T075 | `super()` 代理对象 | [P5-T075-class-super.md](P5-T075-super.md) | T073 | ⬜ |
| T076 | 类属性 vs 实例属性（遮蔽规则） | [P5-T076-class-attrs.md](P5-T076-class-attrs.md) | T072 | ⬜ |
| T077 | `__call__`/可调用对象 + `CALLABLE` 检查 | [P5-T077-class-callable.md](P5-T077-callable.md) | T073 | ⬜ |
| T078 | **M2**：`isinstance`/`type` + `ISINSTANCE` 指令 + M2 `.ms` 测试 | [P5-T078-class-isinstance-type.md](P5-T078-isinstance-m2.md) | T072–T077 | ⬜ |

---

## P6 — 异常（T079–T085）

> 异常层次、raise/reraise、处理器栈展开、finally 多路径、traceback 打印。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T079 | 异常类层次构建（`BaseException` → `Exception` → 具体类） | [P6-T079-exc-hierarchy.md](P6-T079-exception-hierarchy.md) | T078 | ⬜ |
| T080 | `raise`/`reraise` + `MS_ERROR_VALUE` 传播 | [P6-T080-exc-raise-propagation.md](P6-T080-raise-reraise.md) | T079 | ⬜ |
| T081 | 处理器栈展开 + `catch` 类型匹配 | [P6-T081-exc-handler-unwinding.md](P6-T081-exception-unwinding.md) | T080 | ⬜ |
| T082 | `finally` 多路径语义（正常/return/break/continue/异常） | [P6-T082-exc-finally.md](P6-T082-finally.md) | T081 | ⬜ |
| T083 | traceback 记录（`MsTraceback`）与回溯打印 | [P6-T083-exc-traceback.md](P6-T083-traceback.md) | T081 | ⬜ |
| T084 | `assert` 运行期（`RAISE_ASSERT` 指令） | [P6-T084-exc-assert.md](P6-T084-assert-runtime.md) | T080 | ⬜ |
| T085 | 自定义异常 + 异常完整 `.ms` 测试套件 | [P6-T085-exc-ms-tests.md](P6-T085-custom-exception-tests.md) | T079–T084 | ⬜ |

---

## P7 — 模块 + 字节码缓存（T086–T095）

> import 解析、`MsModule`、包加载、`.msc` marshal/unmarshal、失效策略；M3 里程碑。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T086 | `MsModule` 对象 + globals map | [P7-T086-module-msmodule.md](P7-T086-module.md) | T078 | ⬜ |
| T087 | import 解析（绝对/相对/点号→路径，优先级） | [P7-T087-module-import-resolution.md](P7-T087-import-resolution.md) | T086 | ⬜ |
| T088 | 模块缓存 `vm->modules` + 循环导入检测 | [P7-T088-module-cache-circular.md](P7-T088-module-cache.md) | T087 | ⬜ |
| T089 | 包（`__init__.ms`）与子模块加载 | [P7-T089-module-packages.md](P7-T089-packages.md) | T088 | ⬜ |
| T090 | `MSLANG_PATH` + 内置模块注册 (`msRegisterBuiltinModule`) | [P7-T090-module-mslang-path-builtins.md](P7-T090-mslang-path.md) | T088 | ⬜ |
| T091 | `.msc` marshal 写（序列化 `MsFunction` 树） | [P7-T091-cache-marshal-write.md](P7-T091-msc-marshal-write.md) | T086 | ⬜ |
| T092 | `.msc` unmarshal 读 + 文件头校验（magic/version） | [P7-T092-cache-unmarshal-read.md](P7-T092-msc-marshal-read.md) | T091 | ⬜ |
| T093 | 缓存失效（mtime+size / hash 两种模式）+ 原子写入 | [P7-T093-cache-invalidation-atomic.md](P7-T093-cache-invalidation.md) | T092 | ⬜ |
| T094 | `mslang compile`（compileall）子命令 | [P7-T094-cache-compile-command.md](P7-T094-compile-cmd.md) | T093, T004 | ⬜ |
| T095 | **M3**：模块+缓存端到端 `.ms` 测试套件 | [P7-T095-cache-ms-tests.md](P7-T095-module-e2e-m3.md) | T086–T094 | ⬜ |

---

## P8 — 内置函数（T096–T105）

> 实现全部无需 import 的内置函数；M4 里程碑。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T096 | `print`/`input`/`len`/`type`/`repr`/`str` | [P8-T096-builtin-print-str-type.md](P8-T096-builtin-print-len-type.md) | T067 | ⬜ |
| T097 | `int`/`float`/`bool` 转换函数 | [P8-T097-builtin-numeric-conversions.md](P8-T097-builtin-int-float-bool.md) | T067 | ⬜ |
| T098 | `range`/`enumerate`/`zip`/`map`/`filter`（惰性迭代器） | [P8-T098-builtin-lazy-iterators.md](P8-T098-builtin-range-iter.md) | T065 | ⬜ |
| T099 | `sorted`/`reversed`/`sum`/`min`/`max` | [P8-T099-builtin-sorted-min-max.md](P8-T099-builtin-sorted-minmax.md) | T067 | ⬜ |
| T100 | `abs`/`round`/`pow`（含模幂）/`divmod` | [P8-T100-builtin-math-builtins.md](P8-T100-builtin-math.md) | T067 | ⬜ |
| T101 | `any`/`all`/`iter`/`next`/`callable`/`hash`/`id` | [P8-T101-builtin-iter-callable.md](P8-T101-builtin-iter-util.md) | T065 | ⬜ |
| T102 | `chr`/`ord`/`hex`/`oct`/`bin`/`format` | [P8-T102-builtin-chr-ord-format.md](P8-T102-builtin-chr-ord-format.md) | T067 | ⬜ |
| T103 | `set`/`frozenset`/`bytes`/`bytearray` 构造函数 | [P8-T103-builtin-set-bytes-constructors.md](P8-T103-builtin-constructors.md) | T062, T063, T058 | ⬜ |
| T104 | `vars`/`dir`/`open`（文件 I/O 前置接口） | [P8-T104-builtin-vars-dir-open.md](P8-T104-builtin-vars-dir-open.md) | T067 | ⬜ |
| T105 | **M4**：内置函数完整 `.ms` 测试套件 | [P8-T105-builtin-ms-tests.md](P8-T105-builtin-e2e-m4.md) | T096–T104 | ⬜ |

---

## P9 — 并发（T106–T114）

> 先单线程协作调度器，再演进到 M:N work-stealing；go/channel/select/async/await；M5 里程碑。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T106 | `MsCoroutine` + **单线程协作调度器**（演进基线） | [P9-T106-concurrency-scheduler-basic.md](P9-T106-coroutine-scheduler.md) | T067 | ⬜ |
| T107 | `go` 语句运行期（`GO` 指令派发） | [P9-T107-concurrency-go-stmt.md](P9-T107-go-stmt.md) | T106 | ⬜ |
| T108 | channel 无缓冲（rendezvous 语义） | [P9-T108-concurrency-channel-unbuffered.md](P9-T108-channel-unbuffered.md) | T106 | ⬜ |
| T109 | channel 有缓冲/`close`/迭代 | [P9-T109-concurrency-channel-buffered.md](P9-T109-channel-buffered.md) | T108 | ⬜ |
| T110 | `select` 语句实现 | [P9-T110-concurrency-select.md](P9-T110-select.md) | T109 | ⬜ |
| T111 | `MsFuture`/`async func`/`await` | [P9-T111-concurrency-async-await.md](P9-T111-async-await.md) | T106 | ⬜ |
| T112 | **调度器演进**：M:N 多 Worker + work-stealing | [P9-T112-concurrency-scheduler-mn.md](P9-T112-mn-scheduler.md) | T106 | ⬜ |
| T113 | 安全点与协作抢占（GC/调度器交互） | [P9-T113-concurrency-safepoints.md](P9-T113-safepoint-preemption.md) | T112 | ⬜ |
| T114 | **M5**：并发 `.ms` 测试（生产者消费者/select 超时/async chain） | [P9-T114-concurrency-ms-tests.md](P9-T114-concurrency-e2e-m5.md) | T106–T113 | ⬜ |

---

## P10 — GC 演进（T115–T125）

> 从简易 STW 演进到三代分代 GC + 并发标记 + 写屏障；M6 里程碑。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T115 | 对象头分代位/`gcFlags` + 年轻代 bump/TLAB | [P10-T115-gc-generational-flags.md](P10-T115-generational-header.md) | T050 | ⬜ |
| T116 | Minor GC：半区复制（Cheney BFS）+ 转发指针 | [P10-T116-gc-minor-cheney.md](P10-T116-minor-gc-cheney.md) | T115 | ⬜ |
| T117 | 精确根枚举（VM 栈帧/全局/C API 句柄） | [P10-T117-gc-root-enumeration.md](P10-T117-root-enumeration.md) | T116 | ⬜ |
| T118 | 分代写屏障 + card table + remembered set | [P10-T118-gc-write-barrier-card-table.md](P10-T118-write-barrier.md) | T116 | ⬜ |
| T119 | 中代标记-清除 + 晋升（Middle GC） | [P10-T119-gc-middle-generation.md](P10-T119-middle-gen.md) | T118 | ⬜ |
| T120 | 老年代增量/并发三色标记 + Dijkstra 写屏障 | [P10-T120-gc-old-generation-concurrent.md](P10-T120-old-gen-concurrent.md) | T119 | ⬜ |
| T121 | 并行清扫（多 OS 线程分区无锁清扫） | [P10-T121-gc-parallel-sweep.md](P10-T121-parallel-sweep.md) | T120 | ⬜ |
| T122 | 大对象区（`mmap`/`VirtualAlloc`，`>=32KB`） | [P10-T122-gc-large-objects.md](P10-T122-large-object.md) | T116 | ⬜ |
| T123 | `__del__` 终结队列 + 复活语义 | [P10-T123-gc-finalizers.md](P10-T123-finalizer.md) | T120 | ⬜ |
| T124 | `gc` 内置模块（`collect`/`disable`/`stats`） | [P10-T124-gc-stdlib-module.md](P10-T124-gc-module.md) | T120 | ⬜ |
| T125 | **M6**：GC 压力 `.ms` 测试 + benchmark | [P10-T125-gc-pressure-tests.md](P10-T125-gc-pressure-m6.md) | T115–T124 | ⬜ |

---

## P11 — C API（T126–T132）

> 嵌入 API、扩展模块 API、句柄/根表、值 API、MsType 注册；M7 里程碑。

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T126 | 句柄/根表/本地帧（`MsHandle`/`msPushLocalFrame`） | [P11-T126-capi-handles-roots.md](P11-T126-handle-root.md) | T050 | ⬜ |
| T127 | 嵌入 API（`msNew`/`msRunFile`/`msRunString`/全局读写） | [P11-T127-capi-embedding.md](P11-T127-embed-api.md) | T126 | ⬜ |
| T128 | 值 API（构造/字符串/list/map/属性/类型检查） | [P11-T128-capi-value-api.md](P11-T128-value-api.md) | T127 | ⬜ |
| T129 | 错误处理 API + 内置异常指针（`msExcTypeError`…） | [P11-T129-capi-error-handling.md](P11-T129-error-api.md) | T127 | ⬜ |
| T130 | 扩展模块 API（`MsCFunction`/方法表/`msNewModule`/`msAdd*`） | [P11-T130-capi-extension-module.md](P11-T130-extension-module-api.md) | T128 | ⬜ |
| T131 | `MsType` 注册 API（自定义 C 类型/槽设置） | [P11-T131-capi-type-registration.md](P11-T131-type-register-api.md) | T130 | ⬜ |
| T132 | **M7**：嵌入+扩展完整示例 + C/`.ms` 集成测试 | [P11-T132-capi-example-tests.md](P11-T132-embed-example-m7.md) | T126–T131 | ⬜ |

---

## P12 — 标准库（T133–T201）

> 全部 50+ 标准库模块，全部零外部依赖自实现；M8 里程碑。大模块拆多子任务，小模块合并为 1 任务。

### 基础层

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T133 | `sys`：argv/path/version/exit/stdin-stdout-stderr | [P12-T133-stdlib-sys.md](P12-T133-stdlib-sys.md) | T090 | ⬜ |
| T134 | `io`：I/O 抽象基类/File/BinaryIO/StringIO/异步 I/O | [P12-T134-stdlib-io.md](P12-T134-stdlib-io.md) | T104 | ⬜ |
| T135 | `os`（1/3）：`os.path` — 路径操作 join/split/exists/dirname… | [P12-T135-stdlib-os-path.md](P12-T135-stdlib-os-path.md) | T090 | ⬜ |
| T136 | `os`（2/3）：文件系统 — listdir/mkdir/remove/rename/stat/walk | [P12-T136-stdlib-os-fs.md](P12-T136-stdlib-os-fs.md) | T135 | ⬜ |
| T137 | `os`（3/3）：环境变量/进程 ID/getcwd/chdir/urandom | [P12-T137-stdlib-os-process.md](P12-T137-stdlib-os-process.md) | T135 | ⬜ |
| T138 | `math`（1/2）：常量（pi/e/inf/nan）+ 基础函数（floor/ceil/sqrt/log/exp…） | [P12-T138-stdlib-math-basic.md](P12-T138-stdlib-math-basic.md) | T090 | ⬜ |
| T139 | `math`（2/2）：三角/反三角/双曲 + gamma/erf/lgamma + isfinite/isnan | [P12-T139-stdlib-math-trig.md](P12-T139-stdlib-math-trig.md) | T138 | ⬜ |
| T140 | `strings`（1/2）：find/contains/hasPrefix/hasSuffix/split/join/count/index | [P12-T140-stdlib-strings-search.md](P12-T140-stdlib-strings-search.md) | T090 | ⬜ |
| T141 | `strings`（2/2）：replace/trim/strip/fields/repeat/title/toLower/toUpper/Builder | [P12-T141-stdlib-strings-replace.md](P12-T141-stdlib-strings-replace.md) | T140 | ⬜ |
| T142 | `time`：时钟/sleep/perfCounter + `time.after`（调度扩展） | [P12-T142-stdlib-time.md](P12-T142-stdlib-time.md) | T113 | ⬜ |
| T143 | `fmt`：Sprintf/Printf/Fprintf/Errorf/Sscanf | [P12-T143-stdlib-fmt.md](P12-T143-stdlib-fmt.md) | T090 | ⬜ |

### 数据结构

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T144 | `collections`（1/2）：`deque`（环形缓冲）+ `Counter`（__missing__ 返回 0） | [P12-T144-stdlib-collections-1.md](P12-T144-stdlib-collections-1.md) | T090 | ⬜ |
| T145 | `collections`（2/2）：`defaultdict` + `OrderedDict` + `namedtuple` | [P12-T145-stdlib-collections-2.md](P12-T145-stdlib-collections-2.md) | T144 | ⬜ |
| T146 | `array` + `bisect`：同类型紧凑数组（typecode/raw bytes）+ 二分查找 | [P12-T146-stdlib-array-bisect.md](P12-T146-stdlib-array-bisect.md) | T090 | ⬜ |
| T147 | `heapq` + `queue`：sift_up/sift_down 堆操作 + 线程安全队列（Mutex+条件通道） | [P12-T147-stdlib-heapq-queue.md](P12-T147-stdlib-heapq-queue.md) | T144, T182 | ⬜ |
| T148 | `sort`：Timsort（MIN_MERGE=32/gallop 模式）+ 整数/浮点/字符串特化快速路径 | [P12-T148-stdlib-sort.md](P12-T148-stdlib-sort.md) | T090 | ⬜ |

### 函数式编程

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T149 | `itertools`（1/3）：无限迭代器（count/cycle/repeat）+ islice | [P12-T149-stdlib-itertools-1.md](P12-T149-stdlib-itertools-1.md) | T090 | ⬜ |
| T150 | `itertools`（2/3）：组合工具（product/permutations/combinations/combinations_with_replacement） | [P12-T150-stdlib-itertools-2.md](P12-T150-stdlib-itertools-2.md) | T149 | ⬜ |
| T151 | `itertools`（3/3）：groupby/chain.from_iterable/accumulate/tee/batched | [P12-T151-stdlib-itertools-3.md](P12-T151-stdlib-itertools-3.md) | T149 | ⬜ |
| T152 | `functools`：partial/lru_cache/cached_property/reduce/wraps/cmp_to_key | [P12-T152-stdlib-functools.md](P12-T152-stdlib-functools.md) | T090 | ⬜ |

### 文本与编码

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T153 | `textwrap` + `csv`：文本换行/缩进/dedent + CSV FSM 解析器（RFC 4180） | [P12-T153-stdlib-textwrap-csv.md](P12-T153-stdlib-textwrap-csv.md) | T134 | ⬜ |
| T154 | `re`（1/3）：Thompson NFA 编译核心 + match/fullmatch + MsMatchObj | [P12-T154-stdlib-re-1.md](P12-T154-stdlib-re-1.md) | T090 | ⬜ |
| T155 | `re`（2/3）：search/findall/finditer + 命名分组 (?P<name>) + lookahead/lookbehind | [P12-T155-stdlib-re-2.md](P12-T155-stdlib-re-2.md) | T154 | ⬜ |
| T156 | `re`（3/3）：sub/subn/split + VERBOSE 模式 + 反向引用 + MATCH_STEP_LIMIT 防爆 | [P12-T156-stdlib-re-3.md](P12-T156-stdlib-re-3.md) | T155 | ⬜ |
| T157 | `base64` + `struct`：base64 b16/b32/b64/urlsafe 编解码 + 二进制格式 pack/unpack | [P12-T157-stdlib-base64-struct.md](P12-T157-stdlib-base64-struct.md) | T090 | ⬜ |

### 数值与统计

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T158 | `random`：MT19937（624 uint32_t 状态）+ random/randint/gauss/shuffle/getstate | [P12-T158-stdlib-random.md](P12-T158-stdlib-random.md) | T090 | ⬜ |
| T159 | `statistics`：mean/median/stdev/variance/mode/quantiles/NormalDist + Welford 在线算法 | [P12-T159-stdlib-statistics.md](P12-T159-stdlib-statistics.md) | T162 | ⬜ |
| T160 | `decimal`（1/2）：MsDecimalObj base-10^9 limb 数组 + 四则运算 + Context（精度/舍入） | [P12-T160-stdlib-decimal-1.md](P12-T160-stdlib-decimal-1.md) | T090 | ⬜ |
| T161 | `decimal`（2/2）：sqrt（Newton-Raphson）/ln/pi（Machin）+ 舍入模式 + localcontext | [P12-T161-stdlib-decimal-2.md](P12-T161-stdlib-decimal-2.md) | T160 | ⬜ |
| T162 | `fractions`：Fraction + GCD 规范化 + limit_denominator（Stern-Brocot）+ __int128 溢出保护 | [P12-T162-stdlib-fractions.md](P12-T162-stdlib-fractions.md) | T090 | ⬜ |

### 哈希与加密（全自实现）

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T163 | `hashlib`（1/3）：自实现 MD5（RFC 1321）+ SHA-1（RFC 3174）+ update/digest 接口 | [P12-T163-stdlib-hashlib-1.md](P12-T163-stdlib-hashlib-1.md) | T090 | ⬜ |
| T164 | `hashlib`（2/3）：自实现 SHA-256 + SHA-224（FIPS 180-4，Ch/Maj/Σ/σ 函数） | [P12-T164-stdlib-hashlib-2.md](P12-T164-stdlib-hashlib-2.md) | T163 | ⬜ |
| T165 | `hashlib`（3/3）：自实现 SHA-3（Keccak-p[1600,24]，θρπχι）+ BLAKE2b/2s + SHAKE XOF | [P12-T165-stdlib-hashlib-3.md](P12-T165-stdlib-hashlib-3.md) | T163 | ⬜ |
| T166 | `hmac` + `secrets`：HMAC-RFC2104 + compare_digest 常数时间比较 + BCryptGenRandom/getrandom | [P12-T166-stdlib-hmac-secrets.md](P12-T166-stdlib-hmac-secrets.md) | T163 | ⬜ |

### 压缩与归档（全自实现）

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T167 | `gzip`：自实现 DEFLATE（hash-chain LZ77 + 动态 Huffman）+ CRC-32 + gzip 格式读写 | [P12-T167-stdlib-gzip.md](P12-T167-stdlib-gzip.md) | T134 | ⬜ |
| T168 | `zipfile`：ZIP Local/Central Directory 格式（sig 校验）+ STORED/DEFLATED 模式 | [P12-T168-stdlib-zipfile.md](P12-T168-stdlib-zipfile.md) | T167 | ⬜ |
| T169 | `tarfile`：POSIX ustar 512-byte header + 八进制编码 + GNU LONGNAME + .tar.gz 管道 | [P12-T169-stdlib-tarfile.md](P12-T169-stdlib-tarfile.md) | T167 | ⬜ |

### 日期与时间

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T170 | `datetime`（1/3）：MsDateObj + MsTimeObj + toordinal 序数算法 + strftime | [P12-T170-stdlib-datetime-1.md](P12-T170-stdlib-datetime-1.md) | T090 | ⬜ |
| T171 | `datetime`（2/3）：MsTimeDeltaObj 规范化 + MsDateTimeObj + timestamp + strptime | [P12-T171-stdlib-datetime-2.md](P12-T171-stdlib-datetime-2.md) | T170 | ⬜ |
| T172 | `datetime`（3/3）：MsTzInfoObj + astimezone + ISO 8601 Z/±HH:MM + fold=0/1 夏令时 | [P12-T172-stdlib-datetime-3.md](P12-T172-stdlib-datetime-3.md) | T171 | ⬜ |
| T173 | `calendar`：isleap/monthrange/monthcalendar/itermonthdates + 文本日历格式化 | [P12-T173-stdlib-calendar.md](P12-T173-stdlib-calendar.md) | T170 | ⬜ |

### 操作系统与进程

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T174 | `subprocess`：Popen/run + POSIX fork/exec + Windows CreateProcess + communicate 超时 | [P12-T174-stdlib-subprocess.md](P12-T174-stdlib-subprocess.md) | T090 | ⬜ |
| T175 | `signal` + `shutil` + `tempfile`：sigaction/原子标志 + 文件复制/rmtree + mkstemp/TemporaryDirectory | [P12-T175-stdlib-signal-shutil-tempfile.md](P12-T175-stdlib-signal-shutil-tempfile.md) | T136 | ⬜ |

### 网络（全自实现协议）

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T176 | `socket`：MsSocketObj + 非阻塞 fd（O_NONBLOCK）+ epoll/kqueue/IOCP + 协程 EAGAIN yield | [P12-T176-stdlib-socket.md](P12-T176-stdlib-socket.md) | T090 | ⬜ |
| T177 | `net`：MsConnObj dial/listen/accept + deadline + off-thread getaddrinfo + channel 回传 | [P12-T177-stdlib-net.md](P12-T177-stdlib-net.md) | T176 | ⬜ |
| T178 | `url`：RFC 3986 FSM 解析 + quote/urlencode/parse_qs + urljoin §5.2.2 相对解析 | [P12-T178-stdlib-url.md](P12-T178-stdlib-url.md) | T090 | ⬜ |
| T179 | `http`（1/3）：HTTP/1.1 客户端 + chunked 解码 + Keep-Alive 连接池 + Session + 重定向 | [P12-T179-stdlib-http-client.md](P12-T179-stdlib-http-client.md) | T177, T178 | ⬜ |
| T180 | `http`（2/3）：HTTP 服务端 + Trie 路由（:param）+ 中间件洋葱链 + 静态文件 ETag/304 | [P12-T180-stdlib-http-server.md](P12-T180-stdlib-http-server.md) | T179 | ⬜ |
| T181 | `http`（3/3）：WebSocket（RFC 6455/SHA-1 握手/帧格式）+ SSE（text/event-stream） | [P12-T181-stdlib-http-async.md](P12-T181-stdlib-http-async.md) | T180, T111 | ⬜ |

### 并发工具

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T182 | `sync`（1/2）：MsMutexObj（CAS+协程等待队列）+ RWMutex + WaitGroup + Cond | [P12-T182-stdlib-sync-1.md](P12-T182-stdlib-sync-1.md) | T112 | ⬜ |
| T183 | `sync`（2/2）+ `context`：Once/AtomicInt + MsContextObj cancel/deadline/value 传播链 | [P12-T183-stdlib-sync-2-context.md](P12-T183-stdlib-sync-2-context.md) | T182, T112 | ⬜ |

### 序列化

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T184 | `json`（1/2）：ms 对象 → JSON 编码器 + ensure_ascii/\uXXXX + 循环引用 id set | [P12-T184-stdlib-json-encoder.md](P12-T184-stdlib-json-encoder.md) | T090 | ⬜ |
| T185 | `json`（2/2）：JSON → ms 对象 递归下降解码器 + surrogate pair + object_hook | [P12-T185-stdlib-json-decoder.md](P12-T185-stdlib-json-decoder.md) | T184 | ⬜ |

### 工具

| 编号 | 任务 | 文件 | 依赖 | 状态 |
|---|---|---|---|---|
| T186 | `logging`：Logger 树（"." 分层）/Handler/Formatter + propagate + basicConfig | [P12-T186-stdlib-logging.md](P12-T186-stdlib-logging.md) | T090 | ⬜ |
| T187 | `argparse`：ArgumentParser/add_argument/parse_args + nargs/choices/subparsers/互斥组 | [P12-T187-stdlib-argparse.md](P12-T187-stdlib-argparse.md) | T090 | ⬜ |
| T188 | `testing`：test_*.ms 发现 + B.N 基准自动校准 + TestCase setUp/tearDown + CLI 集成 | [P12-T188-stdlib-testing.md](P12-T188-stdlib-testing.md) | T090 | ⬜ |
| T189 | `uuid` + `enum` + `copy`：UUID v1/v3/v4/v5 + Enum 元类拦截 + 浅/深拷贝（memo 循环检测） | [P12-T189-stdlib-uuid-enum-copy.md](P12-T189-stdlib-uuid-enum-copy.md) | T090 | ⬜ |
| T190 | `pathlib`：MsPathObj + `/` 运算符 + glob（** 递归）+ iterdir/read_text/resolve | [P12-T190-stdlib-pathlib.md](P12-T190-stdlib-pathlib.md) | T135, T136 | ⬜ |
| T191 | `contextlib`：@contextmanager（生成器协议）+ suppress + redirect_stdout + ExitStack | [P12-T191-stdlib-contextlib.md](P12-T191-stdlib-contextlib.md) | T090 | ⬜ |
| T192 | `threading`：Thread/join/Event/local（MsThread.locals）+ Barrier（基于协程） | [P12-T192-stdlib-threading.md](P12-T192-stdlib-threading.md) | T106 | ⬜ |
| T193 | `abc`：ABCMeta.__new__ + __abstractmethods__ set + @abstractmethod + __subclasshook__ | [P12-T193-stdlib-abc.md](P12-T193-stdlib-abc.md) | T072 | ⬜ |
| T194 | `weakref`：MsWeakRefObj + GC sweep 清除 + callback + WeakValueDictionary + finalize | [P12-T194-stdlib-weakref.md](P12-T194-stdlib-weakref.md) | T115 | ⬜ |
| T195 | `io`（扩展）：MsBytesIOObj + getbuffer/seek + BufferedReader peek + TextIOWrapper 换行转换 | [P12-T195-stdlib-io-extended.md](P12-T195-stdlib-io-extended.md) | T134 | ⬜ |
| T196 | `pprint`：_format() 递归 + 行宽启发式 + 深度限制 "..." + compact 模式 + 循环 id set | [P12-T196-stdlib-pprint.md](P12-T196-stdlib-pprint.md) | T090 | ⬜ |
| T197 | `traceback`：StackSummary/FrameSummary + format_exception 链式异常 + 源行缓存 | [P12-T197-stdlib-traceback.md](P12-T197-stdlib-traceback.md) | T083 | ⬜ |
| T198 | `warnings`：过滤器链（default/error/ignore/once）+ catch_warnings + record=True | [P12-T198-stdlib-warnings.md](P12-T198-stdlib-warnings.md) | T197 | ⬜ |
| T199 | `platform`：system/machine/uname + freedesktop_os_release + win32_ver/mac_ver | [P12-T199-stdlib-platform.md](P12-T199-stdlib-platform.md) | T133 | ⬜ |
| T200 | `locale`：setlocale/localeconv/format_string/currency/strcoll（封装 POSIX/Windows CRT） | [P12-T200-stdlib-locale.md](P12-T200-stdlib-locale.md) | T141 | ⬜ |
| T201 | **M8**：stdlib 综合端到端测试（5 场景）+ benchmark 套件（json/hashlib/re/sort/Counter） | [P12-T201-stdlib-e2e-m8.md](P12-T201-stdlib-e2e-m8.md) | T133–T200 | ⬜ |

---

## 统计摘要

| 阶段 | 任务数 | 任务号范围 |
|---|---|---|
| P0 工程地基 | 5 | T001–T005 |
| P1 词法 | 11 | T006–T016 |
| P2 语法/AST | 20 | T017–T036 |
| P3 编译器 | 12 | T037–T048 |
| P4 VM+核心类型+简易GC | 19 | T049–T067 |
| P5 函数/闭包/class | 11 | T068–T078 |
| P6 异常 | 7 | T079–T085 |
| P7 模块+字节码缓存 | 10 | T086–T095 |
| P8 内置函数 | 10 | T096–T105 |
| P9 并发 | 9 | T106–T114 |
| P10 GC 演进 | 11 | T115–T125 |
| P11 C API | 7 | T126–T132 |
| P12 stdlib | 69 | T133–T201 |
| **合计** | **201** | **T001–T201** |

---

## 测试与 Benchmark 约定

### C 单测
- 框架：`tests/ms_test.h`（P0-T003，零外部依赖，`MS_ASSERT_EQ`/`MS_ASSERT_STR_EQ`/`MS_RUN`/`msTestSummary`）。
- 目录：`tests/<子系统>/test_<名>.c`，如 `tests/lexer/test_int_literal.c`。
- golden 文件：`tests/golden/<名>/input.txt` → `<名>.expected`，CTest 自动比对。
- 触发：`cmake --build build && ctest -R <filter>`。

### `.ms` 脚本测试
- VM 可用（P4-T067）后，`.ms` 测试文件位于 `tests/ms/<feature>/test_<name>.ms`。
- 期望输出文件 `test_<name>.expected`（标准输出，行级精确匹配）。
- 运行：`mslang run tests/ms/<feature>/test_<name>.ms`，输出 diff 与 expected。

### Benchmark
- 目录：`benchmarks/<子系统>/bench_<名>.{c,ms}`。
- VM 前（P0–P3）：C microbench（tokens/sec、parse nodes/sec），`time.h`/`clock_gettime` 计时。
- VM 后（P4 起）：`.ms` microbench，`import time; time.perfCounter()` 计时，关键处给出 CPython 对比方法（同等功能 Python 脚本对比）。
- 无意义处标 `N/A`。

### 验收自动化

每个任务的「验收标准（checklist）」节支持机器可读的 `<!-- v:... -->` 标签，由
`tests/ci/verify_task.py` 驱动自动验证并回填 `[x]`。

**运行方式：**

```bash
# 验证（仅输出表格，不改文件）
python tests/ci/verify_task.py T002

# 验证并回填 [x]，全部通过时状态翻 ✅
python tests/ci/verify_task.py T002 --apply

# 指定自定义构建目录
python tests/ci/verify_task.py T002 --build-dir build --rel-dir build_rel --apply
```

**标签词表：**

| 标签 | 触发条件 |
|---|---|
| `<!-- v:build -->` | Debug + Release 构建均成功（`-Werror` 保证无警告） |
| `<!-- v:ctest:<name> -->` | CTest 测试 `<name>` 通过（`ctest -L Txxx`） |
| `<!-- v:golden:<name> -->` | golden 文件比对测试通过（`ms_add_golden_test` 注册的 ctest 条目） |
| `<!-- v:ms:<name> -->` | `.ms` 脚本测试通过（`ms_add_ms_test` 注册的 ctest 条目，P4-T067 后） |
| `<!-- v:manual:<原因> -->` | 无法自动验证；仅打印提示，**不**自动勾选 |
| 无标签 | 视为 UNVERIFIED，不自动勾选 |

**CMake helper 与任务标签：**
- `ms_add_test(T002 name src)` — 不链接 core，自动打 `LABELS T002`，继承 `-Werror`
- `ms_add_test_with_core(T002 name src)` — 链接 mslang_core
- `ms_add_golden_test(T002 name cmd input expected)` — golden 比对
- `ms_add_ms_test(T002 name script expected)` — .ms 脚本测试（P4-T067 后）
- `ms_add_symbol_absent_test(T002 name target symbol)` — Release 符号缺失检查

**增量落地：** 标签随任务实现或 review-fix 时同步补齐；无标签行 verify_task.py 报 UNVERIFIED、
不阻塞其他已标注项的勾选，方案对 201 个任务文件完全向后兼容。

---

## 贡献指引

1. 认领任务：将索引中该行 emoji 改为 🚧，在任务文件顶部注明认领人与日期。
2. 完成任务：全部 checklist 打勾、测试通过后，将 emoji 改为 ✅。
3. 阻塞：改为 ⏸️ 并在任务文件「风险与边界」节说明原因与解除条件。
4. 新增子任务：按末尾最大编号续增，更新本索引，PR 描述说明原因。
5. 模板：复制 [`_template.md`](_template.md) 开始新任务文档。
