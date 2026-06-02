# mslang 执行模式与字节码缓存

## 1. 概览

```
mslang script.ms
       │
       ▼
  查 __mscache__/script.msc
       │
  ┌────┴────────────────────────────────────────┐
  │ 命中（magic/version/mtime+size 均匹配）     │ 未命中（首次运行 / 源文件改动 / 缓存损坏）
  ▼                                             ▼
读 .msc → 反序列化 MsChunk            源码 (.ms)
  │                                    │  词法分析 (lexer)
  │                                    ▼
  │                                 Token 流
  │                                    │  递归下降 + Pratt 解析 (parser)
  │                                    ▼
  │                                   AST
  │                                    │  单遍编译 (compiler)
  │                                    ▼
  │                                 MsChunk
  │                                    │  写 __mscache__/script.msc
  │                                    │  （失败则静默跳过，-v 下警告）
  └──────────────┬─────────────────────┘
                 ▼
          VM 求值循环执行
```

两条路径在得到 `MsChunk` 后完全等价，缓存对 VM 和嵌入者透明。

---

## 2. CLI 接口

### 2.1 运行方式

```
// 直接运行：第一个参数为 .ms 文件时等价于 run
mslang script.ms [args...]

// 显式子命令
mslang run   <script.ms> [args...]
mslang compile <file_or_dir> [file_or_dir...]
mslang disasm  <file.ms | file.msc>
```

`mslang compile` 递归预生成 `__mscache__/*.msc`，类似 `python -m compileall`，
用于部署前烘焙缓存。`mslang disasm` 的输出格式见 `vm.md §8`。

### 2.2 标志

| 标志 | 说明 |
|---|---|
| `-B` | 禁止写入 `__mscache__`；仍可读取已有缓存 |
| `--no-cache` | 完全跳过读写缓存，每次重新编译 |
| `--hash-cache` | 切换为内容哈希失效模式（见 §4.2） |
| `-v` | 详细输出：打印缓存命中/未命中、写入路径、跳过警告等 |

### 2.3 环境变量

| 变量 | 等价标志 |
|---|---|
| `MSLANG_DONT_WRITE_BYTECODE=1` | `-B` |
| `MSLANG_HASH_CACHE=1` | `--hash-cache` |
| `MSLANG_PATH` | 模块搜索路径（见 `modules.md §9`，此处仅引用） |

命令行标志优先级高于环境变量。

---

## 3. 缓存目录布局

缓存目录 `__mscache__` 与源文件**同级**，按需创建。

```
// 一般模块
foo/bar.ms           →  foo/__mscache__/bar.msc
foo/baz/qux.ms       →  foo/baz/__mscache__/qux.msc

// 包初始化文件
pkg/__init__.ms      →  pkg/__mscache__/__init__.msc

// 入口脚本（与 Python 不同，mslang 也缓存入口脚本）
script.ms            →  __mscache__/script.msc
```

路径映射规则：

```
cache_path = dirname(source_path) / "__mscache__" / stem(source_path) + ".msc"
```

其中 `stem` 取去掉 `.ms` 后缀的文件名（不含目录部分）。

---

## 4. .msc 文件格式

所有多字节整数字段均为**小端序**（little-endian）。

### 4.1 文件头（定长，32 字节）

```
偏移   大小   类型      字段
0      4      uint8[4]  magic            // 固定为 0x4D 0x53 0x43 0x00 ("MSC\0")
4      4      uint32    format_version   // 字节码格式版本号，VM 升级指令集时递增；
                                         // 不匹配则视为缓存无效
8      4      uint32    flags            // bit0: 1 = hash 失效模式，0 = mtime 失效模式
                                         // bit1-31: 保留（写 0，读时忽略）
12     8      uint64    source_mtime_ns  // mtime 模式：源文件 mtime（纳秒，POSIX clock_gettime）
                                         // hash 模式：写 0（不使用）
20     8      uint64    source_size      // mtime 模式：源文件字节大小
                                         // hash 模式：写 0（不使用）
28     8      uint64    source_hash      // hash 模式：源文件内容的 FNV-1a 64 位哈希
                                         // mtime 模式：写 0（不使用）
// 合计 36 字节（注：实际布局以字节对齐为准，此处为逻辑字段描述）
```

