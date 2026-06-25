# P3-T037 MsChunk：字节码块、常量池、行号表

> **状态**：✅ 已完成

---

## 任务目标 / 背景

定义并实现 `MsChunk`——字节码块数据结构（存储字节码序列、常量池、行号表），以及向 chunk 追加字节码的基础 emit API。这是编译器（T039–T047）和 VM（T051）的公共基础。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T002 | `msAlloc`/`MsVec` |
| P0-T005 | `MsValue`（常量池中存储 MsValue） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §2（MsChunk 结构与常量池） |
| `vm.md` | §3（指令集） |
| `vm.md` | §9（opcode 命名映射） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_opcode.h   # 操作码枚举（MsOpCode）
include/mslang/ms_chunk.h    # MsChunk 结构体 + emit API
src/compiler/ms_chunk.c      # MsChunk 实现
```

---

## 实现要点

### 1. 操作码枚举（`MsOpCode`）

```c
// include/mslang/ms_opcode.h
#pragma once
#include <stdint.h>

typedef enum MsOpCode {
  // ─── 栈操作 ───────────────────────────────────────────────────
  OP_POP,   // 弹出并丢弃栈顶
  OP_DUP,   // 复制栈顶
  OP_ROT2,  // 交换栈顶两个元素

  // ─── 常量加载 ─────────────────────────────────────────────────
  OP_CONST,        // AX: 常量池索引，push(constants[AX])
  OP_CONST_INT,    // AX: 内联 int24（有符号扩展），压入小整数
  OP_CONST_TRUE,   // push(true)
  OP_CONST_FALSE,  // push(false)
  OP_CONST_NIL,    // push(nil)

  // ─── 变量操作 ─────────────────────────────────────────────────
  OP_GET_LOCAL,     // A: 槽号，压入局部变量
  OP_SET_LOCAL,     // A: 槽号，弹出→局部变量
  OP_GET_GLOBAL,    // AX: 名字常量索引，从全局表查找
  OP_SET_GLOBAL,    // AX: 名字常量索引，写入全局表
  OP_DEL_GLOBAL,    // AX: 名字常量索引，删除全局变量（扩展）
  OP_GET_UPVALUE,   // A: upvalue 索引，从闭包 upvalue 读取
  OP_SET_UPVALUE,   // A: upvalue 索引，写入闭包 upvalue
  OP_CLOSE_UPVALUE, // A: 本地槽号，将 open upvalue 关闭到堆

  // ─── 算术 ─────────────────────────────────────────────────────
  OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
  OP_NEG,   // 一元取负
  OP_BAND, OP_BOR, OP_BXOR,
  OP_SHL, OP_SHR,
  OP_BNOT,  // ~

  // ─── 比较与逻辑 ───────────────────────────────────────────────
  OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
  OP_IN, OP_IS, OP_NOT,
  OP_IS_NOT,   // 扩展
  OP_NOT_IN,   // 扩展
  OP_ISINSTANCE,
  OP_AND_JMP,  // A: 偏移，短路 and（栈顶假则跳，保留值）
  OP_OR_JMP,   // A: 偏移，短路 or（栈顶真则跳，保留值）

  // ─── 跳转（操作数均为 3 字节有符号 AX，相对下一条指令）────────
  OP_JUMP,               // AX: 有符号 24 位偏移，无条件跳转
  OP_JUMP_IF_FALSE,      // AX: 有符号 24 位偏移，弹出，为假则跳
  OP_JUMP_IF_TRUE,       // AX: 有符号 24 位偏移，弹出，为真则跳
  OP_JUMP_IF_FALSE_PEEK, // AX: 有符号 24 位偏移，不弹出，为假则跳

  // ─── 函数调用与返回 ───────────────────────────────────────────
  OP_CALL,        // A: argc，调用同步函数
  OP_CALL_ASYNC,  // A: argc，调用 async func，返回 MsFuture
  OP_CALL_EX,     // 展开调用（*args）
  OP_CALL_KW,     // A: argc，末尾 map 为 kwargs
  OP_RETURN,      // 弹出返回值，销毁当前帧
  OP_RETURN_NIL,  // 等价 CONST_NIL + RETURN

  // ─── 属性与下标 ───────────────────────────────────────────────
  OP_GET_ATTR,  // AX: 名字常量索引
  OP_SET_ATTR,
  OP_DEL_ATTR,
  OP_GET_ITEM,  // s[1][s[0]]（__getitem__）
  OP_SET_ITEM,  // s[2][s[1]] = s[0]（__setitem__）
  OP_DEL_ITEM,

  // ─── 容器构建 ─────────────────────────────────────────────────
  OP_BUILD_LIST,   // A: n，弹出 n 个元素构建 list
  OP_BUILD_MAP,    // A: n，弹出 2n（key,val）构建 map
  OP_BUILD_TUPLE,  // A: n，构建 tuple
  OP_BUILD_SET,    // 扩展
  OP_BUILD_SLICE,  // A: flags，构建切片对象（start/stop/step）
  OP_BUILD_STR,    // 扩展：拼接栈上字符串片段

  // ─── 闭包与类 ─────────────────────────────────────────────────
  OP_MAKE_FUNC,   // AX: 函数常量索引，A: upvalue 数，描述符内联在指令后
  OP_MAKE_CLASS,  // AX: 类常量索引，弹出基类（若有）
  OP_LOAD_SUPER,  // 压入 super 代理

  // ─── 迭代 ─────────────────────────────────────────────────────
  OP_GET_ITER,  // s[0] = s[0].__iter__()
  OP_FOR_ITER,  // AX: 跳出 offset，调 __next__；StopIteration 则跳

  // ─── 异常处理 ─────────────────────────────────────────────────
  OP_PUSH_EXCEPT,     // AX: catch 块 offset，注册处理器
  OP_POP_EXCEPT,      // 弹出处理器（正常离开 try 块）
  OP_RAISE,           // A: 0=无参,1=有参，抛出异常
  OP_RAISE_ASSERT,    // A: 0=无消息,1=有消息，抛出 AssertionError
  OP_RERAISE,         // 在 catch 块内重抛当前异常
  OP_LOAD_EXCEPTION,  // 压入当前异常对象
  OP_CLEAR_EXCEPTION, // 清除当前异常
  OP_WITH_ENTER,      // 调用 __enter__
  OP_WITH_EXIT,       // 调用 __exit__

  // ─── 并发 ─────────────────────────────────────────────────────
  OP_GO,                // 派发到调度器新建 goroutine
  OP_MAKE_CHAN,         // A: 0=无缓冲,1=有缓冲，创建 channel
  OP_CHAN_SEND,         // s[1] <- s[0]，挂起直到接收方就绪
  OP_CHAN_RECV,         // push(<-s[0])，挂起直到有值
  OP_CHAN_CLOSE,        // 关闭 channel
  OP_AWAIT,             // 挂起当前协程直到 awaitable 完成
  OP_SELECT_BEGIN,      // AX: case 数，开始 select 块
  OP_SELECT_CASE_SEND,  // 注册 send case
  OP_SELECT_CASE_RECV,  // 注册 recv case
  OP_SELECT_END,        // 阻塞等待任一 case，跳到对应分支

  // ─── 模块 ─────────────────────────────────────────────────────
  OP_IMPORT,       // AX: 模块名常量索引
  OP_IMPORT_FROM,  // 从模块导入名称

  OP_COUNT,  // sentinel
} MsOpCode;
```

### 2. `MsChunk` 结构体

```c
// include/mslang/ms_chunk.h
#pragma once
#include <stdint.h>
#include "mslang/ms_value.h"

