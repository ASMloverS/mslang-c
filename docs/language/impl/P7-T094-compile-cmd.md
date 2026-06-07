# P7-T094 `compile` / `compileall` 子命令

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `mslang compile <file>` 和 `mslang compileall <dir>` CLI 子命令：预编译 `.ms` 文件为 `.msc` 字节码，加速批量部署场景（无需在运行时编译）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T091 ~ T093 | marshal 写 + 缓存失效 |
| P0-T004 | CLI 子命令框架 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `execution.md` | §2 命令行编译（mslang -c） |

---

## 实现要点

### 1. `compile` 子命令

```c
// mslang compile [--force] <file.ms>
int cmdCompile(int argc, char** argv) {
  bool force = false;
  const char* srcPath = NULL;

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--force") == 0) force = true;
    else srcPath = argv[i];
  }
  if (!srcPath) { fprintf(stderr, "Usage: mslang compile <file.ms>\n"); return 1; }

  // 如果不强制，且缓存有效，跳过
  if (!force) {
    MsSrcInfo info;
    if (msGetSrcInfo(srcPath, &info)) {
      MsChunk* cached = msMarshalRead(srcPath, info.contentHash, info.mtime);
      if (cached) {
        printf("Up to date: %s\n", srcPath);
        msChunkFree(cached);
        return 0;
      }
    }
  }

  char* src = msReadFile(srcPath);
  if (!src) { perror(srcPath); return 1; }

  MsChunk* chunk = msCompileFile(srcPath, src, strlen(src));
  msFree(src);
  if (!chunk) return 1;  // 编译错误已打印

  MsSrcInfo info;
  msGetSrcInfo(srcPath, &info);
  bool ok = msMarshalWrite(chunk, srcPath, info.contentHash, info.mtime);
  msChunkFree(chunk);

  if (ok) printf("Compiled: %s\n", srcPath);
  else   { fprintf(stderr, "Failed to write .msc for %s\n", srcPath); return 1; }
  return 0;
}
```

### 2. `compileall` 子命令

```c
// mslang compileall [--force] [--workers=N] <dir>
int cmdCompileAll(int argc, char** argv) {
  bool  force   = false;
  int   workers = 1;  // 默认单线程（P9 并发后可多线程）
  const char* dir = ".";

  // 解析参数...

  // 递归扫描目录下所有 .ms 文件
  MsFileList list = {0};
  msGlobMs(dir, &list);  // 递归收集 *.ms

  int ok = 0, fail = 0, skip = 0;
  for (uint32_t i = 0; i < list.count; i++) {
    int r = compileOne(list.paths[i], force);
    if (r == 0)  ok++;
    else if (r == 2) skip++;
    else fail++;
  }
  printf("Compiled %d, skipped %d, failed %d\n", ok, skip, fail);
  return (fail > 0) ? 1 : 0;
}
```

### 3. 输出格式

```
$ mslang compile foo.ms
Compiled: foo.ms  →  ~/.cache/mslang/...foo.ms.msc

$ mslang compileall ./lib
Compiling lib/math.ms       [OK]
Compiling lib/strings.ms    [OK]
Compiling lib/net/client.ms [SKIP] (up to date)
Compiled 2, skipped 1, failed 0
```

---

## 验收标准（checklist）

- [ ] `mslang compile foo.ms` → 生成 `.msc` 文件，返回 0。
- [ ] 再次运行（未改变）→ "Up to date: foo.ms"，不重写。
- [ ] `--force` 总是重新编译。
- [ ] `mslang compileall ./lib` → 递归编译全部 `.ms` 文件。
- [ ] 某文件有语法错误 → 打印错误，继续编译其他文件，最终返回 1。

---

## 测试用例（.ms）

```ms
// 无 .ms 测试（CLI 工具），通过 shell golden 测试：
// echo 'x := 42' > /tmp/test_compile.ms
// mslang compile /tmp/test_compile.ms
// mslang run /tmp/test_compile.ms  # 应使用缓存
```

---

## Benchmark

N/A（compile 是预处理步骤，不在热路径）。

---

## 风险与边界

- **并发编译**：`compileall --workers=N` 在 P9 完成前为单线程；N>1 时打印 warning 并回退到 1。
