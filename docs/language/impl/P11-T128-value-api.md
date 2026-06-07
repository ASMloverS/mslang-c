# P11-T128 值 API（构造 / 字符串 / list / map / 属性 / 类型检查）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现完整的 C API 值操作层：从 C 类型构造 `MsValue`、从 `MsValue` 提取 C 类型、操作容器（list/map）、访问对象属性、类型检查。这是 C 扩展与脚本层数据交换的核心接口。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P11-T127 | 嵌入 API（VM 入口） |
| P4-T057 ~ P4-T064 | 核心类型 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `c-api.md` | §4 值 API |

---

## 实现要点

### 1. 标量构造与提取

```c
// 构造（已有宏，这里是函数封装供 FFI 使用）
MsValue msInt(int64_t v)   { return MS_INT_VAL(v); }
MsValue msFloat(double v)  { return MS_FLOAT_VAL(v); }
MsValue msBool(bool v)     { return MS_BOOL_VAL(v); }
MsValue msNil(void)        { return MS_NIL_VAL; }

// 提取（带类型检查，失败返回默认值）
bool     msToInt   (MsValue v, int64_t* out);
bool     msToFloat (MsValue v, double*  out);
bool     msToBool  (MsValue v, bool*    out);
bool     msToCStr  (MsValue v, const char** out, size_t* lenOut);

// 实现
bool msToInt(MsValue v, int64_t* out) {
  if (MS_IS_INT(v))   { *out = MS_AS_INT(v); return true; }
  if (MS_IS_BOOL(v))  { *out = MS_AS_BOOL(v) ? 1 : 0; return true; }
  if (MS_IS_FLOAT(v)) { *out = (int64_t)MS_AS_FLOAT(v); return true; }
  return false;
}
```

### 2. 字符串 API

```c
// 从 C 字符串创建 ms 字符串
MsValue msNewStr(const char* data, size_t len);

// 从格式化字符串创建
MsValue msStrFormat(const char* fmt, ...);

// 获取字符串内容（只读，GC 期间可能移动，需在安全点前使用）
bool msStrGet(MsValue v, const char** data, size_t* len);

// 字符串追加（创建新字符串）
MsValue msStrConcat(MsValue a, MsValue b);
```

### 3. List API

```c
// 创建 list
MsValue msNewList(void);

// 追加元素
bool msListAppend(MsValue list, MsValue item);

// 获取元素
MsValue msListGet(MsValue list, int64_t idx);

// 设置元素
bool msListSet(MsValue list, int64_t idx, MsValue val);

// 获取长度
int64_t msListLen(MsValue list);

// 从 C 数组创建 list
MsValue msNewListFromArray(const MsValue* items, uint32_t count);
```

### 4. Map API

```c
// 创建 map
MsValue msNewMap(void);

// 设置键值（字符串键）
bool msMapSetStr(MsValue map, const char* key, MsValue val);

// 获取值
MsValue msMapGetStr(MsValue map, const char* key);

// 检查键是否存在
bool msMapHasStr(MsValue map, const char* key);

// 删除键
bool msMapDelStr(MsValue map, const char* key);

// 获取长度
int64_t msMapLen(MsValue map);
```

### 5. 属性访问

```c
// 获取对象属性
MsValue msGetAttr(MsValue obj, const char* name);

// 设置对象属性
bool msSetAttr(MsValue obj, const char* name, MsValue val);

// 调用对象方法
MsValue msCallMethod(MsVM* vm, MsValue obj, const char* method,
                     const MsValue* args, int argc);
```

### 6. 类型检查

```c
// 类型检查函数
bool msIsNil   (MsValue v)  { return MS_IS_NIL(v); }
bool msIsBool  (MsValue v)  { return MS_IS_BOOL(v); }
bool msIsInt   (MsValue v)  { return MS_IS_INT(v); }
bool msIsFloat (MsValue v)  { return MS_IS_FLOAT(v); }
bool msIsStr   (MsValue v);
bool msIsList  (MsValue v);
bool msIsMap   (MsValue v);
bool msIsTuple (MsValue v);
bool msIsCallable(MsValue v);
bool msIsInstance(MsValue v, MsValue klass);  // isinstance 检查

// 获取类型名
const char* msTypeName(MsValue v);
```

---

## 验收标准（checklist）

- [ ] `msInt(42)` → `MS_INT_VAL(42)`；`msToInt` 正确提取。
- [ ] `msNewStr("hello", 5)` → 创建 GC 管理的字符串。
- [ ] `msListAppend` + `msListGet` 正确访问 list 元素。
- [ ] `msMapSetStr` + `msMapGetStr` 正确操作 map。
- [ ] `msGetAttr` 返回实例属性或类方法（含 MRO）。
- [ ] 类型检查函数全部正确。

---

## 测试用例（C 单测）

```c
// tests/test_value_api.c
void testListApi(void) {
  MsValue lst = msNewList();
  for (int i = 0; i < 10; i++)
    msListAppend(lst, msInt(i * i));

  MS_ASSERT(msListLen(lst) == 10);
  MS_ASSERT(MS_AS_INT(msListGet(lst, 0)) == 0);
  MS_ASSERT(MS_AS_INT(msListGet(lst, 9)) == 81);
}

void testMapApi(void) {
  MsValue m = msNewMap();
  msMapSetStr(m, "name", msNewStr("mslang", 6));
  msMapSetStr(m, "version", msInt(1));

  MsValue name = msMapGetStr(m, "name");
  MS_ASSERT(msIsStr(name));

  const char* s; size_t len;
  msStrGet(name, &s, &len);
  MS_ASSERT(strncmp(s, "mslang", len) == 0);
}
```

---

## Benchmark

N/A（值 API 调用开销在 C FFI 层，不需要 .ms benchmark）。

---

## 风险与边界

- **GC 安全**：`msStrGet` 返回的 `const char*` 指向 GC 管理内存；在下次 GC 触发前使用（Minor GC 后字符串可能被复制到新地址）；短期内使用无问题，长期存储需要复制或使用句柄。
