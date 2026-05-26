# mslang 模块系统

## 1. import 语法

```ms
import math                       // 导入标准库模块
import os.path                    // 导入子模块（对应 os/path.ms 或 os/path/init.ms）
import .utils                     // 相对导入：当前包目录下的 utils.ms
import ..common.util              // 相对导入：上级包的 common/util.ms
import math as m                  // 别名
import strings as str
```

访问方式：

```ms
import math
x := math.sqrt(16.0)

import math as m
x := m.sqrt(16.0)

import os.path
exists := os.path.exists("/tmp/foo")
```

**点号（`.`）在模块名中的含义**：将点映射为路径分隔符。`import os.path` 等价于查找 `os/path.ms` 或 `os/path/init.ms`。

**无引号**：import 语句直接使用标识符与点号，不使用字符串字面量。

---

## 2. 解析优先级

给定 `import foo.bar`，按以下顺序查找：

1. **模块缓存**：若已导入（同一 `MsVM` 实例中），直接返回缓存对象。
2. **内置模块**：检查是否为内置标准库（`fmt`、`math`、`os` 等），按点号分层匹配。
3. **文件系统查找**（`foo.bar` → 路径 `foo/bar`，按顺序）：
   a. `<script 所在目录>/foo/bar.ms`
   b. `<script 所在目录>/foo/bar/init.ms`
   c. `<MSLANG_PATH 中每个目录>/foo/bar.ms`
   d. `<MSLANG_PATH 中每个目录>/foo/bar/init.ms`
4. 若全部查找失败，抛出 `ImportError`。

### 相对导入（前缀点）

前缀点的数量表示相对层级（类 Python）：

| 语法 | 含义 |
|---|---|
| `import .utils` | 当前模块所在目录的 `utils.ms` |
| `import ..common` | 上一级目录的 `common.ms` 或 `common/init.ms` |
| `import ..common.util` | 上一级目录的 `common/util.ms` |

相对导入**不经过** `MSLANG_PATH`，始终相对于**当前模块文件**所在目录解析。

---

## 3. 模块加载流程

```
1. 解析 DottedName：
   - 无前缀点：绝对模块名（foo.bar → 路径 foo/bar）
   - 有前缀点：相对路径（. → 当前目录，.. → 上级目录）
2. 查模块缓存 vm->modules（MsMap: str→MsModule*），缓存键为规范化绝对路径
3. 若未缓存：
   a. 读取文件字节流
   b. 词法 → 解析 → 编译为 MsChunk（模块级代码）
   c. 新建 MsModule 对象（含独立 globals MsMap）
   d. 加入缓存（在执行前，防止循环导入死循环）
   e. 在模块的 globals 中执行模块级代码（ms_exec_chunk）
4. 将 MsModule* 绑定到导入名（或别名）
```

---

## 4. 模块对象（MsModule）

```c
typedef struct {
    MsObject head;
    MsStr   *name;       // 模块名（如 "math" 或 "os.path"）
    MsStr   *file_path;  // 来源文件绝对路径（内置模块为 NULL）
    MsMap   *globals;    // 模块全局变量/函数/类
} MsModule;
```

模块级代码执行后，其全局变量均存于 `globals`。`import foo.bar` 时，`foo` 名字绑定到顶层模块对象（`MsModule*`），而 `foo.bar` 作为 `foo.globals["bar"]` 访问。若 `bar` 是子模块，则 `foo.bar` 是另一个 `MsModule*`，自动挂在父模块的 globals 中。

---

## 5. 子模块访问

`import os.path` 后，可以：

```ms
import os.path
os.path.exists("/tmp")      // 通过链式属性访问
exists := os.path.exists    // 也可取出函数

import os.path as osp       // 别名直接指向子模块
osp.exists("/tmp")
```

导入子模块时，父模块（`os`）也会被加载（执行 `os/init.ms`，若存在）。

---

## 6. 导出（Export）

mslang 无显式 `export` 关键字：模块顶层定义的所有名字均为可导入的公共接口。

约定（非强制）：以 `_` 开头的名字为**私有**（仍可访问，但文档约定不使用）。

---

## 7. 循环导入

如 `a.ms` 导入 `b.ms`，`b.ms` 又导入 `a.ms`：

- 加载 `a.ms` 时，先将**空的** `MsModule` 加入缓存，再执行模块代码。
- `b.ms` 导入 `a.ms` 时命中缓存，得到尚未完全初始化的模块对象。
- 若 `b.ms` 在此时访问 `a.xxx`，`xxx` 可能尚未定义（`AttributeError`）。

**建议**：避免循环依赖；如无法避免，将共享接口提取到第三个模块。

---

## 8. 包（目录模块）

目录模块通过 `init.ms` 文件标识：

```
mypackage/
  init.ms        // 包初始化代码，定义包级接口
  utils.ms
  algo/
    init.ms
    sort.ms
```

```ms
import mypackage           // 执行 mypackage/init.ms
import mypackage.algo      // 执行 mypackage/algo/init.ms
import mypackage.algo.sort // 执行 mypackage/algo/sort.ms
```

`init.ms` 可通过导入子模块并重导出名字来组织包接口：

```ms
// mypackage/init.ms
import .utils
import .algo

sqrt  := utils.sqrt
qsort := algo.qsort
```

---

## 9. MSLANG_PATH 环境变量

```
MSLANG_PATH=/usr/local/lib/ms:/home/user/mslibs
```

多路径用 `:` 分隔（Windows 用 `;`）。自动在路径列表**末尾**追加内置标准库目录。

---

## 10. 内置模块注册（C 扩展）

C 扩展通过 C API 在解释器初始化时注册内置模块（详见 c-api.md）：

```c
MsModule *mymod = Ms_NewModule(vm, "mymod");
Ms_AddFunction(vm, mymod, "hello", my_hello_fn, 0);
MsVM_RegisterBuiltinModule(vm, mymod);
```

注册后，`import mymod` 在第 2 步（内置查找）命中，不经文件系统。

---

## 11. 模块热重载（规划）

初版不支持热重载。未来版本可通过清除模块缓存条目、重新加载文件实现；调试/开发模式下可监听文件变更自动重载。
