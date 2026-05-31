# mslang C API

## 1. 设计原则

- **句柄/本地根表**：C 代码不直接持有裸 `MsObject*`，而是持有 `MsHandle`（指向根表槽的稳定引用）。移动式 GC（半区复制）只需更新根表，不会使 C 持有的句柄失效。
- **本地根栈帧**：类 JNI local frame，函数入口 `ms_push_local_frame()`，返回前 `ms_pop_local_frame()` 批量释放本次调用创建的所有句柄，避免逐个释放。
- **API 前缀**：所有公开符号以 `ms_` 或 `ms_vm_` 为前缀；类型/typedef 沿用 CamelCase（如 `MsValue`、`MsVM`）。
- **错误传播**：C API 区分两种返回约定——
  - 返回 `MsValue` 的 API（如 `ms_vm_run_file`、`ms_call`、`ms_map_get`）：失败返 `MS_ERROR_VALUE`，以 `ms_is_error(v)` 检查，通过 `ms_get_error(vm)` 取异常对象。
  - 返回 `int` 的 API（如 `ms_vm_set_global`、`ms_list_append`、`ms_map_set`、`ms_set_attr`）：0 = 成功，-1 = 失败；异常详情同样通过 `ms_get_error(vm)` 获取。
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
// 句柄：C 代码持有 MsObject 的间接引用
typedef struct MsHandleSlot {
    MsObject *ptr;           // GC 更新此字段（移动后）
    int32_t   ref_count;     // 句柄引用计数（0=空闲槽）
} MsHandleSlot;

typedef MsHandleSlot* MsHandle;   // C 代码持有的就是槽指针

// 解引用句柄（获得底层对象指针，仅在持有期间安全）
#define MS_HANDLE_GET(h)  ((h)->ptr)
```

### 3.2 根表操作

```c
// 为对象创建句柄（加入根表，GC 不会回收）
MsHandle ms_new_handle(MsVM *vm, MsObject *obj);

// 释放句柄（槽引用计数 -1，归零时槽回收）
void ms_free_handle(MsVM *vm, MsHandle h);

// 句柄转 MsValue（用于传递给 API 函数）
MsValue  ms_handle_to_value(MsHandle h);
```

### 3.3 本地根栈帧（LocalFrame）

```c
// 进入函数时创建本地帧（分配 initial_cap 槽）
void ms_push_local_frame(MsVM *vm, int initial_cap);

// 离开函数时批量释放所有本地句柄（建议在每个 C 扩展函数末尾调用）
void ms_pop_local_frame(MsVM *vm);

// 在当前本地帧中创建句柄（PushLocalFrame 后自动跟踪）
MsHandle ms_local_handle(MsVM *vm, MsObject *obj);
MsHandle ms_local_handle_v(MsVM *vm, MsValue v);  // 若 v 非 OBJ 则返回 NULL
```

---

## 4. 嵌入 API（Embedding）

### 4.1 初始化与销毁

```c
MsVM *ms_vm_new(void);                     // 创建解释器实例
void  ms_vm_free(MsVM *vm);                // 销毁（触发终结，释放所有内存）
void  ms_vm_register_builtin_module(MsVM *vm, MsModule *mod);
```

### 4.2 执行脚本

```c
// 从文件执行（返回 MS_ERROR_VALUE 若出错）
MsValue ms_vm_run_file(MsVM *vm, const char *path);

// 从字符串执行（source_name 用于错误信息，可为 "<string>"）
MsValue ms_vm_run_string(MsVM *vm, const char *source, const char *source_name);

// 编译但不执行，返回 MsFunction*
MsValue ms_vm_compile(MsVM *vm, const char *source, const char *source_name);

// 执行已编译函数（0 个参数）
MsValue ms_vm_call0(MsVM *vm, MsValue fn);
```

### 4.3 全局变量读写

```c
MsValue ms_vm_get_global(MsVM *vm, const char *name);
int     ms_vm_set_global(MsVM *vm, const char *name, MsValue val);
```

### 4.4 错误处理

```c
int      ms_is_error(MsValue v);                     // 是否为错误哨兵
MsValue  ms_get_error(MsVM *vm);                     // 取当前异常（MsException*）
void     ms_clear_error(MsVM *vm);
void     ms_raise_string(MsVM *vm, MsType *exc_type, const char *msg);
void     ms_raise_exception(MsVM *vm, MsObject *exc);

// 内置异常类型指针（初始化后有效）
extern MsType *ms_exc_runtime_error;
extern MsType *ms_exc_type_error;
extern MsType *ms_exc_value_error;
// ... 其他见 mslang/exceptions.h
```

### 4.5 安全点控制

```c
// 允许当前 Worker 进入 GC 安全点（长时间运行的 C 函数应周期性调用，以免阻塞 STW）
void ms_allow_gc(MsVM *vm);

