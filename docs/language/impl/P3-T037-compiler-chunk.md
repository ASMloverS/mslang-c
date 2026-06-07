# P3-T037 MsChunk：字节码块、常量池、行号表

> **状态**：⬜ 未开始

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
| `vm.md` | §2 字节码格式（指令集/操作码定义） |
| `vm.md` | §2.1 MsChunk 结构 |
| `vm.md` | §2.2 常量池（MsValue 数组） |
| `vm.md` | §2.3 行号表（RLE 压缩） |

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
typedef enum MsOpCode {
  // ─── 常量 / 栈操作 ────────────────────────────────────────────
  OP_CONST,       // 压入常量池[arg]
  OP_NIL,         // 压入 nil
  OP_TRUE,        // 压入 true
  OP_FALSE,       // 压入 false
  OP_POP,         // 弹出栈顶（丢弃）

  // ─── 局部变量 ─────────────────────────────────────────────────
  OP_GET_LOCAL,   // 压入局部变量[arg]
  OP_SET_LOCAL,   // 弹出栈顶→局部变量[arg]
  OP_GET_GLOBAL,  // 压入全局变量（按名称 const[arg]）
  OP_SET_GLOBAL,
  OP_DEL_GLOBAL,  // 删除全局变量

  // ─── Upvalue（闭包捕获）─────────────────────────────────────────
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_CLOSE_UPVALUE,  // 将 upvalue 迁移到堆

  // ─── 算术/位/比较 ─────────────────────────────────────────────
  OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
  OP_NEG,   // 一元取负
  OP_NOT,   // 逻辑非
  OP_BITNOT,// ~
  OP_BITAND, OP_BITOR, OP_BITXOR,
  OP_SHL, OP_SHR,
  OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
  OP_IS, OP_IS_NOT, OP_IN, OP_NOT_IN,

  // ─── 跳转 ─────────────────────────────────────────────────────
  OP_JUMP,          // 无条件跳转（+arg 偏移）
  OP_JUMP_FALSE,    // 栈顶为假则跳转（不弹栈）
  OP_JUMP_TRUE,     // 栈顶为真则跳转（不弹栈）
  OP_POP_JUMP_FALSE,// 弹栈后若假跳转
  OP_POP_JUMP_TRUE,
  OP_LOOP,          // 向后跳转（循环）

  // ─── 容器构建 ─────────────────────────────────────────────────
  OP_BUILD_LIST,    // arg=元素个数，弹出 arg 个元素构建 list
  OP_BUILD_MAP,     // arg=键值对数
  OP_BUILD_SET,
  OP_BUILD_TUPLE,
  OP_UNPACK,        // arg=目标个数（迭代解包到局部变量）
  OP_BUILD_SLICE,   // 从栈顶 3 个元素构建 slice（lo/hi/step）

  // ─── 属性/下标 ────────────────────────────────────────────────
  OP_GET_ATTR,      // 栈顶对象，const[arg] 为名称
  OP_SET_ATTR,
  OP_DEL_ATTR,
  OP_GET_INDEX,     // 栈[-2]=obj, 栈[-1]=key
  OP_SET_INDEX,     // 栈[-3]=obj, 栈[-2]=key, 栈[-1]=val
  OP_DEL_INDEX,

  // ─── 调用 ─────────────────────────────────────────────────────
  OP_CALL,          // arg=位置参数个数（callee 在栈底）
  OP_CALL_KW,       // arg=位置参数个数，下一 const[arg2] 存 kwarg 名称 tuple
  OP_CALL_EX,       // 展开调用（*args/**kwargs）
  OP_RETURN,        // 返回栈顶值

  // ─── 函数/类 ──────────────────────────────────────────────────
  OP_MAKE_FUNC,     // arg=upvalue 个数，const[next] 存 struct MsChunk* 指针（函数原型）
  OP_MAKE_CLASS,    // arg=方法数，const[next] 存类名
  OP_GET_SUPER,     // 压入 super 代理

  // ─── 迭代 ─────────────────────────────────────────────────────
  OP_GET_ITER,      // 将栈顶转为迭代器
  OP_FOR_ITER,      // 迭代器取下一个，若耗尽跳转(+arg)

  // ─── 异常 ─────────────────────────────────────────────────────
  OP_PUSH_EXCEPT,   // 注册异常处理器（+arg 到 handler）
  OP_POP_EXCEPT,    // 出栈异常处理器
  OP_RAISE,         // 抛出异常
  OP_RERAISE,       // 重抛当前异常
  OP_WITH_ENTER,    // 调用 __enter__
  OP_WITH_EXIT,     // 调用 __exit__

  // ─── 并发 ─────────────────────────────────────────────────────
  OP_GO,            // 启动 goroutine
  OP_MAKE_CHAN,     // 创建 channel
  OP_CHAN_SEND,     // channel 发送
  OP_CHAN_RECV,     // channel 接收
  OP_SELECT,        // select 多路复用

  // ─── 断言/调试 ─────────────────────────────────────────────────
  OP_ASSERT,        // 断言（含消息）
  OP_IMPORT,        // 导入模块（const[arg]=模块名）
  OP_IMPORT_FROM,   // 从模块导入名称

  // ─── 特殊 ─────────────────────────────────────────────────────
  OP_AWAIT,         // await 表达式
  OP_ISINSTANCE,    // isinstance(obj, type)

  OP_COUNT,         // sentinel
} MsOpCode;
```

### 2. `MsChunk` 结构体

```c
// include/mslang/ms_chunk.h
struct MsChunk {
  uint8_t*    code;      // 字节码序列（动态数组）
  uint32_t    codeLen;
  uint32_t    codeCap;

