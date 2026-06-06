# P12-T199 stdlib: platform

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `platform` 模块：获取当前运行平台的操作系统、处理器架构、Python（mslang）版本等信息。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T133 | sys.version/platform |

---

## API 清单

```ms
// 平台信息
platform.system() → str      // "Linux" "Windows" "Darwin"
platform.release() → str     // 内核版本（"5.15.0-74-generic"）
platform.version() → str     // 完整版本字符串
platform.machine() → str     // "x86_64" "arm64" "aarch64"
platform.processor() → str   // CPU 型号（可为空）
platform.node() → str        // 主机名

// Python 版本（在 mslang 中为 mslang 版本）
platform.python_version() → str         // "3.11.0"（或 mslang 版本）
platform.python_version_tuple() → tuple // ("3","11","0")
platform.python_implementation() → str  // "CPython" or "mslang"
platform.python_revision() → str        // git revision（若可用）
platform.python_build() → (buildno, builddate)

// 综合信息
platform.platform(aliased=false, terse=false) → str
// Linux-5.15.0-74-generic-x86_64-with-glibc2.35

platform.uname() → uname_result
// uname_result(system, node, release, version, machine, processor)

// Windows 特定
platform.win32_ver(release="", version="", csd="", ptype="")
platform.win32_edition() → str   // "Enterprise" "Home" ...
platform.win32_is_iot() → bool

// macOS 特定
platform.mac_ver(release="", versioninfo=("","",""), machine="")
// → (release, (version, dev_stage, non_release), machine)

// Linux 特定
platform.libc_ver(executable=nil, lib="", version="", chunksize=16384)
// → ("glibc", "2.35")（或空）

platform.freedesktop_os_release() → dict
// 读取 /etc/os-release → {"ID": "ubuntu", "VERSION_ID": "22.04", ...}
```

---

## 实现要点

```c
// system()：#ifdef __linux__ → "Linux"
//           #ifdef _WIN32   → "Windows"
//           #ifdef __APPLE__ → "Darwin"

// machine()：uname() 系统调用或 PROCESSOR_ARCHITECTURE env（Windows）
// 常见值映射：x86_64 AMD64 → "x86_64"; arm64 aarch64 → "arm64"

// release()/version()：
// POSIX：struct utsname u; uname(&u); u.release / u.version
// Windows：GetVersionExW / RtlGetVersion（Vista+）

// node()：gethostname()

// uname()：
// POSIX：uname() syscall → struct utsname
// Windows：GetComputerNameEx + GetVersionEx + GetSystemInfo

// freedesktop_os_release()：
// 解析 /etc/os-release 或 /usr/lib/os-release（KEY=VALUE 格式）
// 支持引号值（KEY="some value"）和转义

// python_version()：从 gVM.msVersion 字符串提取
// python_implementation()：固定返回 "mslang"

// platform()：组装平台描述字符串
// Linux：system + release + machine + libc
// Windows：system + release + version + edition

// 结果缓存：首次调用后缓存（platform info 不变）
```

---

## 验收标准（checklist）

- [ ] `platform.system()` 返回 "Linux"/"Windows"/"Darwin" 之一。
- [ ] `platform.machine()` 返回 "x86_64" 或 "arm64"。
- [ ] `platform.uname()` 所有字段非空字符串（node 为主机名）。
- [ ] `platform.python_implementation()` → `"mslang"`。
- [ ] `platform.freedesktop_os_release()` 在 Linux 上解析 /etc/os-release。
- [ ] `platform.platform()` 返回人类可读的平台描述字符串。

---

## 测试用例（.ms）

```ms
import platform

// 基础信息
print(platform.system())    // "Linux" 或 "Windows" 或 "Darwin"
print(platform.machine())   // "x86_64" 或 "arm64" ...
print(platform.node())      // 主机名（非空）

// uname
info := platform.uname()
print(info.system, info.release, info.machine)
print(info.node)   // 同 gethostname()

// mslang 版本
print(platform.python_implementation())  // "mslang"
print(platform.python_version())         // 如 "1.0.0"

// 完整平台字符串
print(platform.platform())
// 示例：Linux-5.15.0-x86_64-with-glibc2.35

// Linux 特定
import sys
if sys.platform == "linux":
    rel := platform.freedesktop_os_release()
    print(rel.get("ID", "unknown"))         // 如 "ubuntu"
    print(rel.get("VERSION_ID", ""))        // 如 "22.04"

// Windows 特定
if sys.platform == "win32":
    print(platform.win32_ver())
    print(platform.win32_edition())
```
