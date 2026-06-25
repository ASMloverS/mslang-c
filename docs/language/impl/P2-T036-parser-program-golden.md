# P2-T036 Program 顶层 + `parse` 子命令 + AST golden 测试

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `mslang parse <file>` CLI 子命令（以可读文本格式打印 AST），并搭建 AST golden 测试套件，是 P2 解析阶段的收尾任务。`msParseProgram` 已在之前的 parser 任务中实现（`src/parser/ms_parser.c:1062`），本任务直接复用。顶层 `FuncDecl`/`ClassDecl`/`ImportDecl` 经由 `msParseStmt` 分派处理。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T017 ~ T035 | 全部 parser 子系统 |
| P0-T003 | golden runner |
| P0-T004 | CLI 子命令框架（`CLI_CMD_PARSE`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.1 顶层结构（Program 文法） |
| `syntax.md` | §2.2 语句（语句分隔/终止规则） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增 / 修改文件

```
src/parser/ms_ast_print.c        # AST 文本序列化（msAstPrint）
include/mslang/ms_parser.h       # 导出 msAstPrint
src/core/ms_cli.c                # 实现 cmdParse(ctx)（已有占位桩）
tests/golden/parser/             # golden 输入与期望输出
benchmarks/bench_parser.c        # 解析 benchmark
```

---

## 实现要点

### 1. `msParseProgram`（已实现，本任务复用）

`msParseProgram` 已在 `src/parser/ms_parser.c:1062` 完整实现并导出（`ms_parser.h:80`），行为如下：

- 构造 `MS_ND_PROGRAM` 根节点，`program.filename` 取自 lexer 文件名
- 循环：跳过 `MS_TOK_NEWLINE`/`MS_TOK_SEMICOLON`，调用 `msParseStmt(p)` 解析各语句
- 顶层 `FuncDecl`/`ClassDecl`/`ImportDecl` 经由 `msParseStmt` 分派（T034/T035 已实现）
- 语句终止符处理依赖 ASI 规则：`MS_TOK_NEWLINE` 为 lexer 自动插入的虚拟 `';'`，与显式 `MS_TOK_SEMICOLON` 在语句分隔处等价
- 以 `msNodeListAppend` 追加到 `program.stmts` 链表
- 遇 `MS_TOK_EOF` 时结束，返回 `MS_ND_PROGRAM` 节点

本任务无需修改 `msParseProgram`。

### 2. AST 文本格式（`msAstPrint`）

采用缩进树格式，每行一个节点，子节点多缩进 2 空格：

```
PROGRAM
  FUNC_DECL name=greet
    PARAM a
    PARAM greeting default=
      STRING "Hello"
    BLOCK
      RETURN
        FSTRING ...
  VAR_DECL name=x
    INT 42
  EXPR_STMT
    CALL
      IDENT print
      IDENT x
```

```c
// 以缩进树格式打印 AST 节点（2 空格层级递增，指 AST 输出格式，非源码缩进）。
// node: 要打印的节点；indent: 当前缩进层级（根节点传 0）；fp: 输出流。
void msAstPrint(MsNode* node, int indent, FILE* fp);
```

每个节点类型打印固定前缀（`KIND_NAME`），关键字段紧跟在同一行，子节点递归缩进。声明位置参照 `msTokenPrint`（`ms_lexer.h:166`）的约定，导出到 `include/mslang/ms_parser.h`。

### 3. `cmdParse`（`src/core/ms_cli.c`）

`cmdParse` 桩函数已在 `ms_cli.c:266` 存在（当前仅打印 "not implemented yet"），本任务替换为完整实现。参照 `cmdTokens`（`ms_cli.c:238`）的范式：

```c
static int cmdParse(struct MsCliCtx* ctx) {
  if (!ctx->script) {
    fprintf(stderr, "mslang parse: no input file\n");
    return 1;
  }
  uint32_t srcLen = 0;
  char* src = readFileAll(ctx->script, &srcLen);
  if (!src) {
    return 1;
  }
  struct MsArena arena;
  msArenaInit(&arena);
  MsParser p;
  msParserInit(&p, src, srcLen, ctx->script, &arena);
  MsNode* prog = msParseProgram(&p);

  if (p.hadError) {
    msArenaFree(&arena);
    free(src);
    return 1;
  }
  msAstPrint(prog, 0, stdout);
  msArenaFree(&arena);
  free(src);
  return 0;
}
```

### 4. Golden 测试文件

