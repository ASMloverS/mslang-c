# P7-T093 缓存失效（mtime/hash）+ 原子写

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现字节码缓存的有效性判断：通过源文件 `mtime`（修改时间）和内容 FNV-1a 哈希双重校验缓存是否过期；过期则重新编译并原子写入新 `.msc`（避免写到一半被读取）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T091 | marshal 写 |
| P7-T092 | marshal 读 |

---

## 实现要点

### 1. 双重有效性校验

```c
typedef struct MsSrcInfo {
  uint32_t mtime;       // 源文件 mtime（秒级）
  uint32_t contentHash; // 源文件内容的 FNV-1a 哈希
  size_t   size;
} MsSrcInfo;

bool msGetSrcInfo(const char* path, MsSrcInfo* info) {
  struct stat st;
  if (stat(path, &st) != 0) return false;
  info->mtime = (uint32_t)st.st_mtime;
  info->size  = (size_t)st.st_size;

  // 读取内容计算哈希
  char* src = msReadFile(path);
  if (!src) return false;
  info->contentHash = msFNV1a32(src, strlen(src));
  msFree(src);
  return true;
}
```

### 2. 加载流程（缓存优先）

```c
// 在 OP_IMPORT（T087）中替换直接编译的步骤：
MsChunk* msLoadChunk(const char* srcPath) {
  MsSrcInfo info;
  if (!msGetSrcInfo(srcPath, &info)) return NULL;  // 文件不存在

  // 尝试读缓存
  MsChunk* chunk = msMarshalRead(srcPath, info.contentHash, info.mtime);
  if (chunk) return chunk;  // 缓存命中

  // 缓存失效/不存在：重新编译
  char* src = msReadFile(srcPath);
  chunk = msCompileFile(srcPath, src, strlen(src));
  msFree(src);
  if (!chunk) return NULL;

  // 写新缓存
  msMarshalWrite(chunk, srcPath, info.contentHash, info.mtime);
  return chunk;
}
```

### 3. 原子写（writeFileAtomic）

```c
// 防止写到一半被另一个进程读取
bool writeFileAtomic(const char* finalPath, const uint8_t* data, uint32_t len) {
  // 写到临时文件（同目录，保证同一文件系统以支持 rename）
  char tmpPath[MAX_PATH];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp.%u", finalPath, (unsigned)getpid());

  FILE* f = fopen(tmpPath, "wb");
  if (!f) return false;
  bool ok = (fwrite(data, 1, len, f) == len);
  fflush(f);
  fclose(f);
  if (!ok) { remove(tmpPath); return false; }

  // 原子 rename
#ifdef _WIN32
  // Windows rename 不原子，但 MoveFileExW REPLACE_EXISTING 接近原子
  if (!MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING)) {
    remove(tmpPath); return false;
  }
#else
  if (rename(tmpPath, finalPath) != 0) { remove(tmpPath); return false; }
#endif
  return true;
}
```

### 4. 缓存目录

```c
// 缓存目录策略（优先级：MSLANG_CACHE_DIR > ~/.cache/mslang）
void makeMscPath(const char* srcPath, char* mscPath, uint32_t mscLen) {
  const char* cacheDir = getenv("MSLANG_CACHE_DIR");
  if (!cacheDir) {
    // ~/.cache/mslang/
    const char* home = getenv("HOME");
    if (!home) home = ".";
    snprintf(mscPath, mscLen, "%s/.cache/mslang/__cache__", home);
  } else {
    strlcpy(mscPath, cacheDir, mscLen);
  }
  // 将 srcPath 中 '/' 替换为 '_' 作为文件名（简单策略）
  // 生产环境可用 SHA256(srcPath) 作为路径哈希
  char encoded[256];
  encodePathFlat(srcPath, encoded, sizeof(encoded));
  snprintf(mscPath, mscLen, "%s/%s.msc", mscPath, encoded);
}
```

---

## 验收标准（checklist）

- [ ] `foo.ms` 未修改 → 第二次 `import` 使用缓存，不重新编译。
- [ ] `foo.ms` 内容改变 → 缓存失效，重新编译并更新缓存。
- [ ] 缓存目录不存在时 → 创建目录后写入（不 crash）。
- [ ] 临时文件 + rename 原子操作（不会出现半写文件）。
- [ ] `--no-cache` 标志 → 跳过缓存读写（CLI 层透传）。

---

## 测试用例

```bash
# 测试缓存命中
mslang run foo.ms  # 第一次：编译 + 写缓存
mslang run foo.ms  # 第二次：直接读缓存

# 验证缓存时间戳
touch -m foo.ms    # 修改 mtime
mslang run foo.ms  # 触发重新编译
```

---

## Benchmark

```c
// 有缓存 vs 无缓存启动时间
// 目标：有缓存时冷启动 < 20ms（1000行模块，从缓存加载）
```

---

## 风险与边界

- **mtime 精度**：某些 FAT32 文件系统 mtime 精度为 2 秒；双重校验（mtime + hash）弥补精度不足。
- **--hash-cache 标志**：`--hash-cache` 只用内容哈希校验，忽略 mtime，速度略慢但更准确；默认用 mtime 快速判断，不一致再比哈希。