struct MsStr;

struct MsChunk {
  uint8_t*       code;        // 字节码字节流
  uint32_t       codeLen;
  uint32_t       codeCap;

  MsValue*       constants;   // 常量池（MsValue 数组）
  uint32_t       constLen;
  uint32_t       constCap;

  uint32_t*      lines;       // lines[i] = code[i] 所在源码行
  uint32_t       linesLen;
  uint32_t       linesCap;

  struct MsStr*  sourceName;  // 源文件名（调试用，GC 根追踪）
};
```

### 3. Emit API

```c
void msChunkInit(struct MsChunk* ck, struct MsStr* sourceName);
void msChunkFree(struct MsChunk* ck);

// 追加单字节
void msChunkEmit(struct MsChunk* ck, uint8_t byte, uint32_t line);

// 追加操作码（无参数指令）
static inline void msChunkEmitOp(struct MsChunk* ck, MsOpCode op,
                                  uint32_t line) {
  msChunkEmit(ck, (uint8_t)op, line);
}

// 追加操作码 + 1 字节参数 A（0~255）
void msChunkEmitOpA(struct MsChunk* ck, MsOpCode op, uint8_t a,
                    uint32_t line);

// 追加操作码 + 3 字节操作数 AX（24 位大端，跳转用有符号解读）
void msChunkEmitOpAX(struct MsChunk* ck, MsOpCode op, uint32_t ax,
                     uint32_t line);

