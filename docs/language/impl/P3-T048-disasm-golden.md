# P3-T048 反汇编器（disasm）+ P3 里程碑 golden 测试

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现字节码反汇编器（`msChunkDisasm`）与 `mslang disasm <file>` CLI 子命令，输出人类可读的指令列表。配套 golden 测试覆盖 P1–P3 的 lexer / parser / compiler 三层输出，同时提供整体编译 benchmark。此任务是 P3 阶段的**里程碑收口**（M0.5）：确保词法、语法、编译三层流水线端到端可验证。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T037 | `MsChunk` 结构与 emit 工具 |
| P3-T046 | 所有编译器 compileXxx 实现完成 |
| P1-T016 | `tokens` 子命令 golden 框架 |
| P2-T036 | `parse` 子命令 golden 框架 |
| P0-T003 | golden runner / CTest 基础设施 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §2 MsChunk 结构（`lines` 扁平逐字节行号表 / 常量池） |
| `vm.md` | §3 指令集与 `A`/`AX` 操作数宽度（`AX` = 3 字节大端） |
| `vm.md` | §8 反汇编器输出格式 |
| `vm.md` | §9 opcode 命名映射（实现层 `OP_` 前缀，`msChunkGetLine`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/compiler/ms_disasm.c        # msChunkDisasm / msDisasmInstr
include/mslang/ms_disasm.h      # 公共声明
```

### 修改文件

```
src/cli/ms_cli.c                # "disasm" 子命令
tests/golden/disasm/            # golden 输出文件
tests/golden/disasm/*.ms        # 输入 .ms 文件（实际是 tokens/parse/disasm 三套共用源）
```

---

## 实现要点

### 1. 反汇编器 API

```c
// 反汇编整个 chunk（含嵌套函数 proto）
void msChunkDisasm(const MsChunk* chunk, const char* name, FILE* fp);

// 反汇编单条指令，返回消费的字节数（含操作码本身）
int msDisasmInstr(const MsChunk* chunk, uint32_t offset, FILE* fp);
```

### 2. 输出格式

```
== <name> ==
0000    1 OP_CONST         0    '42'
0004    | OP_POP
0005    2 OP_GET_GLOBAL    1    'print'
0009    | OP_CONST         2    'hello'
000D    | OP_CALL          1
000F    | OP_RETURN
```

列含义：
- 列 1：字节偏移（4 位十六进制，不带前缀）
- 列 2：行号（与前一指令同行时显示 `|`）
- 列 3：操作码助记符（左对齐，固定宽度 20 字符）
- 列 4+：操作数（索引 + 对应常量的值/字符串预览）

> 本反汇编器打印**实现层 `OP_` 枚举名**（vm.md §9 明确实现层用 `OP_` 前缀）及内联常量预览（`'...'`），与 vm.md §8 示例的规范助记名 + `; 注释` 样式不同——因反汇编对象是实现层 `MsChunk`/`MsOpCode`，此为有意偏离。

嵌套函数 proto 递归反汇编：

```
== <top-level> ==
...
== <function 'add'> ==
0000    1 OP_GET_LOCAL     0    'a'
0002    | OP_GET_LOCAL     1    'b'
...
```

### 3. 指令反汇编实现骨架

```c
int msDisasmInstr(const MsChunk* chunk, uint32_t offset, FILE* fp) {
  uint32_t line = msChunkGetLine(chunk, offset);
  // prevLine 取前一字节所在行；lines[] 为逐字节表，指令内各字节同行，故等价"前一指令行"
  uint32_t prevLine = (offset > 0) ? msChunkGetLine(chunk, offset - 1) : 0;
  char lineNumStr[8];
  snprintf(lineNumStr, sizeof(lineNumStr), "%u", line);
  const char* lineStr = (offset > 0 && line == prevLine) ? "|" : lineNumStr;

  fprintf(fp, "%04X %4s ", offset, lineStr);
  uint8_t op = chunk->code[offset];
  switch (op) {
  // AX 指令（CONST / GET_GLOBAL / SET_GLOBAL / GET_ATTR 等）：3 字节大端操作数，消耗 4 字节
  case OP_CONST: {
    uint32_t idx = (chunk->code[offset+1] << 16) |
                   (chunk->code[offset+2] << 8) | chunk->code[offset+3];
    fprintf(fp, "%-20s %5u  ", "OP_CONST", idx);
    msPrintConst(chunk->constants[idx], fp);
    fprintf(fp, "\n");
    return 4;
  }
  // 无操作数指令：消耗 1 字节
  case OP_POP:
    fprintf(fp, "OP_POP\n");
    return 1;
  // A 指令（CALL / GET_LOCAL 等）：1 字节操作数，消耗 2 字节；以此类推处理所有 ~60+ 操作码
  default:
    fprintf(fp, "UNKNOWN(%02X)\n", op);
    return 1;
  }
}
```

### 4. 常量值打印

```c
static void msPrintConst(MsValue v, FILE* fp) {
  switch (MS_GET_TAG(v)) {
  case MS_TAG_INT:   fprintf(fp, "%" PRId64, MS_AS_INT(v));     break;
  case MS_TAG_FLOAT: fprintf(fp, "%g",       MS_AS_FLOAT(v));   break;
  case MS_TAG_BOOL:  fprintf(fp, "%s",       MS_AS_BOOL(v) ? "true" : "false"); break;
  case MS_TAG_NIL:   fprintf(fp, "nil");                         break;
  case MS_TAG_OBJ: {
    MsObject* obj = MS_AS_OBJ(v);
    if (obj->type == &msStrType) {
      fprintf(fp, "'");
      // 截断打印（最多 40 个字节）
      fwrite(((MsStr*)obj)->data, 1, MIN(40, ((MsStr*)obj)->len), fp);
      if (((MsStr*)obj)->len > 40) {
        fprintf(fp, "...");
      }
      fprintf(fp, "'");
    } else if (obj->type == &msFuncProtoType) {
      fprintf(fp, "<func '%s'>", ((MsFuncProto*)obj)->name);
    } else {
      fprintf(fp, "<%s>", obj->type->name);
    }
    break;
  }
  default: fprintf(fp, "?");
  }
}
```

### 5. CLI 子命令

```c
// src/cli/ms_cli.c
static int cmdDisasm(int argc, char** argv) {
  if (argc < 2) {
    fputs("usage: mslang disasm <file>\n", stderr);
    return 1;
  }
  const char* path = argv[1];
  char* src = msReadFile(path);  // 读入源码
  if (!src) {
    perror(path);
    return 1;
  }

  MsCompileResult result = msCompile(src, (uint32_t) strlen(src), path);
  free(src);
  if (result.hadError) {
    fprintf(stderr, "compile error: %s\n", result.errBuf);
    msCompileResultFree(&result);
    return 1;
  }
  msChunkDisasm(result.chunk, path, stdout);
  msCompileResultFree(&result);
  return 0;
}
```

---

## Golden 测试框架

### 目录布局

```
tests/golden/
  lexer/
    basic.ms           # 输入
    basic.tokens       # 期望输出（mslang tokens basic.ms）
  parser/
    basic.ms
    basic.ast          # 期望输出（mslang parse basic.ms）
  disasm/
    arith.ms
    arith.disasm       # 期望输出（mslang disasm arith.ms）
    func.ms
    func.disasm
    closure.ms
    closure.disasm
    try_catch.ms
    try_catch.disasm
    import.ms
    import.disasm
```

### Golden Runner

```bash
#!/bin/bash
# tests/run_golden.sh
MSLANG=./build/mslang
PASS=0; FAIL=0

run_case() {
    local input="$1" expected="$2" cmd="$3"
    actual=$($MSLANG $cmd "$input" 2>&1)
    if [ "$actual" = "$(cat "$expected")" ]; then
        echo "PASS $input"
        ((PASS++))
    else
        echo "FAIL $input"
        diff <(echo "$actual") "$expected"
        ((FAIL++))
    fi
}

for ms in tests/golden/lexer/*.ms; do
    run_case "$ms" "${ms%.ms}.tokens" "tokens"
done
for ms in tests/golden/parser/*.ms; do
    run_case "$ms" "${ms%.ms}.ast" "parse"
done
for ms in tests/golden/disasm/*.ms; do
    run_case "$ms" "${ms%.ms}.disasm" "disasm"
done

echo "Golden: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
```

### 示例 golden 输入/输出

**`tests/golden/disasm/arith.ms`**：
```ms
result := 1 + 2 * 3
print(result)
```

> 整数字面量统一由编译器（T046，`ms_compiler.c` `emitConst`）发 `OP_CONST` + 常量池索引，不启用 vm.md §3.1 的 `CONST_INT` 内联优化路径，故下例中 `1`/`2`/`3` 均为 `OP_CONST`。

**`tests/golden/disasm/arith.disasm`**（期望输出）：
```
== arith.ms ==
0000    1 OP_CONST              0    1
0004    | OP_CONST              1    2
0008    | OP_CONST              2    3
000C    | OP_MUL
000D    | OP_ADD
000E    | OP_SET_GLOBAL         3    'result'
0012    | OP_POP
0013    2 OP_GET_GLOBAL         4    'print'
0017    | OP_GET_GLOBAL         3    'result'
001B    | OP_CALL               1
001D    | OP_POP
001E    0 OP_RETURN_NIL
```

> 末条 `OP_RETURN_NIL` 是编译器（`ms_compiler.c:1422`）为顶层程序合成的隐式返回，行号为合成值 `0`（非源码行），故不显示 `|` 续行。

---

## 验收标准（checklist）

- [x] `mslang disasm` 子命令对任意合法 `.ms` 文件输出无崩溃。
- [x] 输出格式：`OFFSET  LINE  MNEMONIC  OPERANDS  [CONST_PREVIEW]`，行号同行显示 `|`。
- [ ] 嵌套函数 proto 递归反汇编（每个函数有独立 `== <name> ==` 块）。**未实现**：`compileFuncToConst` 当前将嵌套 chunk 编译后即丢弃、常量池存 `MS_NIL_VAL` 占位（P4-T049 引入 `MsFuncProto` 对象后才能保留 chunk 供反汇编递归），本任务范围内无法实现，经 review 判定可接受、不阻塞。
- [x] `tests/golden/lexer/` golden 测试全部通过（`mslang tokens`）。
- [x] `tests/golden/parser/` golden 测试全部通过（`mslang parse`）。
- [x] `tests/golden/disasm/` 至少 5 组 golden 测试全部通过（含 arith/func/closure/try_catch/import）。
- [x] CTest 集成：`ctest -R golden` 可运行所有 golden 测试。
- [x] 编译 benchmark ≥ **50 万行/秒**（500K 行源码文件，端到端编译到 disasm）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_disasm.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_disasm.h"

static void testDisasmNocrash(void) {
  MsCompileResult result = msCompile("x := 1 + 2\nprint(x)", 20, "<t>");
  MS_ASSERT_TRUE(!result.hadError, "no error");
  // 捕获 disasm 输出到临时文件回读（fmemopen 在 Windows MSVC 不可用，改用 tmpfile）
  FILE* out = tmpfile();
  msChunkDisasm(result.chunk, "<t>", out);
  long size = ftell(out);
  rewind(out);
  char buf[4096] = {0};
  fread(buf, 1, (size < 4095 ? (size_t) size : 4095), out);
  fclose(out);
  MS_ASSERT_TRUE(strstr(buf, "OP_ADD") != NULL, "OP_ADD in output");
  MS_ASSERT_TRUE(strstr(buf, "OP_CALL") != NULL, "OP_CALL in output");
  msCompileResultFree(&result);
}

int main(void) {
  MS_RUN(testDisasmNocrash);
  return msTestSummary();
}
```

### .ms 使用示例

N/A（disasm 子命令输出为文本，非 .ms 运行产物）。

---

## Benchmark

### 编译吞吐量 benchmark

**目标**：`≥ 50 万行/秒`（测量从源码字符串到 `MsChunk` 完整产出）。

```c
// benchmarks/bench_compiler.c
#include <time.h>
#include "mslang/ms_compiler.h"

int main(void) {
  // 生成 500K 行源码（简单表达式混合），堆分配以容纳完整体量
  uint32_t cap = 32 * 1024 * 1024;
  char* src = malloc(cap);
  uint32_t len = genBenchSrc(src, cap, 500000);

  int runs = 5;
  double totalSec = 0;
  for (int i = 0; i < runs; i++) {
    clock_t t0 = clock();
    MsCompileResult result = msCompile(src, len, "<bench>");
    clock_t t1 = clock();
    totalSec += (double)(t1 - t0) / CLOCKS_PER_SEC;
    msCompileResultFree(&result);
  }

  double avgSec = totalSec / runs;
  uint32_t lines = countLines(src, len);
  printf("Compiler bench: %.2f M lines/sec\n", (lines / avgSec) / 1e6);
  free(src);
  return 0;
}
```

---

## 风险与边界

- **`fmemopen` 跨平台**：Windows MSVC 无 `fmemopen`；单测中改用临时文件（`tmpfile()`）或封装 `MsStringWriter`。
- **`OP_MAKE_FUNC` 操作数变长**：每个 upvalue 描述占 2 字节（is_local + index），反汇编时需动态读取 `upvalueCount` 字段。
- **常量池预览截断**：字符串常量超过 40 字节时截断并加 `...`，避免输出行过长。
- **P3 里程碑声明**：T048 通过后，整个词法→语法→编译→反汇编流水线可端到端验证，但尚无运行时（P4 起）。
