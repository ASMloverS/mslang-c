# datetime — 日期、时间、时区与时间差对象

```ms
import datetime
```

## 概述

提供不可变的日期/时间对象，支持算术、格式化与时区感知操作。API 与 Python
`datetime` 模块一致，使用 `%Y`/`%m`/`%d` 等格式代码。所有类均为 PascalCase。

类层次：

```
datetime.tzinfo        (抽象基类，供自定义时区继承)
  └─ datetime.timezone (固定 UTC 偏移时区)
datetime.date          (仅日期：年/月/日)
datetime.time          (仅时间：时/分/秒/微秒/时区)
datetime.datetime      (日期+时间，继承自 date)
datetime.timedelta     (时间差/持续时间)
```

## 常量与类型

每个类都暴露以下类级常量（`timedelta` 类型）：

| 常量 | 说明 |
|---|---|
| `date.min` | 可表示的最小日期 `date(1, 1, 1)` |
| `date.max` | 可表示的最大日期 `date(9999, 12, 31)` |
| `date.resolution` | 日期精度 `timedelta(days=1)` |
| `time.min` | `time(0, 0, 0, 0)` |
| `time.max` | `time(23, 59, 59, 999999)` |
| `time.resolution` | `timedelta(microseconds=1)` |
| `datetime.min` | `datetime(1, 1, 1, 0, 0)` |
| `datetime.max` | `datetime(9999, 12, 31, 23, 59, 59, 999999)` |
| `datetime.resolution` | `timedelta(microseconds=1)` |
| `timedelta.min` | `timedelta(days=-999999999)` |
| `timedelta.max` | `timedelta(days=999999999, hours=23, minutes=59, seconds=59, microseconds=999999)` |
| `timedelta.resolution` | `timedelta(microseconds=1)` |
| `timezone.utc` | UTC 时区单例 |

## 函数签名速查

### datetime.date

| 方式 | 签名 | 说明 |
|---|---|---|
| 构造 | `date(year, month, day)` | 参数无效时抛 `ValueError` |
| 类方法 | `date.today() → date` | 本地当天日期 |
| 类方法 | `date.fromtimestamp(ts) → date` | Unix 时间戳 → 本地日期 |
| 类方法 | `date.fromordinal(n) → date` | 格里高利序数 → 日期（1 = 公元1年1月1日） |
| 类方法 | `date.fromisoformat(s) → date` | 解析 `"YYYY-MM-DD"` |
| 属性 | `.year .month .day` | 只读整数 |
| 方法 | `.timetuple() → struct_time` | 转为 time.struct_time |
| 方法 | `.toordinal() → int` | 转为格里高利序数 |
| 方法 | `.weekday() → int` | 星期（0=周一…6=周日） |
| 方法 | `.isoweekday() → int` | 星期（1=周一…7=周日） |
| 方法 | `.isocalendar() → (year, week, weekday)` | ISO 8601 年/周/星期 |
| 方法 | `.isoformat() → str` | `"YYYY-MM-DD"` |
| 方法 | `.strftime(fmt) → str` | 按格式代码格式化 |
| 方法 | `.replace(year=nil, month=nil, day=nil) → date` | 替换指定字段，返回新对象 |

### datetime.time

| 方式 | 签名 | 说明 |
|---|---|---|
| 构造 | `time(hour=0, minute=0, second=0, microsecond=0, tzinfo=nil)` | |
| 属性 | `.hour .minute .second .microsecond .tzinfo` | 只读 |
| 方法 | `.isoformat() → str` | `"HH:MM:SS[.ffffff][+HH:MM]"` |
| 方法 | `.strftime(fmt) → str` | 按格式代码格式化 |
| 方法 | `.replace(...) → time` | 替换指定字段，返回新对象 |
| 方法 | `.utcoffset() → timedelta\|nil` | 时区偏移；naive 时间返回 nil |
| 方法 | `.dst() → timedelta\|nil` | 夏令时偏移 |
| 方法 | `.tzname() → str\|nil` | 时区名称 |

### datetime.datetime