  MsValue*    consts;    // 常量池（MsValue 数组）
  uint32_t    constLen;
  uint32_t    constCap;

  // 行号表（RLE 压缩：[bytecode_count, line] 对的序列）
  uint16_t*   lines;     // 行号 RLE 数组
  uint32_t    lineLen;
  uint32_t    lineCap;
  uint32_t    lineLastBc; // 上一次记录的字节码位置
  uint32_t    lineLastLine; // 上一次记录的行号

  const char* fileName;  // 源文件名（用于错误报告）
};
```

### 3. Emit API

```c
void msChunkInit(struct MsChunk* ck, const char* fileName);
void msChunkFree(struct MsChunk* ck);

// 追加单字节
void msChunkEmit(struct MsChunk* ck, uint8_t byte, uint32_t line);

// 追加操作码
static inline void emitOp(struct MsChunk* ck, MsOpCode op, uint32_t line) {
  msChunkEmit(ck, (uint8_t)op, line);
}

// 追加操作码 + 1 字节参数
void emitOp8(struct MsChunk* ck, MsOpCode op, uint8_t arg, uint32_t line);

// 追加操作码 + 2 字节参数（大端）
void emitOp16(struct MsChunk* ck, MsOpCode op, uint16_t arg, uint32_t line);

// 追加常量到池，返回索引
uint16_t msChunkAddConst(struct MsChunk* ck, MsValue val);

// 行号查询（给定字节码偏移，返回行号）
uint32_t msChunkGetLine(const struct MsChunk* ck, uint32_t offset);