> **修正**：头部字段共 4+4+4+8+8+8 = 36 字节，实现时按此对齐。

### 4.2 失效模式

**mtime 模式（默认）**：加载时比较 `source_mtime_ns` 与 `source_size`
与源文件当前 stat 结果；任一不符则缓存无效。

**hash 模式（`--hash-cache` / `MSLANG_HASH_CACHE=1`）**：
加载时读取源文件全部内容，计算 FNV-1a 64 位哈希，与头部 `source_hash` 比较；
不符则缓存无效。hash 模式检测精确但每次运行需读取完整源文件。

### 4.3 payload

文件头之后为 payload：**序列化的模块级 `MsFunction`**（对应模块顶层代码，
其 `MsChunk` 递归含模块内所有嵌套函数与类模板）。序列化格式见 §5。

---

## 5. 序列化（marshal）格式

payload 以递归方式序列化 `MsFunction` 树。

### 5.1 类型 tag

常量池条目与函数字段前置 1 字节 tag：

| Tag 值 | 类型 |
|---|---|
| `0x01` | `int`（int64） |
| `0x02` | `float`（float64） |
| `0x03` | `str` |
| `0x04` | `function`（嵌套函数/闭包模板） |
| `0x05` | `class_template`（类编译期模板） |

### 5.2 MsFunction 编码

```
// MsFunction（对应 type-system.md §2.10 中的 struct MsFunction 结构）
function_record:
    name          : str_record      // 函数名（匿名函数写空串）
    arity         : uint8           // 形参个数（不含可变参数）
    hasVararg     : uint8           // 0 或 1
    isAsync       : uint8           // 0 或 1
    upvalueCount  : uint8           // 捕获的 upvalue 数（运行期绑定，仅记录数量）
    chunk         : chunk_record
```

`upvalueCount` 只记录数量，不序列化具体 upvalue 值——upvalue 是运行期由闭包捕获的
栈上变量，编译期只需知道数量以便 `MAKE_CLOSURE` 指令分配槽位（见 `vm.md §3.9`）。

### 5.3 MsChunk 编码

```
// MsChunk（对应 vm.md §2 中的 struct MsChunk 结构）
chunk_record:
    sourceName     : str_record        // 文件名（调试用）
    codeLen        : uint32
    code           : uint8[codeLen]    // 字节码字节流
    linesLen       : uint32            // 等于 codeLen（每字节一个行号条目）
    lines          : uint32[linesLen]  // 行号表，用于回溯
    constLen       : uint32
    constants      : const_entry[constLen]
```

行号表**始终保留**（mslang 无 `-O` 优化级别，回溯信息不剥离）。

### 5.4 常量池条目

```
const_entry:
    tag : uint8
    // tag = 0x01: int
    //   value : int64（小端序）
    // tag = 0x02: float
    //   value : float64（IEEE 754，小端序）
    // tag = 0x03: str
    //   see str_record
    // tag = 0x04: function
    //   see function_record（递归嵌入）
    // tag = 0x05: class_template
    //   see class_template_record
```

### 5.5 str_record

```
str_record:
    len  : uint32    // UTF-8 字节数
    data : uint8[len]
```

### 5.6 class_template_record

```
// 对应 vm.md §3.9 MAKE_CLASS 指令所使用的编译期类描述符
class_template_record:
    class_name    : str_record
    has_base      : uint8           // 0 = 无基类，1 = 有基类（基类在运行期由栈上值决定）
    method_count  : uint16
    methods       : function_record[method_count]  // 各方法按声明顺序
```

### 5.7 不可序列化的对象

以下对象在**运行期**构造，不出现在常量池中：

| 对象类型 | 说明 |
|---|---|
| `MsInstance` | 运行期由 `MAKE_CLASS`/`__init__` 创建 |
| `MsUpvalue`（closed 值） | 运行期由闭包捕获 |
| 绑定方法 | 运行期由属性查找创建 |
| C 扩展函数（`MsCFunction`）| 由 C 侧注册，不经字节码 |
| `MsMap` / `MsList` 实例 | 运行期由字面量指令构建 |

类（`MsType*`）在运行期由 `MAKE_CLASS` 指令根据 `class_template_record` 构建，
缓存中存储的是其**编译期模板**，而非运行期对象。

