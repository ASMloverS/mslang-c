# mslang C API

## 1. 设计原则

- **句柄/本地根表**：C 代码不直接持有裸 `MsObject*`，而是持有 `MsHandle`（指向根表槽的稳定引用）。移动式 GC（半区复制）只需更新根表，不会使 C 持有的句柄失效。
- **本地根栈帧**：类 JNI local frame，函数入口 `msPushLocalFrame()`，返回前 `msPopLocalFrame()` 批量释放本次调用创建的所有句柄，避免逐个释放。
- **API 前缀**：所有公开符号以 `ms` 为前缀，驼峰命名（如 `msRunFile`、`msNewHandle`）；类型/typedef 沿用 PascalCase（如 `MsValue`、`MsVM`）。**扩展模块入口点例外**：形如 `ms_module_init_<name>` 的函数由嵌入者按约定调用，属 ABI 契约，保留 snake_case 命名，后缀 `<name>` 与模块名一致即可。
- **错误传播**：C API 区分两种返回约定——
  - 返回 `MsValue` 的 API（如 `msRunFile`、`msCall`、`msMapGet`）：失败返 `MS_ERROR_VALUE`，以 `msIsError(v)` 检查，通过 `msGetError(vm)` 取异常对象。
  - 返回 `int` 的 API（如 `msSetGlobal`、`msListAppend`、`msMapSet`、`msSetAttr`）：0 = 成功，-1 = 失败；异常详情同样通过 `msGetError(vm)` 获取。
  - 两种约定下，失败时均已通过 `thread->exception` 设置当前异常；调用方**每次**须检查返回值，不得忽略。

---

## 2. 公开头文件

```c
#include <mslang/mslang.h>   // 唯一需要包含的头（umbrella header）
```

---

## 3. 句柄与根表

### 3.1 数据类型

```c
// 句柄：C 代码持有 struct MsObject 的间接引用
struct MsHandleSlot {
  struct MsObject* ptr;      // GC 更新此字段（移动后）
  int32_t          refCount; // 句柄引用计数（0=空闲槽）
};

typedef struct MsHandleSlot* MsHandle;  // C 代码持有的就是槽指针

// 解引用句柄（获得底层对象指针，仅在持有期间安全）
#define MS_HANDLE_GET(h)  ((h)->ptr)
```

### 3.2 根表操作

```c
// 为对象创建句柄（加入根表，GC 不会回收）
MsHandle msNewHandle(MsVM* vm, struct MsObject* obj);

// 释放句柄（槽引用计数 -1，归零时槽回收）
void     msFreeHandle(MsVM* vm, MsHandle h);

// 句柄转 MsValue（用于传递给 API 函数）
MsValue  msHandleToValue(MsHandle h);
```

### 3.3 本地根栈帧（LocalFrame）

```c
// 进入函数时创建本地帧（分配 initialCap 槽）
void     msPushLocalFrame(MsVM* vm, int initialCap);

// 离开函数时批量释放所有本地句柄（建议在每个 C 扩展函数末尾调用）
void     msPopLocalFrame(MsVM* vm);

// 在当前本地帧中创建句柄（msPushLocalFrame 后自动跟踪）
MsHandle msLocalHandle(MsVM* vm, struct MsObject* obj);
MsHandle msLocalHandleV(MsVM* vm, MsValue v);  // 若 v 非 OBJ 则返回 NULL
```

---

## 4. 嵌入 API（Embedding）

### 4.1 初始化与销毁

```c
MsVM* msNew(void);                                      // 创建解释器实例
void  msFree(MsVM* vm);                                 // 销毁（触发终结，释放所有内存）
void  msRegisterBuiltinModule(MsVM* vm, MsModule* mod);
```

### 4.2 执行脚本

```c
// 从文件执行（返回 MS_ERROR_VALUE 若出错）
MsValue msRunFile(MsVM* vm, const char* path);

// 从字符串执行（sourceName 用于错误信息，可为 "<string>"）
MsValue msRunString(MsVM* vm, const char* source, const char* sourceName);

// 编译但不执行，返回 struct MsFunction*
MsValue msCompile(MsVM* vm, const char* source, const char* sourceName);

// 执行已编译函数（0 个参数）
MsValue msCall0(MsVM* vm, MsValue fn);
```

### 4.3 全局变量读写

```c
MsValue msGetGlobal(MsVM* vm, const char* name);
int     msSetGlobal(MsVM* vm, const char* name, MsValue val);
```

### 4.4 错误处理

```c
int     msIsError(MsValue v);                                    // 是否为错误哨兵
MsValue msGetError(MsVM* vm);                                    // 取当前异常（struct MsException*）
void    msClearError(MsVM* vm);
void    msRaiseString(MsVM* vm, struct MsType* excType, const char* msg);
void    msRaiseException(MsVM* vm, struct MsObject* exc);

// 内置异常类型指针（初始化后有效）
extern struct MsType* msExcRuntimeError;
extern struct MsType* msExcTypeError;
extern struct MsType* msExcValueError;
// ... 其他见 mslang/exceptions.h
```