// 抑制安全点（临时持有裸指针期间使用，须成对调用）
void ms_disallow_gc(MsVM *vm);
```

### 4.6 函数调用

```c
// 调用脚本侧函数，argc 个参数在 argv[]
MsValue ms_call(MsVM *vm, MsValue fn, int argc, MsValue *argv);

// 调用方法（obj.method_name(...)）
MsValue ms_call_method(MsVM *vm, MsValue obj,
                       const char *method, int argc, MsValue *argv);
```

---

## 5. 值 API（Value API）

### 5.1 基本值构造

```c
MsValue ms_int(int64_t i);
MsValue ms_float(double f);
MsValue ms_bool(int b);          // b != 0 → true
MsValue ms_nil(void);
MsValue ms_true(void);
MsValue ms_false(void);
```

### 5.2 字符串

```c
MsValue ms_str(MsVM *vm, const char *utf8, size_t len);
MsValue ms_str_c(MsVM *vm, const char *cstr);       // strlen
const char *ms_str_data(MsValue s);                  // 裸字节指针（GC 移动后失效，仅临时用）
size_t      ms_str_len(MsValue s);                   // 字节长度
```

### 5.3 list

```c
MsValue ms_new_list(MsVM *vm, int initial_cap);
int     ms_list_append(MsVM *vm, MsValue lst, MsValue item);
MsValue ms_list_get(MsValue lst, int64_t idx);       // 不越界检查（C 层调用者负责）
int     ms_list_set(MsValue lst, int64_t idx, MsValue item);
int64_t ms_list_len(MsValue lst);
```

### 5.4 map

```c
MsValue ms_new_map(MsVM *vm, int initial_cap);
int     ms_map_set(MsVM *vm, MsValue map, MsValue key, MsValue val);
MsValue ms_map_get(MsVM *vm, MsValue map, MsValue key); // MS_NIL 若无
int     ms_map_del(MsVM *vm, MsValue map, MsValue key);
int64_t ms_map_len(MsValue map);
```

### 5.5 属性访问

```c
MsValue ms_get_attr(MsVM *vm, MsValue obj, const char *name);
int     ms_set_attr(MsVM *vm, MsValue obj, const char *name, MsValue val);
```

### 5.6 类型检查

```c
int ms_is_int(MsValue v);
int ms_is_float(MsValue v);
int ms_is_str(MsValue v);
int ms_is_list(MsValue v);
int ms_is_map(MsValue v);
int ms_is_nil(MsValue v);
int ms_is_bool(MsValue v);
int ms_is_callable(MsVM *vm, MsValue v);
int ms_is_instance(MsVM *vm, MsValue obj, MsType *type);

int64_t ms_as_int(MsValue v);   // 不检查类型（调用者保证 ms_is_int）
double  ms_as_float(MsValue v);
int     ms_as_bool(MsValue v);
```

---

## 6. 扩展模块 API

### 6.1 原生函数签名

```c
// 所有注册到脚本的 C 函数均采用此签名
typedef MsValue (*MsCFunction)(MsVM *vm, MsValue *argv, int argc);
```

参数布局规则：
- **普通函数**：`argv[0]` 为第一个实参 arg0，`argv[1]` 为 arg1，依此类推；`argc` 为实参总数。
- **绑定方法**（通过实例或类调用）：`argv[0]` 为 self，`argv[1]` 为第一个实参 arg0，余参顺延；`argc` 含 self（即实参数 + 1）。

示例：`obj.method(a, b)` → C 侧收到 `argc=3`，`argv[0]=self`，`argv[1]=a`，`argv[2]=b`。

### 6.2 方法表

```c
typedef struct {
    const char  *name;
    MsCFunction  fn;
    int          arity;  // 期望参数数（-1 = 可变）
    const char  *doc;    // 文档字符串（可为 NULL）
} MsMethodDef;

// 方法表以 {NULL,NULL,0,NULL} 结尾
```

### 6.3 模块创建与注册

```c
MsModule *ms_new_module(MsVM *vm, const char *name);

// 向模块添加函数
int ms_add_function(MsVM *vm, MsModule *mod,
                   const char *name, MsCFunction fn, int arity);

// 向模块添加常量
int ms_add_int(MsVM *vm, MsModule *mod, const char *name, int64_t val);
int ms_add_float(MsVM *vm, MsModule *mod, const char *name, double val);
int ms_add_str(MsVM *vm, MsModule *mod, const char *name, const char *val);
int ms_add_object(MsVM *vm, MsModule *mod, const char *name, MsValue val);

