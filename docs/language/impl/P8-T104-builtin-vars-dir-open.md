# P8-T104 内置函数：vars / dir / open

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现内省和文件 I/O 内置函数：`vars`（返回对象的属性字典）、`dir`（返回属性名列表）、`open`（打开文件，返回文件对象）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T060 | MsMapObj |
| P5-T072 | MsInstanceObj |
| P7-T090 | 内置模块注册（open 归属于 io 模块，但作为内置提供） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib.md` | §1 内置函数 |

---

## 实现要点

### 1. `vars([object])`

```c
// vars() → 当前帧局部变量字典（调试用）
// vars(obj) → obj.__dict__（实例属性字典）
static MsValue builtinVars(MsThread* t, MsValue* args, int argc) {
  if (argc == 0) {
    // 收集当前帧的局部变量
    MsFrame* f = t->frame;
    MsValue  d = msNewMap();
    for (uint16_t i = 0; i < f->chunk->localCount; i++) {
      const char* name = f->chunk->localNames[i];
      MsValue     val  = f->slots[i];
      msMapSet(d, msNewStrIntern(name, strlen(name)), val);
    }
    return d;
  }
  MsValue obj = args[0];
  // 实例：返回实例 attrs
  if (MS_IS_OBJ(obj) && MS_AS_OBJ(obj)->type == &msInstanceType) {
    MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(obj);
    return MS_OBJ_VAL(inst->attrs);
  }
  // 模块：返回 globals
  if (MS_IS_OBJ(obj) && MS_AS_OBJ(obj)->type == &msModuleType) {
    MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(obj);
    return MS_OBJ_VAL(m->globals);
  }
  return msRaiseTypeError(t, "vars() argument must be object with __dict__");
}
```

### 2. `dir([object])`

```c
// dir() → 当前作用域内所有名称列表
// dir(obj) → 对象所有属性名（含继承的方法名）
static MsValue builtinDir(MsThread* t, MsValue* args, int argc) {
  MsValue names = msNewSet();  // 用 set 去重，最后转 list 排序

  if (argc == 0) {
    // 当前帧局部变量 + 全局
    MsFrame* f = t->frame;
    for (uint16_t i = 0; i < f->chunk->localCount; i++)
      msSetAdd(names, msNewStrIntern(f->chunk->localNames[i], strlen(f->chunk->localNames[i])));
    // globals（遍历 map 的 keys）
    msMapForEachKey(t->frame->globals, names);
  } else {
    MsValue obj = args[0];
    if (MS_IS_OBJ(obj) && MS_AS_OBJ(obj)->type == &msInstanceType) {
      MsInstanceObj* inst = (MsInstanceObj*)MS_AS_OBJ(obj);
      // 实例属性
      msMapForEachKey(MS_OBJ_VAL(inst->attrs), names);
      // 类 MRO 方法
      MsTypeObj* klass = inst->klass;
      for (uint32_t i = 0; i < klass->mroLen; i++) {
        msMapForEachKey(MS_OBJ_VAL(klass->mro[i]->methods), names);
      }
    } else if (MS_IS_OBJ(obj) && MS_AS_OBJ(obj)->type == &msTypeType) {
      // 类对象
      MsTypeObj* klass = (MsTypeObj*)MS_AS_OBJ(obj);
      msMapForEachKey(MS_OBJ_VAL(klass->methods), names);
    }
    // else：基础类型，返回类型方法名
  }

  // 转为 list 并排序
  MsValue lst = msSetToSortedList(names);
  return lst;
}
```

### 3. `open(path, mode="r")`

```c
// 返回 MsFileObj（实现了读写接口）
// mode: "r"读/"w"写/"a"追加/"rb"/"wb" 等
typedef struct MsFileObj {
  MsObject header;
  FILE*    fp;
  char     mode[8];
  bool     closed;
} MsFileObj;

MsType msFileType = {
  .name = "file",
  .tpGetattr = fileGetAttr,  // 提供 read/write/close/readline/readlines 方法
};

static MsValue builtinOpen(MsThread* t, MsValue* args, int argc) {
  if (argc < 1) return msRaiseTypeError(t, "open() requires path argument");
  if (!MS_IS_OBJ(args[0]) || MS_AS_OBJ(args[0])->type != &msStrType)
    return msRaiseTypeError(t, "open() path must be str");

  MsStrObj* path = (MsStrObj*)MS_AS_OBJ(args[0]);
  const char* mode = "r";
  if (argc >= 2 && MS_IS_OBJ(args[1]) && MS_AS_OBJ(args[1])->type == &msStrType)
    mode = ((MsStrObj*)MS_AS_OBJ(args[1]))->data;

  FILE* fp = fopen(path->data, mode);
  if (!fp) return msRaiseOSError(t, errno, path->data);

  MsFileObj* f = msGCAlloc(sizeof(*f), &msFileType);
  f->fp = fp;
  strlcpy(f->mode, mode, sizeof(f->mode));
  f->closed = false;
  return MS_OBJ_VAL((MsObject*)f);
}

// MsFileObj 方法：read/readline/write/close
// 支持 with 语句（__enter__返回 self，__exit__调用 close()）
```

---

## 验收标准（checklist）

- [ ] `vars()` → 返回当前作用域的变量字典。
- [ ] `vars(instance)` → 返回实例属性字典（同一对象）。
- [ ] `dir([])` → 包含 `"append"/"pop"/"sort"` 等方法名。
- [ ] `dir(instance)` → 含实例属性 + 类方法名，已排序。
- [ ] `open("test.txt", "w")` → 可写入；`open("test.txt")` → 可读取。
- [ ] `with open("f.txt") as f: ...` → 关闭后 `f.closed == true`。

---

## 测试用例（.ms）

```ms
// vars
x := 10
y := "hello"
v := vars()
print("x" in v)   // true
print(v["x"])     // 10

// dir
class Dog {
    func bark(self) { return "woof" }
}
d := Dog()
d.name := "Rex"
attrs := dir(d)
print("bark" in attrs)  // true
print("name" in attrs)  // true

// open
with open("/tmp/test_open.txt", "w") as f {
    f.write("hello ms!\n")
}
with open("/tmp/test_open.txt") as f {
    content := f.read()
    print(content)   // hello ms!
}
```

---

## Benchmark

N/A（I/O 操作，性能由 OS 决定）。

---

## 风险与边界

- **`open` 的二进制模式**：`"rb"`/`"wb"` 模式返回 bytes 而非 str；`read()` 在文本模式下解码 UTF-8，错误时抛 `UnicodeDecodeError`；初版只做基础 UTF-8 解码，忽略其他编码。
- **文件对象 GC**：若 `MsFileObj` 被 GC 回收时未关闭，`tpFree` 中自动 `fclose`（资源泄漏保护）。
