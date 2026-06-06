# P2-T036 Program 顶层 + `parse` 子命令 + AST golden 测试

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `msParseProgram`（解析整个源文件为 `ND_PROGRAM` 根节点）和 `mslang parse <file>` CLI 子命令（以可读文本格式打印 AST），并搭建 AST golden 测试套件，是 P2 解析阶段的收尾任务。

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
| `syntax.md` | §2.9 Program（顶层语句列表） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增 / 修改文件

```
src/parser/ms_ast_print.c        # AST 文本序列化
include/mslang/ms_parser.h       # 导出 msAstPrint
src/cli.c                        # 实现 cliRunParse(ctx)
tests/golden/parser/             # golden 输入与期望输出
benchmarks/bench_parser.c        # 解析 benchmark
```

---

## 实现要点

### 1. `msParseProgram`

```c
MsNode* msParseProgram(MsParser* p) {
    MsSrcPos pos = p->cur.pos;

    MsNodeList* stmts = NULL;
    MsNodeList** tail = &stmts;

    // 跳过文件首的换行/分号
    while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}

    while (!check(p, TOK_EOF)) {
        MsNode* stmt = msParseStmt(p);
        if (stmt) {
            MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
            item->node = stmt; item->next = NULL;
            *tail = item; tail = &item->next;
        }
        // 语句分隔符
        if (!match(p, TOK_NEWLINE) && !match(p, TOK_SEMICOLON)) {
            if (!check(p, TOK_EOF)) {
                parserError(p, "expected newline or ';' after statement");
                syncError(p);
            }
        }
        while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}
    }

    MsNode* prog = MS_ARENA_NEW(p->arena, MsNode);
    prog->kind           = ND_PROGRAM;
    prog->pos            = pos;
    prog->program.stmts  = stmts;
    prog->program.filename = p->lex.fileName;
    return prog;
}
```

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
void msAstPrint(MsNode* node, int indent, FILE* fp);
```

每个节点类型打印固定前缀（`KIND_NAME`），关键字段紧跟在同一行，子节点递归缩进。

### 3. `cliRunParse`

```c
void cliRunParse(MsCliCtx* ctx) {
    // 1. 读取 ctx->script 文件
    // 2. 初始化 MsArena
    // 3. msParserInit + msParseProgram
    // 4. 若有错误，打印到 stderr 并以非零退出
    // 5. msAstPrint(program, 0, stdout)
    // 6. msArenaFree
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
int main(void) {
    const char* src = loadFile("benchmarks/data/bench_300l.ms");
    uint32_t srcLen = (uint32_t)strlen(src);
    uint64_t totalNodes = 0;
    clock_t start = clock();

    for (int i = 0; i < 1000; i++) {
        MsArena arena; msArenaInit(&arena);
        MsParser p;
        msParserInit(&p, src, srcLen, "bench", &arena);
        MsNode* prog = msParseProgram(&p);
        totalNodes += countNodes(prog);  // 辅助：递归计数
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
- [ ] 全部 parser golden 测试（`ctest -R parser_*`）通过。
- [ ] 语法错误输入打印包含行号的错误消息，并以非零退出。
- [ ] `msParseProgram` 对空文件返回 `ND_PROGRAM(stmts=NULL)`（不报错）。
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
- **错误恢复**：parser 在错误恢复后仍可打印部分 AST；golden 测试应只针对合法输入（错误输入单独测试退出码）。
- **Windows 行尾**：golden `.expected` 文件必须 LF 行尾（`CLAUDE.md §编码`）；golden runner 在比对前标准化行尾。