| 方式 | 签名 | 说明 |
|---|---|---|
| 构造 | `datetime(year, month, day, hour=0, minute=0, second=0, microsecond=0, tzinfo=nil)` | |
| 类方法 | `datetime.now(tz=nil) → datetime` | 当前本地（或指定时区）日期时间 |
| 类方法 | `datetime.utcnow() → datetime` | 当前 UTC 日期时间（naive） |
| 类方法 | `datetime.today() → datetime` | 等同 `datetime.now()` |
| 类方法 | `datetime.fromtimestamp(ts, tz=nil) → datetime` | Unix 时间戳 → datetime |
| 类方法 | `datetime.fromisoformat(s) → datetime` | 解析 ISO 8601 字符串 |
| 类方法 | `datetime.strptime(s, fmt) → datetime` | 按格式代码解析字符串 |
| 类方法 | `datetime.combine(date, time, tzinfo=nil) → datetime` | 合并 date 与 time |
| 类方法 | `datetime.fromordinal(n) → datetime` | 格里高利序数 → datetime（时间为 0:0:0） |
| 属性 | `.year .month .day .hour .minute .second .microsecond .tzinfo` | 只读 |
| 方法 | `.date() → date` | 提取日期部分 |
| 方法 | `.time() → time` | 提取时间部分（丢弃 tzinfo） |
| 方法 | `.timetz() → time` | 提取时间部分（保留 tzinfo） |
| 方法 | `.replace(...) → datetime` | 替换指定字段，返回新对象 |
| 方法 | `.astimezone(tz=nil) → datetime` | 转换到指定时区；tz=nil 使用本地时区 |
| 方法 | `.utcoffset() → timedelta\|nil` | 时区偏移 |
| 方法 | `.dst() → timedelta\|nil` | 夏令时偏移 |
| 方法 | `.tzname() → str\|nil` | 时区名称 |
| 方法 | `.timetuple() → struct_time` | 按本地时间转 struct_time |
| 方法 | `.utctimetuple() → struct_time` | 按 UTC 时间转 struct_time |
| 方法 | `.timestamp() → float` | 转 Unix 时间戳 |
| 方法 | `.toordinal() → int` | 转格里高利序数 |
| 方法 | `.weekday() → int` | 0=周一…6=周日 |
| 方法 | `.isoweekday() → int` | 1=周一…7=周日 |
| 方法 | `.isocalendar() → (year, week, weekday)` | ISO 8601 |
| 方法 | `.isoformat(sep="T") → str` | ISO 8601 字符串；sep 可设为空格 |
| 方法 | `.strftime(fmt) → str` | 按格式代码格式化 |

### datetime.timedelta

| 方式 | 签名 | 说明 |
|---|---|---|
| 构造 | `timedelta(days=0, seconds=0, microseconds=0, milliseconds=0, minutes=0, hours=0, weeks=0)` | 所有参数可选；内部规范化为 (days, seconds, microseconds) |
| 属性 | `.days` | 规范化后的天数（可为负） |
| 属性 | `.seconds` | 规范化后的秒数，范围 [0, 86400) |
| 属性 | `.microseconds` | 规范化后的微秒数，范围 [0, 1000000) |
| 方法 | `.total_seconds() → float` | 总秒数（含小数） |
| 方法 | `.isoformat() → str` | ISO 8601 持续时间，如 `"P1DT2H3M4S"` |

### datetime.timezone

| 方式 | 签名 | 说明 |
|---|---|---|
| 构造 | `timezone(offset, name=nil)` | `offset` 为 timedelta；name 为可选字符串 |
| 常量 | `timezone.utc` | UTC 时区单例 |
| 方法 | `.utcoffset(dt) → timedelta` | 返回固定偏移 |
| 方法 | `.tzname(dt) → str` | 返回时区名称字符串 |
| 方法 | `.dst(dt) → nil` | 始终返回 nil（固定偏移无夏令时） |
| 方法 | `.fromutc(dt) → datetime` | 将 UTC datetime 转换到此时区 |

## 详细语义

### Naive 与 Aware

不含 `tzinfo` 的 `datetime`/`time` 对象称为 **naive**（朴素），含 `tzinfo`
的称为 **aware**（感知）。naive 对象不携带时区信息，直接参与算术运算；
aware 对象在比较和转换时会考虑 UTC 偏移。naive 与 aware 对象之间不可直接
比较，会抛 `TypeError`。

### timedelta 规范化

构造 `timedelta` 时所有参数都会被规范化，使得：

- `0 <= microseconds < 1_000_000`
- `0 <= seconds < 86_400`
- `days` 无上下限约束（仅受 `timedelta.min/max` 限制）

```ms
td := datetime.timedelta(minutes=90)
fmt.println(td.seconds)  // 5400（= 90*60）
fmt.println(td.days)     // 0

td2 := datetime.timedelta(days=-1, seconds=1)
fmt.println(td2.days)        // -1
fmt.println(td2.seconds)     // 1
fmt.println(td2.total_seconds())  // -86399.0
```

