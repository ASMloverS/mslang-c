# P6-T083 traceback 记录与回溯打印

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现异常时的调用栈回溯（traceback）：记录异常发生时的帧链（文件名、函数名、行号），并在未处理异常时以人类可读的格式打印到 stderr。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T080 | 异常状态（`t->currentException`） |
| P3-T037 | `msChunkGetLine`（从 IP 反查行号） |

---

## 实现要点

### 1. TracebackEntry

```c
typedef struct MsTracebackEntry {
  const char* fileName;
  const char* funcName;
  uint32_t    line;
} MsTracebackEntry;

typedef struct MsTraceback {
  MsObject         header;
  MsTracebackEntry entries[64];  // 最多 64 层（足够大多数情况）
  uint32_t         count;
} MsTraceback;
```

### 2. 记录 traceback

在 `OP_RAISE` 执行时（或异常传播时），遍历当前帧链快照：

```c
static void captureTraceback(MsThread* t, MsTraceback* tb) {
  tb->count = 0;
  MsFrame* f = t->frame;
  while (f && tb->count < 64) {
    tb->entries[tb->count].fileName = f->chunk->fileName;
    tb->entries[tb->count].funcName = f->closure
      ? ((MsClosureObj*)MS_AS_OBJ(f->closure))->proto->name
      : "<module>";
    // 从 IP 反查当前行号
    uint32_t offset = (uint32_t)(f->ip - f->chunk->code - 1);
    tb->entries[tb->count].line = msChunkGetLine(f->chunk, offset);
    tb->count++;
    f = f->caller;
  }
}
```

### 3. 打印 traceback

```c
void msPrintTraceback(MsThread* t, FILE* fp) {
  // 从 t->currentException 取附带的 traceback
  MsTraceback* tb = msGetCurrentTraceback(t);
  fprintf(fp, "Traceback (most recent call last):\n");
  if (tb) {
    // 从底部（最外层）到顶部（异常发生处）
    for (int i = (int)tb->count - 1; i >= 0; i--) {
      fprintf(fp, "  File \"%s\", line %u, in %s\n",
          tb->entries[i].fileName,
          tb->entries[i].line,
          tb->entries[i].funcName ? tb->entries[i].funcName : "<module>");
    }
  }
  // 打印异常类型和消息
  MsValue exc = t->currentException;
  if (MS_IS_OBJ(exc) && MS_AS_OBJ(exc)->type == &msInstanceType) {
    MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(exc);
    fprintf(fp, "%s", inst->klass->mstype.name);
    MsValue msg = msMapGet(MS_OBJ_VAL(inst->attrs), msInternStr("message"));
    if (!MS_IS_NIL(msg)) {
      MsStrObj* s = (MsStrObj*)MS_AS_OBJ(msg);
      fprintf(fp, ": %.*s", (int)s->len, s->data);
    }
    fprintf(fp, "\n");
  }
}
```

### 4. 将 traceback 附加到异常对象

```c
// 异常实例有 __traceback__ 属性
// 在 OP_RAISE 时创建 MsTraceback 对象，存入 exc.__traceback__
```

---

## 验收标准（checklist）

- [ ] 未处理异常打印格式：`Traceback (most recent call last):` + 帧列表 + 异常类型: 消息。
- [ ] 帧列表从外到内（most recent last）。
- [ ] 文件名、行号、函数名正确。
- [ ] `e.__traceback__` 属性存在（MsTraceback 对象）。
- [ ] 跨帧展开的 traceback 正确（包含所有帧）。

---

## 测试用例（.ms）

```ms
// 产生 traceback
func inner() { raise ValueError("oops") }
func outer() { inner() }

try {
    outer()
} catch ValueError as e {
    // 手动打印 traceback（T083 完整后）
    print(type(e))           // ValueError
    print(e.message)         // oops
    // e.__traceback__ → MsTraceback
}

// 未处理异常（输出到 stderr）：
// Traceback (most recent call last):
//   File "test.ms", line 8, in <module>
//   File "test.ms", line 3, in outer
//   File "test.ms", line 2, in inner
// ValueError: oops
```

---

## Benchmark

N/A（traceback 记录在异常路径，非热路径）。

---

## 风险与边界

- **traceback 对象 GC**：`MsTraceback` 是 GC 管理对象，存储在 `exc.__traceback__` 属性中，随异常对象一起被回收。
- **行号精度**：依赖 `msChunkGetLine` 的 RLE 行号表（T037）；IP 在指令执行后已递增，需用 `ip - 1` 反查。