// 追加常量到池，返回索引
uint32_t msChunkAddConst(struct MsChunk* ck, MsValue val);

// 行号查询（给定字节码偏移，返回行号）
uint32_t msChunkGetLine(const struct MsChunk* ck, uint32_t offset);

// 跳转回填：在 patchOffset 处写入 3 字节有符号相对偏移
// relOffset = targetOffset - (patchOffset + 4)（相对下一条指令）
void msChunkPatchJump(struct MsChunk* ck, uint32_t patchOffset,
                      uint32_t targetOffset);
```

### 4. 指令编码规则

- 所有指令格式：`[opcode 1B] [操作数 0/1/3B]`
- `A` = 1 字节操作数（0~255），用于局部变量槽号、argc、upvalue 索引等
- `AX` = 3 字节操作数（大端，无符号 0~16 777 215；跳转指令中以**有符号 24 位补码**解读，偏移范围 ±8 388 607）
- 跳转偏移一律为 3 字节有符号 AX，表示**相对下一条指令**的字节偏移；负值实现回跳（循环）

### 5. 行号表编码

行号表采用并行数组：`lines[i]` 存储 `code[i]` 所在源码行号（与 vm.md §2 完全一致）。

```c
// msChunkEmit 每追加一字节时同步追加对应行号
// msChunkGetLine(offset) → 直接返回 lines[offset]
```

`linesLen` 始终等于 `codeLen`，随字节码增长同步扩容（与 `code` 数组的增长策略相同）。

---

## 验收标准（checklist）

- [ ] `msChunkInit` + `msChunkEmitOp(ck, OP_CONST_NIL, 1)` + `msChunkEmitOp(ck, OP_RETURN, 1)` → `codeLen=2`，`code=[OP_CONST_NIL, OP_RETURN]`。<!-- v:ctest:T037_chunk_basic -->
- [ ] `msChunkAddConst` 添加相同 `MsValue(INT, 42)` 两次 → 返回不同索引（常量池不去重；去重优化留后续）。<!-- v:ctest:T037_chunk_const -->
- [ ] `msChunkGetLine(0)` 返回正确行号（`lines[0]` 直接读取）。<!-- v:ctest:T037_chunk_line -->
- [ ] `msChunkPatchJump` 回填后，`patchOffset` 处 3 字节存储正确的有符号相对偏移（正向跳转）；另有负偏移（回跳）用例验证。<!-- v:ctest:T037_patch_jump -->
- [ ] AddressSanitizer 下无内存泄漏（`msChunkFree` 完整释放）。<!-- v:ctest:T037_chunk_asan -->
- [ ] 超过 256 个常量时 `msChunkAddConst` 仍正常工作（返回 `uint32_t` 索引，与 AX 24 位上限一致）。<!-- v:ctest:T037_const_overflow -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_chunk.c`）