### datetime 算术

```ms
d1 := datetime.date(2026, 6, 3)
d2 := datetime.date(2026, 1, 1)
delta := d1 - d2          // timedelta
fmt.println(delta.days)   // 153

dt := datetime.datetime(2026, 6, 3, 12, 0, 0)
later := dt + datetime.timedelta(hours=3, minutes=30)
fmt.println(later.isoformat())  // "2026-06-03T15:30:00"
```

### strftime / strptime 格式代码

| 代码 | 含义 | 示例 |
|---|---|---|
| `%Y` | 4 位年份 | 2026 |
| `%m` | 月份（补零） | 06 |
| `%d` | 日（补零） | 03 |
| `%H` | 24 小时制时（补零） | 15 |
| `%M` | 分钟（补零） | 04 |
| `%S` | 秒（补零） | 05 |
| `%f` | 微秒（补零至 6 位） | 123456 |
| `%Z` | 时区名称 | UTC |
| `%z` | UTC 偏移 `±HHMM[SS[.ffffff]]` | +0800 |
| `%a` | 缩写星期 | Mon |
| `%A` | 完整星期 | Monday |
| `%b` | 缩写月份 | Jun |
| `%B` | 完整月份 | June |
| `%I` | 12 小时制时（补零） | 03 |
| `%p` | AM/PM | PM |
| `%j` | 年内天数（补零至 3 位） | 154 |
| `%U` | 周数（周日为首日，补零） | 22 |
| `%W` | 周数（周一为首日，补零） | 22 |
| `%c` | 本地日期时间 | Mon Jun  3 15:04:05 2026 |
| `%x` | 本地日期 | 06/03/26 |
| `%X` | 本地时间 | 15:04:05 |
| `%%` | 字面量 `%` | % |

### timezone 与 astimezone

```ms
utc := datetime.timezone.utc
cst := datetime.timezone(datetime.timedelta(hours=8), "CST")

dt_utc := datetime.datetime(2026, 6, 3, 7, 0, 0, tzinfo=utc)
dt_cst := dt_utc.astimezone(cst)
fmt.println(dt_cst.isoformat())  // "2026-06-03T15:00:00+08:00"
```

## 示例

```ms
import datetime
import fmt

// 1. 创建日期并做算术
d := datetime.date(2026, 6, 3)
tomorrow := d + datetime.timedelta(days=1)
fmt.println(tomorrow.isoformat())          // "2026-06-04"
fmt.println(d.weekday())                   // 1（周二）

// 2. 创建 datetime 并格式化
dt := datetime.datetime(2026, 6, 3, 15, 4, 5, 123456)
fmt.println(dt.strftime("%Y年%m月%d日 %H:%M:%S"))
// "2026年06月03日 15:04:05"

// 3. strptime 解析
parsed := datetime.datetime.strptime("2026-01-15 09:30", "%Y-%m-%d %H:%M")
fmt.println(parsed.year, parsed.month)     // 2026 1

// 4. isoformat 往返
dt2 := datetime.datetime.fromisoformat("2026-06-03T15:04:05")
fmt.println(dt2.isoformat())               // "2026-06-03T15:04:05"
fmt.println(dt2.isoformat(sep=" "))        // "2026-06-03 15:04:05"

// 5. timedelta 运算
week := datetime.timedelta(weeks=1)
fmt.println(week.days)                     // 7
fmt.println(week.total_seconds())          // 604800.0

half_day := datetime.timedelta(hours=12)
fmt.println((week / half_day))             // 14.0（timedelta / timedelta）

// 6. 时区转换
utc := datetime.timezone.utc
cst := datetime.timezone(datetime.timedelta(hours=8), "CST")

now_utc := datetime.datetime.now(tz=utc)
now_cst := now_utc.astimezone(cst)
fmt.println($"UTC: {now_utc.strftime('%H:%M:%S')}  CST: {now_cst.strftime('%H:%M:%S')}")

// 7. combine date + time
d2 := datetime.date.today()
t2 := datetime.time(8, 30, 0)
dt3 := datetime.datetime.combine(d2, t2, tzinfo=cst)
fmt.println(dt3.isoformat())
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | 参数超出范围（月份非 1–12 等）；strptime 格式不匹配；timezone offset 绝对值 ≥ 24 小时 |
| `TypeError` | naive 与 aware 对象混合比较或相减；offset 非 timedelta 类型 |
| `OverflowError` | timedelta 或 datetime 超出 min/max 范围 |
