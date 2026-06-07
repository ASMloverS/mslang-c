# P7-T092 `.msc` 反序列化（unmarshal 读 + 头部校验）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `.msc` 文件的读取与反序列化：读取头部（魔数/版本/源哈希/mtime），验证合法性后，将字节码数据还原为 `MsChunk` 对象供 VM 直接执行，跳过重新编译。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T091 | .msc 格式定义（marshal 写） |
| P3-T037 | MsChunk 结构 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `execution.md` | §4 .msc 格式 / §5 marshal 读写 |

---

## 实现要点

### 1. Reader 结构

```c
typedef struct MsReader {
  const uint8_t* buf;
  uint32_t       pos;
  uint32_t       len;
  bool           error;
} MsReader;

static uint8_t  readerReadByte(MsReader* r);
static uint16_t readerReadU16 (MsReader* r);
static uint32_t readerReadU32 (MsReader* r);
static uint64_t readerReadU64 (MsReader* r);
static void     readerReadBytes(MsReader* r, void* dst, uint32_t n);
```

### 2. 头部校验

```c
typedef struct MscHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint32_t sourceHash;
  uint32_t mtime;
} MscHeader;

// 返回 true 表示 header 合法且与当前源文件匹配
bool msMarshalReadHeader(MsReader* r, MscHeader* hdr) {
  hdr->magic      = readerReadU32(r);
  hdr->version    = readerReadU16(r);
  hdr->flags      = readerReadU16(r);
  hdr->sourceHash = readerReadU32(r);
  hdr->mtime      = readerReadU32(r);

  if (r->error) return false;
  if (hdr->magic   != 0x4D534300) return false;  // 魔数错
  if (hdr->version != 0x0001)     return false;  // 版本不匹配
  return true;
}
```

### 3. Chunk 反序列化

```c
static MsValue unmarshalConst(MsReader* r) {
  uint8_t tag = readerReadByte(r);
  switch (tag) {
    case 0: return MS_NIL_VAL;
    case 1: return MS_BOOL_VAL(readerReadByte(r) != 0);
    case 2: return MS_INT_VAL((int64_t)readerReadU64(r));
    case 3: {
      double d; readerReadBytes(r, &d, 8);
      return MS_FLOAT_VAL(d);
    }
    case 4: {
      uint32_t len = readerReadU32(r);
      char* data = msAlloc(len + 1);
      readerReadBytes(r, data, len);
      data[len] = '\0';
      MsValue v = msNewStrIntern(data, len);
      msFree(data);
      return v;
    }
    default: r->error = true; return MS_NIL_VAL;
  }
}

static MsChunk* unmarshalChunk(MsReader* r) {
  MsChunk* c = msChunkNew();

  uint32_t codeLen = readerReadU32(r);
  c->code    = msAlloc(codeLen);
  c->codeLen = codeLen;
  readerReadBytes(r, c->code, codeLen);

  uint32_t constCount = readerReadU32(r);
  for (uint32_t i = 0; i < constCount; i++) {
    MsValue v = unmarshalConst(r);
    msConstPoolAdd(&c->constants, v);
  }

  unmarshalLineTbl(r, c);

  uint16_t localCount = readerReadU16(r);
  for (uint16_t i = 0; i < localCount; i++) { }  // 读 nameIdx

  uint16_t upvalCount = readerReadU16(r);
  for (uint16_t i = 0; i < upvalCount; i++) { }  // 读 isLocal + idx

  uint16_t protoCount = readerReadU16(r);
  c->protos    = msAlloc(protoCount * sizeof(MsChunk*));
  c->protoCount = protoCount;
  for (uint16_t i = 0; i < protoCount; i++)
    c->protos[i] = unmarshalChunk(r);  // 递归

  uint8_t fnLen = readerReadByte(r);
  // 读 fileName, funcName, arity, arityMax, flags ...

  if (r->error) { msChunkFree(c); return NULL; }
  return c;
}
```

### 4. 顶层入口（在 import 时调用）

```c
// 尝试从 .msc 加载；成功返回 MsChunk*，失败（过期/损坏）返回 NULL
MsChunk* msMarshalRead(const char* srcPath,
            uint32_t srcHash, uint32_t mtime) {
  char mscPath[MAX_PATH];
  makeMscPath(srcPath, mscPath, sizeof(mscPath));

  size_t fileLen;
  uint8_t* data = msReadFileBinary(mscPath, &fileLen);
  if (!data) return NULL;

  MsReader r = { data, 0, fileLen, false };
  MscHeader hdr;
  if (!msMarshalReadHeader(&r, &hdr) ||
    hdr.sourceHash != srcHash ||
    hdr.mtime      != mtime) {
    msFree(data);
    return NULL;  // 缓存失效（T093 处理）
  }

  MsChunk* chunk = unmarshalChunk(&r);
  msFree(data);
  return chunk;
}
```

---

## 验收标准（checklist）

- [ ] marshal 写 → unmarshal 读 → 执行结果与直接编译一致（roundtrip）。
- [ ] 魔数错误 → 返回 NULL（不 crash）。
- [ ] 版本不匹配 → 返回 NULL。
- [ ] 文件截断（不完整）→ `r.error = true`，返回 NULL。
- [ ] sourceHash/mtime 不匹配 → 返回 NULL（由 T093 触发重新编译）。

---

## 测试用例（C 单测）

```c
// tests/test_unmarshal.c
void testRoundtripNested(void) {
  const char* src = "func f(x) { return x * 2 }\nprint(f(21))";
  MsChunk* orig = msCompileStr(src, "<test>");

  // 写到内存
  MsWriter w = {0};
  marshalChunkToWriter(&w, orig);

  // 读回
  MsReader r = { w.buf, 0, w.len, false };
  MsChunk* loaded = unmarshalChunk(&r);
  MS_ASSERT(!r.error);
  MS_ASSERT(loaded->protoCount == 1);  // func f
  MS_ASSERT(loaded->protos[0]->arity == 1);

  // 执行验证（需要 VM）
  // ...
}
```

---

## Benchmark

N/A（unmarshal 只在模块加载时执行一次）。

---

## 风险与边界

- **内存安全**：`unmarshalChunk` 面对恶意/损坏的 .msc 文件不得越界读；`MsReader.error` 标志确保读取超界时不继续解析。
