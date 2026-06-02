# C 语言编码规范

> 版本 1.0 | 适用标准：C17/C23 | 最后更新：2026-06-02

## 目录

1. [引言](#1-引言)
2. [文件结构](#2-文件结构)
3. [命名约定](#3-命名约定)
4. [类型与 typedef](#4-类型与-typedef)
5. [格式排版](#5-格式排版)
6. [注释](#6-注释)
7. [函数设计](#7-函数设计)
8. [错误处理与资源管理](#8-错误处理与资源管理)
9. [内存管理](#9-内存管理)
10. [预处理器与宏](#10-预处理器与宏)
11. [模块化与接口设计](#11-模块化与接口设计)
12. [安全与可移植性](#12-安全与可移植性)
13. [附录：工具链使用](#13-附录工具链使用)

---

## 1. 引言

本规范适用于所有以 **C17 或更新标准**编写的 C 语言项目。

格式与组织总体参考 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)；
命名约定参考 [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html)
并适配 C 语言无类的特点。

### 1.1 优先级

1. **本规范**优先于个人习惯。
2. **一致性优先**：在已有代码库中，优先与周边代码风格保持一致；引入新文件时遵循本规范。
3. **可读性**：所有规则均以「让陌生人在最短时间内读懂代码」为最终目标。

### 1.2 目标 C 标准

所有代码使用 **C17**（ISO/IEC 9899:2018）或更新版本编译。允许且鼓励使用以下特性：

- `//` 行注释
- 声明与语句混合出现（变量随用随声明）
- `<stdbool.h>` 提供的 `bool`、`true`、`false`
- `<stdint.h>` 提供的定宽整数类型（`uint32_t` 等）
- 指定初始化器（designated initializer）：`.field = value`
- `_Static_assert` / `static_assert`（C23 无下划线前缀）
- 复合字面量（compound literal）

### 1.3 文件编码与换行

| 要求 | 说明 |
|------|------|
| 字符编码 | UTF-8，无 BOM |
| 换行符 | LF（`\n`），**禁止** CRLF |
| 行尾空白 | 必须清除，任何行不得以空格或 Tab 结尾 |
| 文件结尾 | 以单个换行符结尾 |

这些规则由项目根目录的 `.editorconfig` 自动执行。

---

## 2. 文件结构

### 2.1 文件命名

源文件和头文件使用 **snake_case**（小写字母 + 下划线），扩展名为 `.c` / `.h`。

```
// 正例
user_manager.c
user_manager.h
http_client.c
ring_buffer.h

// 反例
UserManager.c    // 禁止 PascalCase
httpClient.c     // 禁止 camelCase
http-client.c    // 禁止 kebab-case
```

### 2.2 头文件保护

所有头文件使用 `#pragma once`，不使用 `#ifndef` 宏保护。

```c
// 正例
#pragma once

#include <stdint.h>

// ...
```

### 2.3 头文件自包含

每个头文件必须能**独立被包含**（self-contained）：它自己 `#include` 所有它所依赖的头文件，
不依赖调用方先行包含某个特定头。

### 2.4 #include 顺序

`#include` 按以下顺序排列，每组之间留一个空行：

1. 对应头文件（`.c` 文件中 `#include "foo.h"` 放最前）
2. C 标准库头文件（`<stdio.h>`、`<stdlib.h>` 等）
3. 系统/POSIX 头文件（`<unistd.h>`、`<sys/types.h>` 等）
4. 第三方库头文件
5. 本项目其他头文件（带路径，使用双引号）

```c
// user_manager.c 示例
#include "user_manager.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "config.h"
#include "logger.h"
```

组内按字母顺序排列（`.clang-format` 的 `SortIncludes` 自动执行）。

### 2.5 文件头注释

每个源文件和头文件开头用 `//` 注释简述文件内容与版权（如有）。

```c
// user_manager.h
// User lifecycle management: creation, lookup, and deletion.
//
// Copyright (C) 2026 Example Corp. All rights reserved.
```

---

## 3. 命名约定

命名是可读性的核心。本规范的命名体系以 **Java 风格**为基础，适配 C 语言无类/无命名空间的特点。

### 3.1 总览

| 实体 | 风格 | 示例 |
|------|------|------|
| 函数 | camelCase | `getUserName`, `parseHttpHeader` |
| 局部变量、参数 | camelCase | `userCount`, `bufSize` |
| 全局变量 | `g` 前缀 + camelCase | `gConfig`, `gLogLevel` |
| 结构体/联合体成员 | camelCase | `userName`, `nextNode` |
| typedef 类型名 | PascalCase | `UserInfo`, `ErrorCode` |
| 宏 | UPPER\_SNAKE\_CASE | `MAX_SIZE`, `ARRAY_LEN` |
| `const` 常量 | `k` 前缀 + PascalCase | `kMaxRetries`, `kDefaultTimeout` |
| 枚举值 | UPPER\_SNAKE\_CASE | `STATE_RUNNING`, `ERR_TIMEOUT` |
| 文件名 | snake\_case | `user_manager.c` |

### 3.2 函数命名

函数名使用 **camelCase**，以动词或动词短语开头，清晰表达行为。

```c
// 正例
int  getUserById(uint32_t userId, UserInfo* out);
void resetBuffer(Buffer* buf);
bool isValidEmail(const char* email);

// 反例
int  GetUserById(...);   // 禁止 PascalCase
int  get_user_by_id(...);// 禁止 snake_case
int  user(...);          // 名称过短，不表达行为
```

对外公共 API 可加模块前缀以避免链接符号冲突，前缀用 camelCase 连写：

```c
// 模块 "net" 的公共 API
int  netSendPacket(NetConn* conn, const void* data, size_t len);
void netClose(NetConn* conn);
```

### 3.3 变量命名

**局部变量与函数参数**：camelCase，名称有意义，不使用单字母（循环变量 `i`/`j`/`k` 除外）。

```c
// 正例
int  retryCount = 0;
const char* userName = user->name;
for (int i = 0; i < len; i++) { ... }

// 反例
int  rc  = 0;       // 过度缩写，含义不明
int  retry_count;   // 禁止 snake_case
```

**全局变量**：加 `g` 前缀 + camelCase，并在注释中说明其生命周期与线程安全性。

```c
// 正例
static LogLevel gLogLevel = LOG_INFO;  // module-private global
int gExitCode = 0;                     // process-wide exit code

// 反例
LogLevel logLevel;  // 缺少 g 前缀，无法一眼识别全局变量
```

---

### 3.4 结构体与联合体成员

成员名使用 **camelCase**，与函数参数风格一致，使 `obj->fieldName` 读起来自然。

```c
struct UserInfo {
  uint32_t    userId;
  char        userName[64];
  uint64_t    createdAt;
  bool        isActive;
};
```

### 3.5 类型名（typedef）

`typedef` 引入的类型别名使用 **PascalCase**，无后缀。

```c
// 正例
typedef struct UserInfo UserInfo;
typedef uint32_t UserId;
typedef int (*CompareFunc)(const void* a, const void* b);

// 反例
typedef struct user_info user_info_t;  // 禁止 snake_case + _t 后缀
typedef int (*compare_func)(...);      // 禁止 snake_case
```

枚举类型本身也用 PascalCase：

```c
typedef enum AppState {
  APP_STATE_IDLE,
  APP_STATE_RUNNING,
  APP_STATE_STOPPED,
} AppState;
```

### 3.6 宏

函数宏和对象宏使用 **UPPER\_SNAKE\_CASE**。

```c
// 正例
#define MAX_SIZE         256
#define ARRAY_LEN(arr)   (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b)        ((a) < (b) ? (a) : (b))

// 反例
#define maxSize  256         // 禁止 camelCase
#define array_len(arr) ...   // 禁止 snake_case
```

### 3.7 const 常量

文件作用域或函数作用域的 `const` 变量（代替魔法数字）使用 **`k` 前缀 + PascalCase**。

```c
// 正例
static const int kMaxRetries = 3;
static const size_t kDefaultBufSize = 4096;

// 反例
static const int MAX_RETRIES = 3;   // const 不是宏，不用全大写
static const int maxRetries  = 3;   // 缺 k 前缀
```

### 3.8 枚举值

枚举值使用 **UPPER\_SNAKE\_CASE**，建议以枚举类型名的缩写作前缀，避免全局命名冲突。

```c
typedef enum ErrorCode {
  ERR_OK           = 0,
  ERR_INVALID_ARG  = -1,
  ERR_OUT_OF_MEM   = -2,
  ERR_TIMEOUT      = -3,
} ErrorCode;
```

---

## 4. 类型与 typedef

### 4.1 使用定宽整数类型

优先使用 `<stdint.h>` 中的定宽类型，而非裸 `int`/`long`，以明确位宽和符号。

```c
// 正例
uint32_t userId;
int64_t  timestamp;
uint8_t  flags;

// 反例
unsigned int userId;   // 位宽不明确
long timestamp;        // 平台相关
```

`size_t` 用于表示内存尺寸或数组长度；`ptrdiff_t` 用于指针差值；`bool` 用于布尔逻辑。

### 4.2 typedef 的使用原则

**规则**：`typedef` 仅用于以下两种情况：

1. **不透明句柄（opaque handle）**：实现细节对调用方不可见，隐藏 `struct` 关键字。
2. **函数指针类型**：提高可读性。

普通 `struct`/`union`/`enum` 类型声明时**不隐藏**关键字：调用方使用 `struct Foo`。

```c
// 正例：不透明句柄，隐藏实现
// io_handle.h
typedef struct IoHandle IoHandle;  // 仅暴露名字，结构体定义在 .c 文件
IoHandle* ioOpen(const char* path);
void      ioClose(IoHandle* h);

// 正例：函数指针
typedef int (*Comparator)(const void* a, const void* b);

// 正例：普通 struct，不 typedef
struct Point {
  int x;
  int y;
};
void movePoint(struct Point* p, int dx, int dy);

// 反例：为普通 struct 做 typedef 只为省 struct 关键字
typedef struct { int x; int y; } Point;  // 避免
```

### 4.3 枚举优先于宏常量

用 `enum` 定义一组相关整数常量，而非多个 `#define`：

```c
// 正例
typedef enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
} LogLevel;

// 反例
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
```

---

## 5. 格式排版

### 5.1 缩进

- 每级缩进 **2 个空格**，**禁止使用 Tab**。
- 换行后的续行缩进 **4 个空格**（相对最近的非续行代码行）。

### 5.2 行宽

每行不超过 **120 个字符**。超出时在合适位置换行：

```c
// 正例：参数过多时，每个参数独占一行，对齐到第一个参数位置
int result = doSomethingWithLongName(firstArg, secondArg,
                                     thirdArg, fourthArg);

// 或在括号后换行，缩进 4 空格
int result = doSomethingWithLongName(
    firstArg, secondArg, thirdArg, fourthArg);
```

### 5.3 大括号（K&R 风格）

左括号跟在语句末尾，右括号独占一行。`if`/`for`/`while`/`do` 即使只有单条语句也**必须加大括号**。

```c
// 正例
if (err != ERR_OK) {
  return err;
}

for (int i = 0; i < len; i++) {
  process(buf[i]);
}

// 反例
if (err != ERR_OK)
  return err;   // 禁止省略大括号

if (err != ERR_OK) { return err; }  // 禁止与左括号同行
```

函数定义的左括号同样跟在函数签名末尾：

```c
int getUserById(uint32_t id, UserInfo* out) {
  // ...
}
```

### 5.4 指针声明

星号 `*` 紧贴**类型名**，与变量名之间有空格。

```c
// 正例
int* ptr;
const char* name;
void* getUserById(uint32_t id);

// 反例
int *ptr;    // 禁止星号贴变量名
int * ptr;   // 禁止两侧各有空格
```

多指针变量不可在同一行声明：

```c
// 反例（歧义）
int* a, b;   // b 是 int，不是 int*；分行声明

// 正例
int* a;
int* b;
```

### 5.5 空格规则

- **运算符两侧**加空格：`a + b`，`x == y`，`i++`（后缀 `++`/`--` 贴变量名，无前置空格）。
- **逗号后**加空格：`foo(a, b, c)`。
- **关键字与括号之间**加空格：`if (cond)`，`for (int i = 0; ...)`，`while (x)`。
- **函数名与括号之间不加空格**：`foo(x)` 而非 `foo (x)`。
- **强制类型转换后加空格**：`(int) value`。

### 5.6 空行

- 函数定义之间留 **1 个空行**。
- 函数内逻辑分段可用 **1 个空行**，不超过 1 个。
- 文件末尾恰好一个换行（`.editorconfig` `insert_final_newline = true` 自动保证）。

---

## 6. 注释

### 6.1 只使用 `//` 注释

**禁止使用 `/* */`**（包括单行、多行、块注释和文档注释）。统一使用 `//`，原因：

- 可以用 `/* */` 临时整块注释掉含有 `//` 的代码，两者语义不冲突。
- 避免 `/* */` 嵌套失效问题。
- 风格一致，无需区分场景。

```c
// 正例
// Compute the checksum for the given buffer.
// Returns 0 on success, negative on error.
int computeChecksum(const uint8_t* buf, size_t len, uint32_t* outCrc);

// 反例
/* Compute checksum */           // 禁止
/** @param buf buffer pointer */ // 禁止（Doxygen /* */ 也禁止）
```

### 6.2 文件头注释

```c
// network.h
// TCP/IP connection management.
//
// Provides functions to establish, use, and tear down TCP connections.
// Thread safety: all functions are thread-safe after netInit() is called.
//
// Copyright (C) 2026 Example Corp.
```

### 6.3 函数注释

公共 API（头文件中声明的函数）在声明处用 `//` 说明：
**目的**、**各参数含义**、**返回值**、**副作用或线程安全性**（如有）。

```c
// Looks up a user by ID and fills the output struct.
//
// userId: the unique user ID to search for.
// out:    caller-allocated struct to fill; must not be NULL.
//
// Returns ERR_OK on success, ERR_NOT_FOUND if no such user exists,
// or ERR_INVALID_ARG if out is NULL.
int getUserById(uint32_t userId, UserInfo* out);
```

内部（`static`）函数若逻辑不显而易见，也应加注释；简单函数可省略。

### 6.4 行内注释

用于解释不显而易见的实现细节，放在语句同行末尾或上方。
注释内容说明**为什么**，而非重复代码在做什么。

```c
// 正例
timeout *= 2;  // exponential back-off: double on each retry

// 反例
i++;  // increment i    （只是重复代码，无价值）
```

### 6.5 TODO 注释

```c
// TODO(username): replace linear scan with hash lookup when user count > 1000
```

格式：`// TODO(<owner>): <描述>`。`owner` 可以是用户名或追踪系统 ticket 号。

---

## 7. 函数设计

### 7.1 单一职责

每个函数只做一件事，名称完整表达其职责。若函数超过 **80 行**或含超过 **50 条语句**，应考虑拆分。

### 7.2 参数数量

参数数量建议不超过 **8 个**。超过时，考虑将相关参数归入一个 `struct`。

### 7.3 参数顺序

输入参数在前，输出参数（指针）在后：

```c
// 输入: buf, len；输出: out
int parseHeader(const uint8_t* buf, size_t len, Header* out);
```

### 7.4 输出参数约定

- 输出参数用指针传入，调用方分配内存。
- 函数名可用 `get`/`parse`/`compute` 前缀暗示有输出参数。
- 若函数可能不填充输出参数（如失败时），文档注释中须说明。

### 7.5 返回值约定

- **错误码**：返回 `int` 或枚举；`0` / `ERR_OK` 表示成功，负值或正枚举值表示错误。
- **布尔判断**：返回 `bool`（`<stdbool.h>`）。
- **查找/分配**：返回指针，失败返回 `NULL`，并通过 `errno` 或出参报告错误。

不要在同一个函数的不同路径中既返回数据又返回错误码（混用会引发调用方困惑）。

### 7.6 避免过深嵌套

嵌套层级建议不超过 **3 层**。尽早 `return`（guard clause 风格）代替深层嵌套：

```c
// 正例：guard clause
int processRequest(Request* req) {
  if (req == NULL) {
    return ERR_INVALID_ARG;
  }
  if (!req->isValid) {
    return ERR_INVALID_ARG;
  }
  // 正常流程从这里开始，缩进浅
  return doProcess(req);
}

// 反例：深嵌套
int processRequest(Request* req) {
  if (req != NULL) {
    if (req->isValid) {
      return doProcess(req);
    }
  }
  return ERR_INVALID_ARG;
}
```

---

## 8. 错误处理与资源管理

### 8.1 错误码模式

函数通过**返回值**传递错误：`0`（或 `ERR_OK`）表示成功，负值/枚举表示具体错误。
**禁止**忽略返回值——调用方必须检查并处理错误。

```c
int result = openFile(path, &file);
if (result != ERR_OK) {
  logError("openFile failed: %d", result);
  return result;
}
```

### 8.2 goto cleanup 模式

当函数中涉及多个需要释放的资源时，使用 `goto cleanup` 统一清理，避免在每个
提前返回路径上重复释放逻辑：

```c
// 正例：goto cleanup 范式
int processFile(const char* path, Result* out) {
  int err = ERR_OK;
  FILE* fp = NULL;
  uint8_t* buf = NULL;

  fp = fopen(path, "rb");
  if (fp == NULL) {
    err = ERR_IO;
    goto cleanup;
  }

  buf = malloc(kDefaultBufSize);
  if (buf == NULL) {
    err = ERR_OUT_OF_MEM;
    goto cleanup;
  }

  // ... 正常处理 ...
  err = parseContent(fp, buf, kDefaultBufSize, out);

cleanup:
  free(buf);      // free(NULL) 是安全的
  if (fp != NULL) {
    fclose(fp);
  }
  return err;
}
```

**规则**：

- `goto` 只跳向**同函数内**、**后方**的 `cleanup` 标签，禁止向上跳或跨函数跳。
- 资源按**逆序**释放（后分配的先释放）。
- `free(NULL)` 安全，无需先判空；但 `fclose`/`close` 等需判空/判有效。
- `cleanup` 标签之前声明并初始化为 `NULL`/`0` 的指针，可在 `cleanup` 无条件释放。

### 8.3 指针释放后置 NULL

指针释放后立即置 `NULL`，防止悬空指针被意外解引用：

```c
free(buf);
buf = NULL;
```

### 8.4 不使用全局 errno 传递错误

禁止以修改全局 `errno` 作为函数错误传递机制；仅在封装系统调用时，
可将 `errno` 映射为本项目的错误码后返回。

---

## 9. 内存管理

### 9.1 谁分配谁释放

内存的分配方与释放方必须一致（调用方分配 → 调用方释放；被调用方分配 → 文档明确说明由谁释放）。
在函数文档注释中标注所有权：

```c
// Allocates and returns a new Buffer of the given size.
// Caller is responsible for calling bufferFree() when done.
Buffer* bufferAlloc(size_t size);

void bufferFree(Buffer* buf);
```

### 9.2 检查分配结果

`malloc`/`calloc`/`realloc` 返回值必须检查，返回 `NULL` 时处理错误：

```c
// 正例
void* ptr = malloc(size);
if (ptr == NULL) {
  return ERR_OUT_OF_MEM;
}

// 反例
void* ptr = malloc(size);
memcpy(ptr, src, size);  // 若 ptr == NULL，立即崩溃
```

### 9.3 使用 calloc 初始化

分配需要零初始化的内存时，优先用 `calloc` 而非 `malloc + memset`：

```c
// 正例
UserInfo* user = calloc(1, sizeof(UserInfo));

// 反例
UserInfo* user = malloc(sizeof(UserInfo));
memset(user, 0, sizeof(UserInfo));
```

### 9.4 避免 VLA

**禁止**使用可变长数组（VLA，Variable-Length Array）：栈溢出风险且行为依赖平台。
使用 `malloc` 替代：

```c
// 反例
void foo(int n) {
  int buf[n];  // 禁止 VLA
}

// 正例
void foo(int n) {
  int* buf = malloc((size_t)n * sizeof(int));
  if (buf == NULL) { return; }
  // ...
  free(buf);
}
```

---

## 10. 预处理器与宏

### 10.1 优先使用语言特性替代宏

| 用途 | 宏（避免） | 替代 |
|------|-----------|------|
| 常量 | `#define PI 3.14` | `static const double kPi = 3.14;` |
| 内联函数 | `#define MAX(a,b) ...` | `static inline int max(int a, int b)` |
| 类型安全断言 | `#define ASSERT(x) ...` | `_Static_assert(x, "msg")` |

### 10.2 函数宏的必要约束

若不得不写函数宏，必须：

1. 用 `do { ... } while (0)` 包裹多语句宏体，避免 `if` 分支问题。
2. 宏参数全部加括号，防止运算符优先级陷阱。
3. 避免参数被多次求值（有副作用的表达式传入时危险）。

```c
// 正例
#define SWAP(a, b)  \
  do {              \
    typeof(a) _tmp = (a); \
    (a) = (b);      \
    (b) = _tmp;     \
  } while (0)

// 反例（参数多次求值）
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
// MAX(x++, y++) 会对 x 或 y 求值两次
```

### 10.3 条件编译

条件编译块末尾注明对应的条件，便于阅读长文件：

```c
#ifdef PLATFORM_LINUX
  // ... Linux 实现 ...
#endif // PLATFORM_LINUX
```

---

## 11. 模块化与接口设计

### 11.1 公共 API 与内部实现分离

- 头文件（`.h`）只暴露调用方需要的符号：函数声明、公共类型、常量。
- 模块内部函数（不对外暴露的）在 `.c` 文件中声明为 `static`，不出现在头文件中。

```c
// user_manager.h — 公共接口
#pragma once
#include <stdint.h>
#include "error_code.h"

typedef struct UserInfo UserInfo;

ErrorCode userCreate(uint32_t id, const char* name, UserInfo** out);
void      userDestroy(UserInfo* user);

// user_manager.c — 内部实现
static bool isNameValid(const char* name);  // 外部不可见
```

### 11.2 最小化头文件依赖

- 优先使用**前向声明**（forward declaration）而非 `#include` 完整头文件，减少编译依赖。
- 只有当需要访问结构体字段（而非仅持有指针）时，才在头文件中 `#include` 对应头。

```c
// 正例：仅持有指针，前向声明即可
struct Config;  // 前向声明
void applyConfig(const struct Config* cfg);
```

### 11.3 循环依赖

头文件之间**禁止循环包含**。发现循环依赖时，用前向声明或引入中间抽象层打破循环。

---

## 12. 安全与可移植性

### 12.1 使用安全替代函数

| 危险函数 | 安全替代 |
|---------|---------|
| `strcpy` | `strncpy` 或 `snprintf` |
| `sprintf` | `snprintf` |
| `gets` | `fgets` |
| `scanf("%s", ...)` | `scanf("%127s", ...)` 或 `fgets` |

### 12.2 sizeof 使用对象而非类型

```c
// 正例
UserInfo* user = malloc(sizeof(*user));

// 反例
UserInfo* user = malloc(sizeof(UserInfo));  // 类型改名后需手动同步
```

### 12.3 避免未定义行为

- 有符号整数溢出是**未定义行为**；需要溢出语义时改用无符号类型。
- 禁止对 `NULL` 指针解引用；所有指针在解引用前必须确认非 `NULL`。
- 禁止越界访问数组；使用 `ARRAY_LEN` 宏获取数组长度。
- 位移操作数必须在 `[0, sizeof(type)*8)` 范围内。

### 12.4 整数类型提升

混合有符号与无符号比较时，编译器会隐式转换，结果可能出乎意料：

```c
// 危险：n 为 int，len 为 size_t；若 n < 0，比较结果错误
for (int n = -1; n < len; n++) { ... }

// 正例：统一使用 size_t
for (size_t i = 0; i < len; i++) { ... }
```

---

## 13. 附录：工具链使用

### 13.1 文件清单

| 文件 | 用途 |
|------|------|
| `.editorconfig` | 编辑器/CI 统一 UTF-8 LF、trim 行尾空白、末尾换行 |
| `.clang-format` | `clang-format` 自动格式化 |
| `.clang-tidy` | `clang-tidy` 静态检查（含命名规范） |

将以上三个文件放置在项目**根目录**，工具会自动向上查找并应用。

### 13.2 格式化

```bash
# 格式化单个文件（原地修改）
clang-format -i src/user_manager.c

# 格式化所有 .c 和 .h 文件
find src -name '*.c' -o -name '*.h' | xargs clang-format -i

# 仅检查不修改（用于 CI）
clang-format --dry-run --Werror src/user_manager.c
```

### 13.3 静态检查

```bash
# 配合 CMake 生成编译数据库，再运行 clang-tidy
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build .
clang-tidy -p build src/user_manager.c
```

### 13.4 验证文件级规则

```bash
# 检查行尾空白（无输出为通过）
grep -nrP ' +$' src/

# 检查 CRLF 换行（无输出为通过）
grep -lUP '\r$' src/

# 检查文件编码（应显示 UTF-8）
file src/*.c src/*.h
```

### 13.5 CI 集成建议

在 CI 流水线中依次执行：

1. `clang-format --dry-run --Werror` — 格式检查。
2. `clang-tidy` — 命名与静态检查。
3. `grep -rP ' +$' src/` — 行尾空白检查（非零退出即失败）。