---

## 6. 加载与失效流程

以下逻辑由 `msRunFile`（`c-api.md §4.2`）与模块加载器（`modules.md §3`）共用：

```
// 伪码
struct MsChunk* loadChunk(MsVM* vm, const char* sourcePath) {
  const char* cachePath = makeCachePath(sourcePath);
  // __mscache__/stem.msc

  if (!vm->flags.noCache && fileExists(cachePath)) {
    MscHeader hdr = readHeader(cachePath);
    if (hdr.magic == MSC_MAGIC
        && hdr.formatVersion == MSC_FORMAT_VERSION
        && validateStaleness(hdr, sourcePath, vm->flags.hashCache)) {
      return mscUnmarshal(cachePath);  // 命中：直接返回
    }
    // 未命中（含魔数/版本不符、mtime 变化、hash 不匹配、文件损坏）
    // 不报错，继续走编译路径
  }

  // 正常编译
  char* src = readFile(sourcePath);
  struct MsChunk* chunk = msCompile(vm, src, sourcePath);

  // 写缓存（若允许）
  if (!vm->flags.noCache && !vm->flags.dontWriteBytecode) {
    bool ok = mscWriteAtomic(vm, chunk, sourcePath, cachePath);
    // mscWriteAtomic：先写临时文件，再 rename 覆盖
    if (!ok && vm->flags.verbose) {
      fprintf(stderr, "[mslang] warning: failed to write cache %s\n", cachePath);
    }
  }
  return chunk;
}
```

`validateStaleness` 根据 `vm->flags.hashCache` 选择 mtime+size 比较或 FNV-1a 哈希比较。

---

## 7. 与模块系统的衔接

`modules.md §3` 的加载流程第 3 步原为：

```
3. 若未缓存：
   a. 读取文件字节流
   b. 词法 → 解析 → 编译为 MsChunk（模块级代码）
   ...
```

引入字节码缓存后，步骤 3.a–3.b 替换为调用 §6 的 `loadChunk`：

```
3. 若未缓存：
   b. loadChunk(filePath)  // 优先读 __mscache__；未命中则编译并写缓存
   ...
```

注意事项：

- **内置模块**（`modules.md §10`）不走文件系统，不生成也不读取 `__mscache__`。
- **循环导入**语义不受影响：`loadChunk` 只替换"编译"子步骤，模块对象的缓存键
  （规范化绝对路径）与执行时机均不变（见 `modules.md §7`）。
- **相对导入**（`modules.md §2`）在解析得到绝对路径后，同样经过 `loadChunk`，
  无需特殊处理。

---

## 8. 并发与原子性

多进程（或多次同时启动的 VM 实例）可能并发写同一 `.msc` 文件。

**写入协议**（`msc_write_atomic`）：

```
1. 在 __mscache__/ 目录内生成临时文件名：<stem>.<随机后缀>.tmp
   （如 bar.3f7a2c.tmp；随机后缀取自 OS 熵源）
2. 将完整 .msc 内容写入临时文件
3. rename(tmp_path, cache_path)
```

`rename` 在同文件系统内是**原子操作**（POSIX），读者要么看到旧文件要么看到
完整新文件，不会读到半写内容。多进程并发时最后一次 rename 胜出，结果
仍是合法的 `.msc`，无需加锁。

`__mscache__` 目录本身在写入前按需创建（`mkdir -p` 语义）；
创建失败（如只读文件系统）直接跳过写入，不报错（`-v` 下打印警告）。

---

## 9. 限制与未来扩展

| 功能 | 初版状态 | 说明 |
|---|---|---|
| sourceless 执行 | 不支持 | 初版要求 `.ms` 源文件存在，`.msc` 仅为加速优化；未来可支持仅凭 `.msc` 发布运行 |
| 优化级别（`-O`） | 不支持 | 无 docstring/断言剥离；行号表始终保留；plain 文件名无需版本后缀区分 |
| 热重载 | 不支持（规划中） | 见 `modules.md §11`；实现时清除模块缓存条目后可复用 `loadChunk` |
| 跨机器可移植 .msc | 不保证 | `format_version` 仅隐含 VM 指令集，不含 OS/架构；如需分发字节码需额外约定平台无关性 |