```c
#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_opcode.h"
#include "mslang/ms_value.h"

static void testChunkBasic(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  msChunkEmitOp(&ck, OP_CONST_NIL, 1);
  msChunkEmitOp(&ck, OP_RETURN,    1);

  MS_ASSERT_EQ(ck.codeLen, 2, "2 bytes");
  MS_ASSERT_EQ(ck.code[0], OP_CONST_NIL, "CONST_NIL");
  MS_ASSERT_EQ(ck.code[1], OP_RETURN,    "RETURN");
  MS_ASSERT_EQ(msChunkGetLine(&ck, 0), 1, "line 1");

  msChunkFree(&ck);
}

static void testChunkAddConst(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsValue v = MS_INT_VAL(42);
  uint32_t idx = msChunkAddConst(&ck, v);
  MS_ASSERT_EQ(idx, 0, "first const idx=0");
  MS_ASSERT_EQ(ck.constLen, 1, "1 const");

  msChunkFree(&ck);
}

static void testPatchJump(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  // emit: JUMP [placeholder AX=0x000000] CONST_NIL
  // OP_JUMP(1B) + AX(3B) = 4 字节，placeholder 写 patchOffset=1
  msChunkEmitOpAX(&ck, OP_JUMP, 0, 1);
  msChunkEmitOp(&ck, OP_CONST_NIL, 1);

  // targetOffset = 5（CONST_NIL 的偏移）
  // 相对偏移 = targetOffset - (patchOffset + 4) = 5 - (1 + 4) = 0
  // 即 JUMP 后紧接着就是 target，rel=0
  uint32_t targetOffset = ck.codeLen - 1;
  msChunkPatchJump(&ck, 1, targetOffset);

  // 验证 code[1..3] 存储了 3 字节有符号相对偏移
  int32_t rel = ((int32_t)ck.code[1] << 16) | ((int32_t)ck.code[2] << 8)
                | ck.code[3];
  // 符号扩展：若 bit23 为 1 则扩展
  if (rel & 0x800000) rel |= (int32_t)0xFF000000;
  MS_ASSERT_EQ(rel, 0, "rel offset=0 for forward jump to next instr");

  // 回跳（负偏移）：CONST_NIL → JUMP 回到自身（rel=-5）
  struct MsChunk ck2;
  msChunkInit(&ck2, NULL);
  msChunkEmitOp(&ck2, OP_CONST_NIL, 1);
  uint32_t loopStart = 0;
  msChunkEmitOpAX(&ck2, OP_JUMP, 0, 1);  // patchOffset=1
  // target=loopStart=0，相对偏移 = 0 - (1 + 4) = -5
  msChunkPatchJump(&ck2, 1, loopStart);
  int32_t rel2 = ((int32_t)ck2.code[1] << 16) | ((int32_t)ck2.code[2] << 8)
                 | ck2.code[3];
  if (rel2 & 0x800000) rel2 |= (int32_t)0xFF000000;
  MS_ASSERT_EQ(rel2, -5, "rel offset=-5 for backward jump");

  msChunkFree(&ck);
  msChunkFree(&ck2);
}

int main(void) {
  MS_RUN(testChunkBasic);
  MS_RUN(testChunkAddConst);
  MS_RUN(testPatchJump);
  return msTestSummary();
}
```

---

## .ms 使用示例

N/A（chunk 是内部实现；通过 `mslang disasm`（T048）验证）。

---

## Benchmark

N/A（chunk 操作开销在 T048 中通过 disasm 间接评估）。

---

## 风险与边界

- **24 位跳转范围**：有符号 AX 偏移支持 ±8 388 607 字节，对单函数体足够；超出时编译器应报错（实际不可能发生）。
- **常量池去重**：初版不去重（每次 `msChunkAddConst` 追加新条目）；字符串常量可能重复，不影响正确性，只影响常量池大小。后续优化可在 compiler 层对 string/int 常量去重。
- **`MsValue` 生命周期**：常量池存储 `MsValue`；若值为对象指针（`MS_TAG_OBJ`），GC 需在扫描时将常量池作为根（T117）。