### 4.5 安全点控制

```c
// 允许当前 Worker 进入 GC 安全点（长时间运行的 C 函数应周期性调用，以免阻塞 STW）
void msAllowGc(MsVM* vm);

// 抑制安全点（临时持有裸指针期间使用，须成对调用）
void msDisallowGc(MsVM* vm);
```

### 4.6 函数调用

```c
// 调用脚本侧函数，argc 个参数在 argv[]
MsValue msCall(MsVM* vm, MsValue fn, int argc, MsValue* argv);

// 调用方法（obj.method_name(...)）
MsValue msCallMethod(MsVM* vm, MsValue obj,
                     const char* method, int argc, MsValue* argv);
```

---

## 5. 值 API（Value API）

### 5.1 基本值构造

```c
MsValue msInt(int64_t i);
MsValue msFloat(double f);
MsValue msBool(int b);    // b != 0 → true
MsValue msNil(void);
MsValue msTrue(void);
MsValue msFalse(void);
```

### 5.2 字符串

```c
MsValue    msStr(MsVM* vm, const char* utf8, size_t len);
MsValue    msStrC(MsVM* vm, const char* cstr);  // strlen
const char* msStrData(MsValue s);               // 裸字节指针（GC 移动后失效，仅临时用）
size_t      msStrLen(MsValue s);                // 字节长度
```

### 5.3 list

```c
MsValue msNewList(MsVM* vm, int initialCap);
int     msListAppend(MsVM* vm, MsValue lst, MsValue item);
MsValue msListGet(MsValue lst, int64_t idx);   // 不越界检查（C 层调用者负责）
int     msListSet(MsValue lst, int64_t idx, MsValue item);
int64_t msListLen(MsValue lst);
```

### 5.4 map

```c
MsValue msNewMap(MsVM* vm, int initialCap);
int     msMapSet(MsVM* vm, MsValue map, MsValue key, MsValue val);
MsValue msMapGet(MsVM* vm, MsValue map, MsValue key);  // MS_NIL 若无
int     msMapDel(MsVM* vm, MsValue map, MsValue key);
int64_t msMapLen(MsValue map);
```

### 5.5 属性访问

```c
MsValue msGetAttr(MsVM* vm, MsValue obj, const char* name);
int     msSetAttr(MsVM* vm, MsValue obj, const char* name, MsValue val);
```

### 5.6 类型检查

```c
int msIsInt(MsValue v);
int msIsFloat(MsValue v);
int msIsStr(MsValue v);
int msIsList(MsValue v);
int msIsMap(MsValue v);
int msIsNil(MsValue v);
int msIsBool(MsValue v);
int msIsCallable(MsVM* vm, MsValue v);
int msIsInstance(MsVM* vm, MsValue obj, struct MsType* type);

int64_t msAsInt(MsValue v);    // 不检查类型（调用者保证 msIsInt）
double  msAsFloat(MsValue v);
int     msAsBool(MsValue v);
```

---

## 6. 扩展模块 API

### 6.1 原生函数签名

```c
// 所有注册到脚本的 C 函数均采用此签名
typedef MsValue (*MsCFunction)(MsVM* vm, MsValue* argv, int argc);
```

参数布局规则：
- **普通函数**：`argv[0]` 为第一个实参 arg0，`argv[1]` 为 arg1，依此类推；`argc` 为实参总数。
- **绑定方法**（通过实例或类调用）：`argv[0]` 为 self，`argv[1]` 为第一个实参 arg0，余参顺延；`argc` 含 self（即实参数 + 1）。

示例：`obj.method(a, b)` → C 侧收到 `argc=3`，`argv[0]=self`，`argv[1]=a`，`argv[2]=b`。

### 6.2 方法表

```c
struct MsMethodDef {
  const char*  name;
  MsCFunction  fn;
  int          arity;  // 期望参数数（-1 = 可变）
  const char*  doc;    // 文档字符串（可为 NULL）
};

// 方法表以 {NULL,NULL,0,NULL} 结尾
```

### 6.3 模块创建与注册

```c
MsModule* msNewModule(MsVM* vm, const char* name);

// 向模块添加函数
int msAddFunction(MsVM* vm, MsModule* mod,
                  const char* name, MsCFunction fn, int arity);

// 向模块添加常量
int msAddInt(MsVM* vm, MsModule* mod, const char* name, int64_t val);
int msAddFloat(MsVM* vm, MsModule* mod, const char* name, double val);
int msAddStr(MsVM* vm, MsModule* mod, const char* name, const char* val);
int msAddObject(MsVM* vm, MsModule* mod, const char* name, MsValue val);

// 注册为内置模块（须在 msRunFile 前调用）
void msRegisterBuiltinModule(MsVM* vm, MsModule* mod);
```

### 6.4 类型（MsType）注册

