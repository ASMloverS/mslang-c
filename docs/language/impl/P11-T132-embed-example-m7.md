# P11-T132 嵌入 / 扩展完整示例 + 测试（M7 里程碑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

通过完整的 C 程序 + `.ms` 测试验证 P11 C API（T126–T131），包括：嵌入执行、C 函数注册、自定义类型、异常传递、句柄保护、模块扩展。此任务是 P11 阶段的**里程碑收口**（M7）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P11-T126 ~ T131 | P11 所有任务 |

---

## M7 示例程序（`examples/`）

### `examples/embed_basic.c`

```c
// 最小嵌入示例：在 C 程序中执行 mslang 脚本
#include "mslang.h"
#include <stdio.h>

int main(int argc, char** argv) {
  MsVM* vm = msNewVM();

  // 注入 C 函数
  static MsCFunctionDef funcs[] = {
    { "c_add", [](MsThread* t, MsValue* a, int c) -> MsValue {
      if (c != 2) return msRaiseTypeError(t, "c_add takes 2 args");
      int64_t x, y;
      if (!msToInt(a[0], &x) || !msToInt(a[1], &y))
        return msRaiseTypeError(t, "c_add requires int args");
      return msInt(x + y);
    }, 2 },
    { NULL }
  };
  MsValue mod = msNewExtModule(vm, "__main__", funcs, NULL);
  for (const MsCFunctionDef* f = funcs; f->name; f++)
    msSetGlobal(vm, f->name, msMapGetStr(mod, f->name));

  // 执行 .ms 脚本
  const char* script =
    "result := c_add(21, 21)\n"
    "print('c_add result:', result)\n"
    "assert result == 42\n";

  MsValue r = msRunString(vm, script, "<test>");
  if (msHasException(&vm->mainThread)) {
    fprintf(stderr, "Error: %s\n", msGetError(vm));
    msFreeVM(vm);
    return 1;
  }

  printf("Embed test passed!\n");
  msFreeVM(vm);
  return 0;
}
```

### `examples/custom_type_ext.c`

```c
// 完整 C 扩展：实现 Vector2D 类型
#include "mslang.h"

typedef struct Vec2Obj {
  MsInstanceObj base;
  double x, y;
} Vec2Obj;

// __init__, __repr__, __add__, dot, length...
// （完整实现，约 100 行）

static MsCFunctionDef vec2Methods[] = {
  { "length", vec2_length, 1 },
  { "dot",    vec2_dot,    2 },
  { "normalize", vec2_norm, 1 },
  { NULL }
};

MsValue ms_init_vec2(MsVM* vm) {
  MsValue Vec2 = msRegisterType(vm,
    &(MsTypeSpec){
      .name = "Vec2",
      .instanceSize = sizeof(Vec2Obj),
      .tpInit = vec2_init,
      .tpRepr = vec2_repr,
      .tpAdd  = vec2_add,
      .methods = vec2Methods,
    }, NULL, 0);

  MsValue mod = msNewExtModule(vm, "vec2", NULL, NULL);
  msModuleAddConst(mod, "Vec2", Vec2);
  msRegisterExtModule(vm, mod);
  return mod;
}
```

### `tests/ms/p11/capi_test.ms`（.ms 侧测试）

```ms
// 测试 C 扩展
import vec2
v1 := vec2.Vec2(3.0, 4.0)
v2 := vec2.Vec2(1.0, 0.0)

print(repr(v1))         // Vec2(3.0, 4.0)
print(v1.length())      // 5.0
print(v1.dot(v2))       // 3.0

v3 := v1 + v2
print(repr(v3))         // Vec2(4.0, 4.0)

// isinstance
print(isinstance(v1, vec2.Vec2))  // true
print(isinstance(v1, int))        // false
```

**期望输出**：
```
Vec2(3.0, 4.0)
5.0
3.0
Vec2(4.0, 4.0)
true
false
```

---

## 验收标准（checklist）

- [ ] `examples/embed_basic.c` 编译并运行成功，输出 "Embed test passed!"。
- [ ] `examples/custom_type_ext.c` 注册 Vec2 类型，`.ms` 测试全部通过。
- [ ] C 函数抛出的异常能被 .ms catch。
- [ ] 句柄保护：GC 触发时，C 持有的对象不被回收。
- [ ] `msFreeVM` 后无内存泄漏（valgrind 检测）。
- [ ] M7 文档：`docs/c-api-guide.md`（实际用法指南，含 5 个完整示例）。

---

## Benchmark（M7 综合）

```c
// benchmarks/bench_capi.c
// 1. C→ms 调用速度
void benchCCallMs(MsVM* vm) {
  // 1M 次 msRunString（短脚本）
  // 目标：重复执行已编译脚本 < 10ns/次
}

// 2. ms→C 调用速度（C 扩展函数调用）
void benchMsCallC(MsVM* vm) {
  // 注册 noop C 函数，1M 次调用
  // 目标：C 函数调用开销 < 50ns/次
}
```

---

## 风险与边界

- **M7 定义**：M7 = C API 完整可用（嵌入 + 扩展 + 自定义类型）+ 示例通过 + 无内存泄漏。至此 mslang 可以被嵌入到任何 C/C++ 应用中，或通过 C 扩展访问系统 API。