```
tests/golden/parser/
  hello.ms          # print("hello")
  hello.expected    # PROGRAM\n  EXPR_STMT\n    CALL\n      ...
  func_decl.ms
  func_decl.expected
  class_decl.ms
  class_decl.expected
  control_flow.ms   # if/for/while/switch
  control_flow.expected
  import_stmts.ms
  import_stmts.expected
  full_program.ms   # 综合测试
  full_program.expected
```

### 5. Parse Benchmark（`benchmarks/bench_parser.c`）

```c
// 重复解析 ~300 行 .ms 程序 1000 次
// 指标：parse nodes/sec（> 5M nodes/sec 为合理目标）
// loadFile / countNodes 为本文件内辅助函数（需自行实现）
int main(void) {
  const char* src = loadFile("benchmarks/data/bench_300l.ms");
  if (src == NULL) {
    return 1;
  }
  uint32_t srcLen = (uint32_t)strlen(src);
  uint64_t totalNodes = 0;
  clock_t start = clock();

  for (int i = 0; i < 1000; i++) {
    struct MsArena arena;
    msArenaInit(&arena);
    MsParser p;
    msParserInit(&p, src, srcLen, "bench", &arena);
    MsNode* prog = msParseProgram(&p);
    totalNodes += countNodes(prog);
    msArenaFree(&arena);
  }

  double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
  printf("%.2f M nodes/sec\n", (double)totalNodes / elapsed / 1e6);
  return 0;
}
```

---

## 验收标准（checklist）

- [ ] `mslang parse tests/golden/parser/hello.ms` 输出与 `hello.expected` 逐字节一致。
- [ ] `func_decl.ms` golden 测试通过（函数声明 + 参数 + 返回语句）。
- [ ] `class_decl.ms` golden 测试通过（class/extends/方法）。
- [ ] `import_stmts.ms` golden 测试通过（import/dotted path/as）。
- [ ] `control_flow.ms` golden 测试通过（if/for/switch）。
- [ ] `full_program.ms` golden 测试通过（综合测试：顶层 func/class/import + 语句）。
- [ ] 全部 parser golden 测试（`ctest -R parser_*`）通过。
- [ ] 语法错误输入打印包含行号的错误消息，并以非零退出（`cmdParse` 返回非零）。
- [ ] `msParseProgram` 对空文件返回 `MS_ND_PROGRAM(stmts=NULL)`（不报错）。
- [ ] benchmark 在 Release 构建中运行并打印 `nodes/sec`。
- [ ] AddressSanitizer 下无内存泄漏（arena 完整释放）。

---

## Golden 文件内容规格

### `hello.ms`

```ms
print("hello, world")
```

### `hello.expected`

```
PROGRAM
  EXPR_STMT
    CALL
      IDENT print
      STRING "hello, world"
```

### `func_decl.ms`

```ms
func add(a, b) {
    return a + b
}
```

### `func_decl.expected`

```
PROGRAM
  FUNC_DECL name=add
    PARAM a
    PARAM b
    BLOCK
      RETURN
        BINARY +
          IDENT a
          IDENT b
```

---

## .ms 使用示例

```sh
# 打印 AST
mslang parse program.ms

# 管道过滤特定节点
mslang parse program.ms | grep FUNC_DECL

# 语法错误检测
mslang parse broken.ms 2>&1; echo "Exit: $?"
```

---

## Benchmark

### 解析 benchmark（`benchmarks/bench_parser.c`）

| 指标 | 目标（Release `-O2`） |
|---|---|
| parse nodes/sec | > 5M |
| 内存峰值/次解析 | < 10MB（arena 控制分配量） |

---

## 风险与边界

- **AST 打印格式稳定性**：golden 测试严格比对文本，任何格式更改都会导致测试失败。建议在 P2 完成后将 `msAstPrint` 输出格式冻结（加注释"格式不向后兼容"）。
- **语句终止符与 ASI**：`syntax.md §2.2` 文法中语句以 `';'` 结尾，实际由 lexer ASI 规则（P1-T015）自动插入虚拟 `MS_TOK_NEWLINE`（`ms_lexer.h:68`）。`MS_TOK_NEWLINE` 与显式 `MS_TOK_SEMICOLON` 在语句分隔处等价，`msParseProgram` 两者均接受。
- **错误恢复**：parser 在错误恢复后仍可打印部分 AST；golden 测试应只针对合法输入（错误输入单独测试退出码）。
- **Windows 行尾**：golden `.expected` 文件必须 LF 行尾（`CLAUDE.md §编码`）；golden runner 在比对前标准化行尾。