// 跳转回填：在 offset 处写入 2 字节目标偏移
void msChunkPatchJump(struct MsChunk* ck, uint32_t patchOffset, uint32_t targetOffset);
```

### 4. 指令编码规则

- 所有指令固定格式：`[opcode 1B] [arg 0/1/2B]`
- 操作码分类：无参数（`OP_ADD` 等）、1 字节参数（小 arg）、2 字节参数（常量池索引 / 跳转偏移）
- 参数 > 255 时使用 2 字节；跳转偏移始终 2 字节（`uint16_t`，大端），支持 ±32767 偏移（对单函数已足够）
- 大函数（> 65535 字节）初版不支持

### 5. 行号 RLE 编码

```c
// RLE：记录 [count, line] 对
// 例：字节码 [0..5] 来自第 3 行，[6..12] 来自第 4 行 →
// lines = [6, 3, 7, 4]（count 为字节数，非字节序号）
// msChunkGetLine(offset) → 线性扫描 RLE 找到 offset 所属的 line
void msChunkEmitLine(struct MsChunk* ck, uint32_t line) {
  if (ck->lineLen > 0 && ck->lines[ck->lineLen - 1] == (uint16_t)line) {
    ck->lines[ck->lineLen - 2]++;  // 同一行，count+1
  } else {
    // 新行
    if (ck->lineLen + 2 > ck->lineCap) growLines(ck);
    ck->lines[ck->lineLen++] = 1;
    ck->lines[ck->lineLen++] = (uint16_t)line;
  }
}
```

---

## 验收标准（checklist）

- [ ] `msChunkInit` + `emitOp(ck, OP_NIL, 1)` + `emitOp(ck, OP_RETURN, 1)` → `codeLen=2`，`code=[OP_NIL, OP_RETURN]`。
- [ ] `msChunkAddConst` 添加相同 `MsValue(INT, 42)` 两次 → 返回不同索引（常量池不去重；去重优化留后续）。
- [ ] `msChunkGetLine(0)` 返回正确行号（RLE 解码）。
- [ ] `msChunkPatchJump` 回填 2 字节偏移后，chunk 对应偏移处字节正确。
- [ ] AddressSanitizer 下无内存泄漏（`msChunkFree` 完整释放）。
- [ ] 超过 256 个常量时 `msChunkAddConst` 仍正常工作（2 字节索引）。

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
  msChunkInit(&ck, "<t>");

  emitOp(&ck, OP_NIL,    1);
  emitOp(&ck, OP_RETURN, 1);

  MS_ASSERT_EQ(ck.codeLen, 2, "2 bytes");
  MS_ASSERT_EQ(ck.code[0], OP_NIL,    "NIL");
  MS_ASSERT_EQ(ck.code[1], OP_RETURN, "RETURN");
  MS_ASSERT_EQ(msChunkGetLine(&ck, 0), 1, "line 1");

  msChunkFree(&ck);
}

static void testChunkAddConst(void) {
  struct MsChunk ck;
  msChunkInit(&ck, "<t>");

  MsValue v = MS_INT_VAL(42);
  uint16_t idx = msChunkAddConst(&ck, v);
  MS_ASSERT_EQ(idx, 0, "first const idx=0");
  MS_ASSERT_EQ(ck.constLen, 1, "1 const");

  msChunkFree(&ck);
}

static void testPatchJump(void) {
  struct MsChunk ck;
  msChunkInit(&ck, "<t>");

  // emit: JUMP [placeholder 0x00 0x00] NOP
  emitOp8(&ck, OP_JUMP, 0, 1);  // 占位（初版 OP_JUMP 为 1+2 字节）
  // 写个假操作码
  emitOp(&ck, OP_NIL, 1);

  uint32_t targetOffset = ck.codeLen;
  msChunkPatchJump(&ck, 1, targetOffset);  // 回填位置 1（OP_JUMP 的参数）
  // 验证 code[1..2] 写入了 targetOffset
  uint16_t patched = ((uint16_t)ck.code[1] << 8) | ck.code[2];
  MS_ASSERT_EQ(patched, (uint16_t)targetOffset, "patched jump");

  msChunkFree(&ck);
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

- **2 字节跳转限制**：`uint16_t` 跳转偏移支持最大 65535 字节函数体；极端大函数（> 64KB 字节码）初版不支持，报编译错误。
- **常量池去重**：初版不去重（每次 `msChunkAddConst` 追加新条目）；字符串常量可能重复，不影响正确性，只影响常量池大小。后续优化可在 compiler 层对 string/int 常量去重。
- **`MsValue` 生命周期**：常量池存储 `MsValue`；若值为对象指针（`MS_TAG_OBJ`），GC 需在扫描时将常量池作为根（T117）。
