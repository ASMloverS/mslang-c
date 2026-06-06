# P7-T091 `.msc` 字节码序列化（marshal 写）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `.msc`（mslang compiled）字节码文件的序列化：将 `MsChunk`（含嵌套 proto）写入二进制文件，供下次加载时跳过编译，加速模块启动。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T037 | MsChunk 结构 |
| P7-T086 | 模块导入流程（调用 marshal） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §10 字节码缓存（.msc 格式） |

---

## 实现要点

### 1. `.msc` 文件格式

```
Header (16 bytes):
  [0..3]   Magic: 0x4D534300  ('M','S','C','\0')
  [4..5]   Version: 0x0001
  [6..7]   Flags: 0x0000 (reserved)
  [8..11]  SourceHash: FNV-1a of source text (uint32_t)
  [12..15] mtime: source file mtime seconds (uint32_t)

Body:
  ChunkRecord (递归)
```

```
ChunkRecord:
  [4]   codeLen      (uint32_t)
  [n]   code         (uint8_t[codeLen])
  [4]   constCount   (uint32_t)
  for each constant:
    [1]   tag         (0=NIL 1=BOOL 2=INT 3=FLOAT 4=STR)
    [n]   payload     (by tag)
  [4]   lineTabLen   (uint32_t, RLE entries)
  [n]   linTab       (uint8_t pairs: count, delta)
  [2]   localCount   (uint16_t)
  for each local:
    [2] nameIdx      (index into constants, must be STR)
  [2]   upvalueCount (uint16_t)
  for each upvalue:
    [1] isLocal      (uint8_t)
    [1] index        (uint8_t)
  [2]   protoCount   (uint16_t)
  for each proto:
    ChunkRecord      (递归)
  [1]   fileNameLen  (uint8_t, 0-255)
  [n]   fileName     (UTF-8, no null)
  [1]   funcNameLen  (uint8_t)
  [n]   funcName
  [1]   arity        (uint8_t)
  [1]   arityMax     (uint8_t)
  [1]   flags        (bit0=hasVararg, bit1=hasKwarg, bit2=isAsync)
```

### 2. 写入实现

```c
typedef struct MsWriter {
    uint8_t* buf;
    uint32_t len, cap;
} MsWriter;

static void writerWriteByte(MsWriter* w, uint8_t b);
static void writerWriteU16 (MsWriter* w, uint16_t v);
static void writerWriteU32 (MsWriter* w, uint32_t v);
static void writerWriteBytes(MsWriter* w, const void* src, uint32_t n);

static void marshalConst(MsWriter* w, MsValue v) {
    if (MS_IS_NIL(v))    { writerWriteByte(w, 0); return; }
    if (MS_IS_BOOL(v))   { writerWriteByte(w, 1); writerWriteByte(w, (uint8_t)MS_AS_BOOL(v)); return; }
    if (MS_IS_INT(v))    { writerWriteByte(w, 2); writerWriteU64(w, (uint64_t)MS_AS_INT(v)); return; }
    if (MS_IS_FLOAT(v))  { writerWriteByte(w, 3); double d = MS_AS_FLOAT(v); writerWriteBytes(w, &d, 8); return; }
    // STR
    writerWriteByte(w, 4);
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
    writerWriteU32(w, s->len);
    writerWriteBytes(w, s->data, s->len);
}

static void marshalChunk(MsWriter* w, MsChunk* c) {
    writerWriteU32(w, c->codeLen);
    writerWriteBytes(w, c->code, c->codeLen);

    writerWriteU32(w, c->constants.count);
    for (uint32_t i = 0; i < c->constants.count; i++)
        marshalConst(w, c->constants.vals[i]);

    // line table RLE
    marshalLineTbl(w, c);

    writerWriteU16(w, c->localCount);
    for each local: writerWriteU16(w, nameIdx);

    writerWriteU16(w, c->upvalueCount);
    for each upvalue: writerWriteByte(w, isLocal); writerWriteByte(w, idx);

    writerWriteU16(w, c->protoCount);
    for (uint16_t i = 0; i < c->protoCount; i++)
        marshalChunk(w, c->protos[i]);   // 递归

    writerWriteByte(w, (uint8_t)strlen(c->fileName));
    writerWriteBytes(w, c->fileName, strlen(c->fileName));
    // funcName, arity, arityMax, flags ...
}

// 顶层入口：写出 .msc 文件
bool msMarshalWrite(MsChunk* chunk, const char* srcPath,
                    uint32_t srcHash, uint32_t mtime) {
    MsWriter w = {0};
    // 写 header
    writerWriteU32(&w, 0x4D534300);  // magic
    writerWriteU16(&w, 0x0001);      // version
    writerWriteU16(&w, 0x0000);      // flags
    writerWriteU32(&w, srcHash);
    writerWriteU32(&w, mtime);
    marshalChunk(&w, chunk);

    char mscPath[MAX_PATH];
    makeMscPath(srcPath, mscPath, sizeof(mscPath));
    return writeFileAtomic(mscPath, w.buf, w.len);  // T093 原子写
}
```

### 3. .msc 路径策略

```
foo.ms  → <mslang_cache_dir>/foo.msc
         (默认 ~/.cache/mslang/ 或 MSLANG_CACHE_DIR)
```

---

## 验收标准（checklist）

- [ ] `mslang compile foo.ms` → 生成 `foo.msc`，magic 正确。
- [ ] 嵌套函数（proto）正确序列化（递归）。
- [ ] 所有常量类型（nil/bool/int/float/str）正确序列化。
- [ ] 文件存在且为有效 .msc → T092 unmarshal 成功。

---

## 测试用例（C 单测）

```c
// tests/test_marshal.c
void test_marshal_roundtrip(void) {
    MsChunk* c = msCompileStr("x := 1 + 2", "<test>");
    uint8_t buf[4096]; uint32_t len;
    MS_ASSERT(marshalToBuffer(c, buf, sizeof(buf), &len));
    MsChunk* c2 = unmarshalFromBuffer(buf, len, 0, 0);
    MS_ASSERT(c2 != NULL);
    MS_ASSERT(c2->codeLen == c->codeLen);
    MS_ASSERT(memcmp(c2->code, c->code, c->codeLen) == 0);
}
```

---

## Benchmark

```c
// benchmarks/bench_marshal.c
// 目标：序列化 100K 常量 < 50ms
```

---

## 风险与边界

- **字节序**：统一小端存储（Little-Endian）；大端机器（如某些 ARM 服务器）需要字节序转换宏。
- **int64_t**：序列化为 8 字节 LE；浮点 double 序列化为原始 8 字节。