```c
// 注册用户定义的 C 类型（可被脚本实例化）
struct MsType* msNewType(MsVM* vm, const char* name, size_t objSize,
                         MsTraverseFn traverse, MsDestroyFn destroy);

// 设置魔术方法槽
void msTypeSetAdd(struct MsType* t, MsBinaryFn fn);
void msTypeSetStr(struct MsType* t, MsUnaryFn fn);
// ... 其他槽类似

// 向模块导出类型
int msAddType(MsVM* vm, MsModule* mod, const char* name, struct MsType* type);
```

---

## 7. 完整示例：C 扩展模块

```c
// mymod.c — 编译为动态库或静态链接
#include <mslang/mslang.h>
#include <math.h>

static MsValue myHypot(MsVM* vm, MsValue* argv, int argc) {
  if (!msIsFloat(argv[0]) || !msIsFloat(argv[1])) {
    msRaiseString(vm, msExcTypeError, "hypot expects two floats");
    return MS_ERROR_VALUE;
  }
  double a = msAsFloat(argv[0]);
  double b = msAsFloat(argv[1]);
  return msFloat(hypot(a, b));
}

static struct MsMethodDef kMymodMethods[] = {
  { "hypot", myHypot, 2, "hypot(a, b) -> float" },
  { NULL, NULL, 0, NULL }
};

// 模块初始化函数（入口点保留 ms_module_init_<name> 命名约定，属 ABI 契约）
MsModule* ms_module_init_mymod(MsVM* vm) {
  MsModule* mod = msNewModule(vm, "mymod");
  for (struct MsMethodDef* m = kMymodMethods; m->name; m++) {
    msAddFunction(vm, mod, m->name, m->fn, m->arity);
  }
  msAddFloat(vm, mod, "MY_PI", 3.14159265358979);
  return mod;
}
```

```c
// main.c — 嵌入示例
#include <mslang/mslang.h>

int main(void) {
  MsVM* vm = msNew();

  // 注册扩展
  MsModule* mymod = ms_module_init_mymod(vm);
  msRegisterBuiltinModule(vm, mymod);

  // 执行脚本
  MsValue result = msRunFile(vm, "script.ms");
  if (msIsError(result)) {
    MsValue exc = msGetError(vm);
    fprintf(stderr, "Error: %s\n", msStrData(msCallMethod(vm, exc, "__str__", 0, NULL)));
    msFree(vm);
    return 1;
  }

  msFree(vm);
  return 0;
}
```

```ms
// script.ms
import mymod

x := mymod.hypot(3.0, 4.0)
print(x)          // 5.0
print(mymod.MY_PI) // 3.14159...
```

---

## 8. GC 交互注意事项

1. **不要将裸 `struct MsObject*` 或 `MsValue` 存入全局 C 变量**，除非通过 `msNewHandle` 将其加入根表。
2. **C 函数执行期间**：扩展 C 函数（`MsCFunction`）由 VM 的某个 Worker 线程调用，其执行区间不经过安全点。由于 Minor GC 为 STW（需所有 Worker 到达安全点），此区间内对象不会被移动，可暂时持有裸指针——但不应跨越任何可能触发 GC 的 API 调用（如 `msCall`/`msNewList` 等分配操作）。长时间运行的 C 函数会阻塞 STW，应周期性调用 `msAllowGc` 让出安全点。
3. **长期持有**：始终用 `msNewHandle` 创建句柄，`msFreeHandle` 释放。
4. **本地帧模式**：在扩展函数开头 `msPushLocalFrame(vm, 16)` 可简化句柄管理；函数返回前 `msPopLocalFrame(vm)` 批量回收。
5. **`msStrData` 返回的指针**：字符串不可变，但新建字符串仍在年轻代，可能被 Minor GC 移动（半区复制）。裸指针应仅在 `msDisallowGc`/`msPushLocalFrame` 保护区间内使用；跨越任何可能触发 GC 的 API 调用（如 `msCall`/`msNewList` 等分配操作）前必须先释放裸指针或改用句柄。已晋升老年代的字符串不参与复制，但初版难以静态判断是否已晋升，建议一律遵守上述限制。

---

## 9. 线程安全

mslang 采用真并行 M:N 调度器，多个 OS Worker 线程并发执行 goroutine。涉及 C API 时有两种上下文，需区别对待：

**嵌入者 API**（`msNew`/`msFree`/`msRunFile`/`msRunString`/`msSetGlobal` 等管理 VM 生命周期的接口）：由持有 VM 所有权的外部线程驱动；若多个外部线程需并发调用，调用方需在外部加锁串行化。通常嵌入者仅从一个线程驱动 VM。

**扩展 C 函数**（`MsCFunction`，由 VM Worker 线程并行调用）：VM 内部共享结构（globals 表、模块缓存、堆分配器）由 VM 内部锁/原子操作/per-Worker TLAB 保护，多个 Worker 可并发调用不同的扩展函数。扩展函数无需自行保护 VM 内部结构，但：
- **共享的 C 侧全局状态**（如静态变量）需扩展模块自行加锁。
- **裸指针**持有规则见 §8：不应跨越分配 API 调用，长时间运行需周期性 `msAllowGc`。