// 注册为内置模块（须在 ms_vm_run_file 前调用）
void ms_vm_register_builtin_module(MsVM *vm, MsModule *mod);
```

### 6.4 类型（MsType）注册

```c
// 注册用户定义的 C 类型（可被脚本实例化）
MsType *ms_new_type(MsVM *vm, const char *name, size_t obj_size,
                   MsTraverseFn traverse, MsDestroyFn destroy);

// 设置魔术方法槽
void ms_type_set_add(MsType *t, MsBinaryFn fn);
void ms_type_set_str(MsType *t, MsUnaryFn fn);
// ... 其他槽类似

// 向模块导出类型
int ms_add_type(MsVM *vm, MsModule *mod, const char *name, MsType *type);
```

---

## 7. 完整示例：C 扩展模块

```c
// mymod.c — 编译为动态库或静态链接
#include <mslang/mslang.h>
#include <math.h>

static MsValue my_hypot(MsVM *vm, MsValue *argv, int argc) {
    if (!ms_is_float(argv[0]) || !ms_is_float(argv[1])) {
        ms_raise_string(vm, ms_exc_type_error, "hypot expects two floats");
        return MS_ERROR_VALUE;
    }
    double a = ms_as_float(argv[0]);
    double b = ms_as_float(argv[1]);
    return ms_float(hypot(a, b));
}

static MsMethodDef mymod_methods[] = {
    { "hypot", my_hypot, 2, "hypot(a, b) -> float" },
    { NULL, NULL, 0, NULL }
};

// 模块初始化函数（暴露给 ms_vm_register_builtin_module 调用者）
MsModule *ms_module_init_mymod(MsVM *vm) {
    MsModule *mod = ms_new_module(vm, "mymod");
    for (MsMethodDef *m = mymod_methods; m->name; m++) {
        ms_add_function(vm, mod, m->name, m->fn, m->arity);
    }
    ms_add_float(vm, mod, "MY_PI", 3.14159265358979);
    return mod;
}
```

```c
// main.c — 嵌入示例
#include <mslang/mslang.h>

int main(void) {
    MsVM *vm = ms_vm_new();

    // 注册扩展
    MsModule *mymod = ms_module_init_mymod(vm);
    ms_vm_register_builtin_module(vm, mymod);

    // 执行脚本
    MsValue result = ms_vm_run_file(vm, "script.ms");
    if (ms_is_error(result)) {
        MsValue exc = ms_get_error(vm);
        fprintf(stderr, "Error: %s\n", ms_str_data(ms_call_method(vm, exc, "__str__", 0, NULL)));
        ms_vm_free(vm);
        return 1;
    }

    ms_vm_free(vm);
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

1. **不要将裸 `MsObject*` 或 `MsValue` 存入全局 C 变量**，除非通过 `ms_new_handle` 将其加入根表。
2. **C 函数执行期间**：扩展 C 函数（`MsCFunction`）由 VM 的某个 Worker 线程调用，其执行区间不经过安全点。由于 Minor GC 为 STW（需所有 Worker 到达安全点），此区间内对象不会被移动，可暂时持有裸指针——但不应跨越任何可能触发 GC 的 API 调用（如 `ms_call`/`ms_new_list` 等分配操作）。长时间运行的 C 函数会阻塞 STW，应周期性调用 `ms_allow_gc` 让出安全点。
3. **长期持有**：始终用 `ms_new_handle` 创建句柄，`ms_free_handle` 释放。
4. **本地帧模式**：在扩展函数开头 `ms_push_local_frame(vm, 16)` 可简化句柄管理；函数返回前 `ms_pop_local_frame(vm)` 批量回收。
5. **`ms_str_data` 返回的指针**：字符串不可变且不参与复制 GC（老年代），在调用期间安全；但不应长期缓存此指针。

---

## 9. 线程安全

mslang 采用真并行 M:N 调度器，多个 OS Worker 线程并发执行 goroutine。涉及 C API 时有两种上下文，需区别对待：

**嵌入者 API**（`ms_vm_new/Free/RunFile/RunString/SetGlobal` 等管理 VM 生命周期的接口）：由持有 VM 所有权的外部线程驱动；若多个外部线程需并发调用，调用方需在外部加锁串行化。通常嵌入者仅从一个线程驱动 VM。

**扩展 C 函数**（`MsCFunction`，由 VM Worker 线程并行调用）：VM 内部共享结构（globals 表、模块缓存、堆分配器）由 VM 内部锁/原子操作/per-Worker TLAB 保护，多个 Worker 可并发调用不同的扩展函数。扩展函数无需自行保护 VM 内部结构，但：
- **共享的 C 侧全局状态**（如静态变量）需扩展模块自行加锁。
- **裸指针**持有规则见 §8：不应跨越分配 API 调用，长时间运行需周期性 `ms_allow_gc`。
