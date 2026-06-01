# C 编码规范

本规范适用于 mslang 项目的全部 C 源文件与头文件，以
[SFU CMPT433 CodeStyleGuide_C](https://opencoursehub.cs.sfu.ca/bfraser/grav-cms/cmpt433/guides/files_bbg/CodeStyleGuide_C.html)
为蓝本，并结合项目实际做以下调整：

- **命名**采用 camelCase（偏离 Google C Style 的 snake_case）；
- **注释**只用 `//`（覆盖 SFU 指南的多行 `/* */` 建议，由项目 CLAUDE.md 强制）；
- **缩进**用 2 空格（不用 Tab）；
- **花括号**全部 K&R/1TBS（覆盖 SFU 指南的函数 Allman 例外）。

---

## 1. 总则

- 优先考虑**简洁、清晰、可维护**，而非追求技巧或过度抽象。
- 所有标识符必须使用**有意义的英文单词**；禁止缩写（除非含义在上下文中绝对清晰）。
  - **例外 1**：循环计数变量可使用单字母 `i`、`j`、`k`。
  - **例外 2**：作用域不超过 20 行的局部变量，若含义明显可适当缩写。
- 代码的目标读者是人，不是编译器。

---

## 2. 命名约定

### 2.1 函数与变量

- 函数和变量统一使用 **camelCase**：首字母小写，后续每个单词首字母大写。

```c
// 推荐
int calculateAverage(int *values, int count);
float taxRate = 0.13f;
bool isEndOfFile = false;

// 禁止
int calculate_average(int *values, int count);  // snake_case
int CalcAvg(int *v, int n);                     // 缩写/混乱大小写
```

- 函数名应体现**动作**：`parseToken`、`resizeBuffer`、`lookupSymbol`。
- 布尔变量命名需使 `if`/`while` 语句读起来像自然语言：

```c
if (isOpen) { ... }
while (!isEndOfFile) { ... }
```

### 2.2 类型与 typedef

- 所有类型（`struct`、`union`、`enum`、`typedef`）使用 **CamelCase**：首字母大写。

```c
typedef struct {
  int32_t ref_count;
  MsObject *ptr;
} MsHandleSlot;

typedef MsHandleSlot *MsHandle;
```

### 2.3 常量与宏

- 常量全部大写，单词间用下划线分隔（**UPPER_CASE**）。
- **优先使用 `const` 变量**代替 `#define` 宏；若必须用宏，同样采用 UPPER_CASE。

```c
// 推荐：const 常量
static const int DAYS_PER_WEEK = 7;
static const size_t MAX_PATH_LEN = 4096;

// 仅在需要时才用宏
#define MS_VERSION_MAJOR 1
```

- **避免魔法数字**（Magic Numbers）；`0` 和 `1` 在含义显而易见时可直接使用。

```c
// 禁止
if (numItems > 42) { ... }

// 推荐
static const int MAX_ITEMS = 42;
if (numItems > MAX_ITEMS) { ... }
```

### 2.4 迁移说明（历史遗留 API）

现有公开 API 符号（如 `ms_vm_run_file`、`ms_new_list` 等）采用 snake_case，属**历史遗留**。
计划在后续版本中逐步重命名为 camelCase（`msVmRunFile`、`msNewList`）。
**新增代码一律使用 camelCase**，不得新增 snake_case 符号。

---

## 3. 格式与排版

### 3.1 缩进

- 使用 **2 个空格**缩进，**禁止使用 Tab**。
- 编辑器应配置为"插入空格"模式，Tab 宽度设为 2。

### 3.2 花括号（K&R / 1TBS）

- **所有**左花括号跟在语句末尾同一行，包括函数定义。
- 右花括号单独一行，与对应关键字对齐。
- `if`/`else`/`while`/`for`/`do` 块**即使只有一条语句也必须加花括号**。

```c
// 推荐（K&R，含函数）
int clamp(int val, int lo, int hi) {
  if (val < lo) {
    return lo;
  } else if (val > hi) {
    return hi;
  } else {
    return val;
  }
}

// 禁止：函数左括号另起一行（Allman）
int clamp(int val, int lo, int hi)
{
  ...
}

// 禁止：单语句不加花括号
if (x < 0)
  x = 0;
```

### 3.3 变量声明

- 每个变量**单独一行**声明，避免逗号分隔的多变量声明（防止指针语法歧义）。

```c
// 推荐
int *pFirst;
int count;

// 禁止（读者难以区分 pFirst 是指针而 count 不是）
int *pFirst, count;
```

### 3.4 语句

- 每条语句**单独一行**，禁止在同一行写多条语句。

```c
// 禁止
x = 1; y = 2;

// 推荐
x = 1;
y = 2;
```

### 3.5 空格

| 场景 | 规则 | 示例 |
|---|---|---|
| 二元运算符 | 两侧各一空格 | `a + b`、`x = y * 2` |
| 逗号 | 后一空格，前无空格 | `foo(a, b, c)` |
| 一元运算符 | 不加额外空格 | `!flag`、`*ptr`、`++i` |
| 函数调用括号 | 紧跟函数名，不加空格 | `strlen(s)` |
| 强制转换 | 类型与表达式间不加空格 | `(int)x` |

### 3.6 复杂表达式

为复杂表达式**加括号**显式表示优先级；或拆分为带有意义名称的子表达式。

```c
// 可接受（显式括号）
int result = (a + b) * (c - d);

// 更推荐（拆分，每个子表达式都有意义）
int sum = a + b;
int diff = c - d;
int result = sum * diff;
```

---

## 4. 注释

> **项目强制规定**：只使用 `//` 行注释，禁止 `/* ... */` 块注释。
> 这覆盖了 SFU 指南关于多行用 `/* */` 的建议。

### 4.1 模块头注释

每个 `.h` 文件顶部必须有描述性注释，说明模块的**用途**和主要提供的数据/功能。

```c
// vm.h — 字节码虚拟机核心：指令分派、调用栈、全局状态。
// 所有 Worker 线程通过此头文件访问 VM 内部结构。
```

### 4.2 函数注释

- 函数名已能说明做什么时，注释可省略。
- 注释应解释**为什么**，而非重复代码在说什么。

```c
// 禁止（重复代码）
// 将 val 加 1
val++;

// 推荐（解释原因）
// 跳过 UTF-8 BOM（0xEF 0xBB 0xBF），避免词法器误判第一个 token。
if (src[0] == 0xEF && src[1] == 0xBB && src[2] == 0xBF) {
  src += 3;
}
```

### 4.3 注释位置

- 注释写在**被注释代码的上方**，与代码保持相同缩进级别。
- 行尾注释只用于极短的说明（如枚举/结构体字段标注），禁止行尾注释包含完整句子。

### 4.4 用函数替代注释

优先将需要大段注释解释的逻辑提取为 3–5 行短函数，而不是靠注释维持可读性。

```c
// 不推荐：用注释分隔大函数的逻辑段落
void processInput(...) {
  // --- 阶段 1：词法分析 ---
  ...
  // --- 阶段 2：语法分析 ---
  ...
}

// 推荐：各阶段提取为独立函数
void processInput(...) {
  tokenize(...);
  parse(...);
}
```

---

## 5. 文件组织

### 5.1 文件命名与扩展名

- 头文件使用 `.h`，源文件使用 `.c`。
- 文件名视为**大小写敏感**，`#include` 中的大小写必须与文件系统完全匹配。

### 5.2 Include Guard

每个头文件必须有 include guard，格式为 `FILENAME_H`（全大写，`.` 替换为 `_`）。

```c
// lexer.h
#ifndef LEXER_H
#define LEXER_H

// ... 头文件内容 ...

#endif // LEXER_H
```

### 5.3 头文件自包含

- 头文件必须**自包含**：包含它所依赖的所有其他头文件，不依赖包含顺序。
- 所有 `#include` 指令集中放在文件**顶部**，不得散落在代码中间。

### 5.4 函数原型

- 头文件中的函数原型必须包含**完整的参数名**。
- 原型中的参数名必须与定义中一致。

```c
// 推荐（参数名完整且与定义一致）
void printResult(int numStudents, float avgScore);

// 禁止（缺省参数名）
void printResult(int, float);
```

### 5.5 文件内函数排序

- 文件内函数按**从一般到具体**的顺序排列：`main` 或顶层入口在上，被调用的子函数在下。
- 配合函数原型，支持读者从上到下顺序阅读，无需反复翻页。

---

## 6. 语句与控制流

### 6.1 自增/自减运算符

单独作为语句使用时，前置（`++i`）和后置（`i++`）均可接受；保持文件内一致即可。

### 6.2 禁止 `goto`

禁止使用 `goto`。

### 6.3 减少 `break` 与 `continue`

循环应设计为自然退出；`break` 和 `continue` 不得用于替代清晰的条件逻辑。
确实需要提前退出时，优先将循环体提取为函数并用 `return`。

### 6.4 `switch` 语句

- **必须**包含 `default` 标签。
- 如果 `default` 在逻辑上不可能到达，用 `assert(false)` 表明意图。
- 刻意的 fall-through（不加 `break`）必须加注释说明。

```c
switch (token.type) {
  case TOKEN_INT:
    parseIntLiteral(&token);
    break;
  case TOKEN_FLOAT:
    parseFloatLiteral(&token);
    break;
  case TOKEN_STRING:  // fall-through: 字符串与 raw 字符串共用同一解析路径
  case TOKEN_RAW_STRING:
    parseStringLiteral(&token);
    break;
  default:
    assert(false);  // 不可能到达：调用方已过滤非字面量 token
}
```

---

## 7. 断言

- **积极使用** `assert()` 校验前置条件、函数参数合法性和对象状态。
- 断言内**禁止**包含有副作用的表达式（`assert(i++ < n)` 等），因为 release 构建会将断言编译掉。

```c
// 推荐
assert(vm != NULL);
assert(index >= 0 && index < list->len);

// 禁止（副作用）
assert(++count < MAX);
```

- Release 构建（定义 `NDEBUG`）断言自动移除；不要将关键逻辑放在断言内。

---

## 8. 编码与提交

### 8.1 文件编码

- 所有源文件必须以 **UTF-8** 编码、**LF**（Unix）行尾保存。
- 每行末尾不得有**尾随空白**（trailing whitespace）。

### 8.2 与 Google C Style 的差异

本规范在以下方面有意偏离 [Google C Style Guide](https://google.github.io/styleguide/cguide.html)：

| 维度 | Google C Style | 本规范 |
|---|---|---|
| 函数/变量命名 | `snake_case` | `camelCase` |
| 缩进 | 2 空格 | 2 空格（相同） |
| 花括号 | K&R | K&R（相同） |

其余方面（文件编码、注释风格、头文件保护等）与 Google C Style 一致或更严格。
