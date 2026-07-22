# .ms 测试指南

本文档规定 `tests/ms/` 下 `.ms` 端到端测试脚本的目录结构、命名、注册与 checklist 关联约定，
以及新任务落地时同步补充 `.ms` 测试的流程。适用于 `mslang <script>` 可执行之后（P4-T067 起）
的所有里程碑。

---

## 1. 目录结构

按**里程碑**组织，与 `docs/language/impl/README.md` 的 M1/M2/M3… 里程碑划分一致：

```
tests/ms/
  m1/     # M1 里程碑：P4-T067 起，基础类型 + 控制流 + 内置函数
  m2/     # M2 里程碑：P5-T068 起，调用约定/默认参数/递归
  ...
```

不按任务号或特性分目录——同一里程碑内的特性用**文件名**区分（见 §2），避免目录层级过深。

---

## 2. 命名约定

- 脚本：`<feature>.ms`；期望输出：`<feature>.expected`（两者一一对应，放同一目录）。
- `<feature>` 用小写 + 下划线，描述被测特性而非任务号（如 `list_ops.ms`，不是 `t059.ms`）。
- 每个 `.ms` 文件顶部用一行 `//` 注释注明覆盖点与依据任务号，例如：
  ```ms
  // list: literal/index/slice/set/concat/repeat/len/in/nested (T059)
  ```
- `.expected` 必须由**实际运行** `mslang <script>` 生成/核对，不得手写臆造（stdout 需与解释器行为
  逐字节一致，包括数值格式化、精度、末尾换行）。

---

## 3. 注册到 CMake（保证 `verify.bat` 能触发）

在 `tests/CMakeLists.txt` 用 `ms_add_ms_test(<label> <ctest名> <script> <expected>)` 注册：

```cmake
ms_add_ms_test(T067 ms_m1_list_ops ${_GM1}/list_ops.ms ${_GM1}/list_ops.expected)
```

- `<label>`：该里程碑的 **e2e 宿主任务号**（M1 → `T067`，即 `P4-T067-vm-e2e-m1`）。
  `verify_task.py` 的任务标签只识别 `T<nnn>` 形式，不识别 `M<n>`，故同一里程碑的所有 `.ms`
  用例统一挂宿主任务的 label，`verify.bat T067` 即可跑通整个 M1 `.ms` 套件。
- `<ctest名>`：`ms_<milestone>_<feature>`，如 `ms_m1_list_ops`。
- 用例注册后自动被 `ctest -L <label>` 与 `verify.bat <label>` 拾取，无需额外接线。

---

## 4. Checklist 关联（`<!-- v:ms:... -->`）

`verify_task.py` 按 `<!-- v:ms:<ctest名> -->` 标签将任务文档的 checklist 行与 ctest 结果关联
（`ms` 是 `ctest` 的别名，语义相同）。**一行 checklist 只认第一个标签**（`V_TAG` 用
`re.search`，只取首个匹配），故每个 ctest 用例必须有**独立一行**：

```markdown
- [ ] M1 list 特性（`list_ops.ms`）golden 通过。<!-- v:ms:ms_m1_list_ops -->
```

不要把多个 ctest 名塞进一行——后续标签会被忽略，导致该用例永远显示 UNVERIFIED。

---

## 5. 断言风格

### 5.1 golden stdout（当前唯一可用方式）

`print(...)` 输出与 `.expected` 文件**精确匹配**（`tests/golden_runner.py` 驱动），
是目前**唯一**可行的断言方式。

### 5.2 assert 风格（P6-T084 落地后启用）

`assert` 语句已可编译（`OP_RAISE_ASSERT`），但 VM 求值循环**尚未派发**该指令
（无任何异常 opcode），`.ms` 脚本中使用 `assert` 会导致未定义行为。**P6-T084（`assert` 运行期）
落地前，禁止在 `.ms` 测试中使用 `assert`。** T084 完成后，可改用 `assert` 内联断言替代部分
golden 用例（尤其是不易做 stdout 精确匹配的场景），两种风格可并存。

---

## 6. 已知不可用特性（编写用例前必读）

以下特性截至 P4-T067/P5-T068 仍不可用，`.ms` 测试**不得**依赖，否则编译失败或运行时静默退出
（exit 1，无 stdout/stderr）：

| 特性 | 现状 | 解锁任务 |
|---|---|---|
| `obj.method()`（str/list/map/set/tuple/bytes 内置方法） | `methods` 字典为 `NULL`，`GET_ATTR` 分派落空 | P5-T073 |
| `frozenset` | 无字面量语法，也未注册为内置构造函数 | 待定（`type-system.md §2.11`） |
| 三元表达式 `a if cond else b` | 编译器报 `cannot compile expression kind 10` | 未排期（记录于本文档，待建任务） |
| f-string 插值 `$"{expr}"` | 词法层已切分 token，但 Pratt 解析器报 `expected expression` | 未排期（记录于本文档，待建任务） |
| 链式比较 `a < b < c` | 语法上仅是左结合的两次二元 `<`（非 Python 链式语义），慎用 | 不适用（按设计） |
| `//` 作为运算符 | 无浮点/整除专用运算符；整数间 `/` 本身即整除（`type-system.md §2.1`） | 不适用（按设计） |
| `del lst[i]` / `del s.item`（list/set 的 `DEL_ITEM`） | 仅 `msMapType` 实现了 `tpDelitem`；list/set 无此槽，触发空指针分派 | 未排期（记录于本文档，待建任务） |
| `and`/`or` 直接作为函数调用实参（如 `print(true and false)`） | 触发 fall-through（非短路）分支时 VM 静默 crash（exit 1）；先 `x := a and b` 赋值再 `print(x)` 可绕开 | 未排期（记录于本文档，待建任务，疑似 `AND_JMP`/`OR_JMP` 跳转在 CALL 实参栈深度下的编译期 bug） |
| `str`/`tuple`/`set`/`map`/`bytes` 的 `print()` 直接输出整个容器 | 均未实现 `tpRepr`/`tpStr`（仅 `list`/`int`/`float`/`bool`/`str` 等有）；可用 `list(container)` 转换后打印，或按下标/`len`/`in` 间接验证 | 未排期 |

> 上表记录的运行时 bug（三元表达式、f-string 插值、`del` on list/set、`and`/`or` fall-through）
> 是编写本轮 M1 用例时通过实际运行 `mslang.exe` 发现的，均**不在本文档职责范围内修复**，仅作为
> 测试编写者的避坑记录；如需修复请另建任务。

---

## 7. 新任务同步约定

每实现一个新任务（Txx），若引入**用户可观察的运行时行为**（新运算符、新内置函数、新类型方法、
新语法可执行语义等），须同步完成：

1. 在当前里程碑目录（如 `tests/ms/m1/`）补充或扩展 `.ms` 用例（按 §2 命名，覆盖新增行为的
   正常路径与至少一个边界情况）；
2. 按 §3 在 `tests/CMakeLists.txt` 注册；
3. 在该任务文档（或所属里程碑的 e2e 宿主任务文档）的验收标准 checklist 中按 §4 添加
   `<!-- v:ms:<ctest名> -->` 关联行；
4. 若新任务解锁了 §6 中标记"不可用"的特性，更新对应表格行（标注已解锁并移除限制）。

若新任务**不引入**运行时可观察行为（纯内部重构、C 单元测试覆盖的内部数据结构等），无需补
`.ms` 用例。
