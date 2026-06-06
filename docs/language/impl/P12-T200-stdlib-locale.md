# P12-T200 stdlib: locale

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `locale` 模块：本地化支持，包括数字/货币格式、字符串比较（本地敏感）、字符集。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T133 | sys |
| P12-T141 | strings |

---

## API 清单

```ms
// locale 设置
locale.setlocale(category, locale=nil)
// category: locale.LC_ALL / LC_COLLATE / LC_CTYPE / LC_MONETARY /
//           LC_NUMERIC / LC_TIME / LC_MESSAGES
// locale=nil → 查询当前 locale
// locale="" → 使用系统默认 locale
// locale="en_US.UTF-8" → 设置指定 locale

locale.getlocale(category=LC_CTYPE) → (language, encoding)
locale.getdefaultlocale() → (language, encoding)  // 系统默认
locale.getpreferredencoding(do_setlocale=true) → str   // "UTF-8"

// 数字格式化
locale.format_string(format, val, grouping=false) → str
locale.format(format, val, grouping=false) → str  // 已弃用
locale.currency(val, symbol=true, grouping=false, international=false) → str
locale.str(val) → str   // 格式化浮点（使用本地小数点）
locale.atof(string) → float   // 解析本地格式浮点
locale.atoi(string) → int     // 解析本地格式整数

// 本地信息
locale.localeconv() → dict
// {'decimal_point': '.', 'thousands_sep': ',', 'grouping': [3,0],
//  'currency_symbol': '$', 'int_curr_symbol': 'USD ',
//  'p_sign_posn': 1, 'n_sign_posn': 1, ...}

// 字符串比较（本地敏感）
locale.strcoll(s1, s2) → int   // 负/零/正
locale.strxfrm(s) → str        // 转换为可用 < > 比较的字符串

// 常量
locale.LC_ALL      locale.LC_COLLATE   locale.LC_CTYPE
locale.LC_MONETARY locale.LC_NUMERIC   locale.LC_TIME
locale.LC_MESSAGES  (POSIX only)

locale.CHAR_MAX   // = 127（用于 grouping 的无分组标志）
```

---

## 实现要点

```c
// 底层：调用 POSIX setlocale() / localeconv() / strcoll() / strxfrm()
// Windows：_configthreadlocale + setlocale（线程安全）

// locale.setlocale 包装：
// setlocale(LC_ALL, "en_US.UTF-8") → POSIX
// Windows：setlocale(LC_ALL, "English_United States.1252")
// 错误（不支持的 locale）→ 抛 locale.Error

// localeconv() 返回 MsMapObj 包含所有 lconv 字段

// format_string：
// 类似 % 格式化，但数字用本地格式（小数点、千位分隔符）
// grouping=true → 按 lconv.grouping 分组（千位分隔）

// currency(val, symbol=true, international=false)：
// international=true → 使用 int_curr_symbol（"USD "）
// international=false → currency_symbol（"$"）
// 按 p_sign_posn/n_sign_posn 放置正负号

// strcoll：直接调用 strcoll(s1, s2)（C 函数）
// strxfrm：strxfrm(dst, src, n)

// 简化实现（若 locale 支持有限）：
// 仅实现 "C"（POSIX）locale 和系统默认 locale
// 不实现所有 LC_* 分类，仅 LC_ALL 和 LC_NUMERIC/LC_MONETARY

// getpreferredencoding：
// POSIX：nl_langinfo(CODESET)
// Windows：GetACP() → 映射到 "cp1252" 或 "UTF-8"
```

---

## 验收标准（checklist）

- [ ] `locale.setlocale(locale.LC_ALL, "")` 设置系统 locale 不崩溃。
- [ ] `locale.localeconv()` 返回含 `decimal_point` 的字典。
- [ ] `locale.format_string("%.2f", 1234567.89, grouping=True)` 按 locale 格式化。
- [ ] `locale.currency(1234.5)` 返回含货币符号的字符串。
- [ ] `locale.strcoll("a", "b") < 0` → `true`（a < b）。
- [ ] `locale.getpreferredencoding()` 返回非空字符串（"UTF-8" 或 "cp1252"）。

---

## 测试用例（.ms）

```ms
import locale

// 查询系统 locale
lang, enc := locale.getlocale()
print(lang, enc)    // 如 ("en_US", "UTF-8") 或 (nil, nil)

pref_enc := locale.getpreferredencoding()
print(pref_enc)     // "UTF-8"（Linux/macOS）或 "cp1252"（Windows）

// localeconv（C locale）
locale.setlocale(locale.LC_ALL, "C")
conv := locale.localeconv()
print(conv["decimal_point"])   // "."
print(conv["thousands_sep"])   // ""（C locale 无千位分隔）

// 尝试系统 locale（若支持）
try:
    locale.setlocale(locale.LC_ALL, "en_US.UTF-8")
    conv2 := locale.localeconv()
    print(conv2["thousands_sep"])  // ","
    print(locale.format_string("%d", 1000000, grouping=true))  // 1,000,000
    print(locale.currency(9.99))   // "$9.99"
catch e as locale.Error:
    print("locale not available:", e)

// strcoll（C locale）
locale.setlocale(locale.LC_ALL, "C")
print(locale.strcoll("apple", "banana") < 0)  // true

// 恢复
locale.setlocale(locale.LC_ALL, "")
```
