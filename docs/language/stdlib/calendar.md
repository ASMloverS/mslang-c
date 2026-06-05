# calendar — 日历辅助工具

```ms
import calendar
```

## 概述

提供日历计算、星期判断和月历格式化。参考 Python `calendar` 模块，涵盖闰年
检测、月份矩阵生成和文本日历输出。默认以周一为每周第一天（符合 ISO 8601），
可通过 `setFirstWeekday` 调整。

## 常量与类型

**星期常量（值为 0–6）：**

| 常量 | 值 |
|---|---|
| `calendar.MONDAY` | 0 |
| `calendar.TUESDAY` | 1 |
| `calendar.WEDNESDAY` | 2 |
| `calendar.THURSDAY` | 3 |
| `calendar.FRIDAY` | 4 |
| `calendar.SATURDAY` | 5 |
| `calendar.SUNDAY` | 6 |

**月份常量（值为 1–12）：**

`calendar.January`(1) … `calendar.December`(12)

**名称序列：**

| 名称 | 说明 |
|---|---|
| `calendar.MONTH_NAME` | 月份全名列表，索引 0 为 `""`，索引 1–12 为 `"January"`…`"December"` |
| `calendar.MONTH_ABBR` | 月份缩写列表，同上（`"Jan"`…`"Dec"`） |
| `calendar.DAY_NAME` | 星期全名列表，索引 0–6 为 `"Monday"`…`"Sunday"` |
| `calendar.DAY_ABBR` | 星期缩写列表（`"Mon"`…`"Sun"`） |

**`calendar.Calendar(firstweekday=0)`** — 可配置首日的日历对象，见下文。

## 函数签名速查

**闰年**

| 函数 | 签名 | 说明 |
|---|---|---|
| `isLeap` | `calendar.isLeap(year) → bool` | 判断是否为闰年 |
| `leapDays` | `calendar.leapDays(y1, y2) → int` | `[y1, y2)` 区间内的闰年数 |

**星期与月份信息**

| 函数 | 签名 | 说明 |
|---|---|---|
| `weekday` | `calendar.weekday(year, month, day) → int` | 指定日期的星期（0=周一…6=周日） |
| `weekHeader` | `calendar.weekHeader(n) → str` | 每格宽度为 n 的缩写星期标题行 |
| `monthRange` | `calendar.monthRange(year, month) → (weekdayOfFirst, daysInMonth)` | 当月第一天的星期 + 当月天数 |
| `monthCalendar` | `calendar.monthCalendar(year, month) → list[list[int]]` | 以周为行的月份矩阵；月外天数为 0 |

**文本格式化**

| 函数 | 签名 | 说明 |
|---|---|---|
| `month` | `calendar.month(year, month, w=2, l=1) → str` | 返回格式化的月历文本 |
| `year` | `calendar.year(year, w=2, l=1, c=6, m=3) → str` | 返回格式化的年历文本 |
| `prMonth` | `calendar.prMonth(year, month, w=2, l=1)` | 打印月历到 stdout |
| `prYear` | `calendar.prYear(year, w=2, l=1, c=6, m=3)` | 打印年历到 stdout |

参数说明：`w` = 日期列宽，`l` = 行间距，`c` = 月份列间距，`m` = 每行月份数。

**首周日设置**

| 函数 | 签名 | 说明 |
|---|---|---|
| `setFirstWeekday` | `calendar.setFirstWeekday(weekday)` | 设置每周第一天；0=周一（默认），6=周日 |
| `firstWeekday` | `calendar.firstWeekday() → int` | 返回当前首周日设置 |

**时间戳转换**

| 函数 | 签名 | 说明 |
|---|---|---|
| `timeGm` | `calendar.timeGm(t) → int` | UTC StructTime → Unix 时间戳；`time.gmtime` 的逆操作 |

## 详细语义

### calendar.monthRange(year, month)

返回元组 `(weekdayOfFirst, daysInMonth)`：

- `weekdayOfFirst`：当月 1 日是星期几（0=周一…6=周日），受 `setFirstWeekday` **不**影响（始终相对 ISO 星期）。
- `daysInMonth`：当月总天数（28/29/30/31）。

```ms
firstDay, total := calendar.monthRange(2026, 2)
fmt.println(firstDay, total)  // 6 28（2026年2月1日是周日，共28天）
```

### calendar.monthCalendar(year, month)

返回列表的列表，每个子列表代表一周（7 个整数）。月份范围外的天数填 0。

```ms
weeks := calendar.monthCalendar(2026, 6)
// 示例输出（首日=周一）：
// [[1,  2,  3,  4,  5,  6,  7],
//  [8,  9, 10, 11, 12, 13, 14],
//  [15, 16, 17, 18, 19, 20, 21],
//  [22, 23, 24, 25, 26, 27, 28],
//  [29, 30,  0,  0,  0,  0,  0]]
```

### calendar.timeGm(t)

将 UTC `StructTime` 转为 Unix 时间戳（int），忽略本地时区设置。
等价于 Python `calendar.timeGm`。与 `time.mktime` 的区别：`mktime` 按本地
时区解释，`timeGm` 始终按 UTC 解释。

```ms
import time

t := time.gmtime(0)          // epoch
fmt.println(calendar.timeGm(t))  // 0
```

### calendar.Calendar 类

`Calendar(firstweekday=0)` 创建一个可迭代日历对象，`firstweekday` 独立于
全局 `setFirstWeekday` 设置。

| 方法 | 签名 | 说明 |
|---|---|---|
| `iterMonthDates` | `c.iterMonthDates(year, month) → iterator[date]` | 按周补全的月份所有日期（含前后月补位） |
| `iterMonthDays` | `c.iterMonthDays(year, month) → iterator[int]` | 同上，但返回日序号；月外为 0 |
| `monthDatesCalendar` | `c.monthDatesCalendar(year, month) → list[list[date]]` | 以周为行的 date 矩阵 |
| `yearDatesCalendar` | `c.yearDatesCalendar(year, width=3) → list` | 以季度为单位的年度日期矩阵 |

## 示例

```ms
import calendar
import datetime
import fmt

// 1. 闰年判断
fmt.println(calendar.isLeap(2024))   // true
fmt.println(calendar.isLeap(2026))   // false
fmt.println(calendar.leapDays(2000, 2026))  // 7

// 2. 获取月份信息
firstDay, days := calendar.monthRange(2026, 6)
fmt.println($"2026年6月共 {days} 天，1日是 {calendar.DAY_NAME[firstDay]}")
// "2026年6月共 30 天，1日是 Monday"

// 3. 打印当月日历
calendar.prMonth(2026, 6)
//      June 2026
// Mo Tu We Th Fr Sa Su
//  1  2  3  4  5  6  7
//  8  9 10 11 12 13 14
// 15 16 17 18 19 20 21
// 22 23 24 25 26 27 28
// 29 30

// 4. 遍历月份矩阵，找出所有周五
weeks := calendar.monthCalendar(2026, 6)
fridays := []
for week in weeks {
    if week[calendar.FRIDAY] != 0 {
        fridays.append(week[calendar.FRIDAY])
    }
}
fmt.println("6月的所有周五:", fridays)  // [5, 12, 19, 26]

// 5. 使用 Calendar 对象迭代日期
cal := calendar.Calendar(firstweekday=calendar.SUNDAY)
for d in cal.iterMonthDates(2026, 6) {
    if d.month == 6 && d.weekday() == calendar.MONDAY {
        fmt.println("周一:", d.isoformat())
    }
}

// 6. timeGm 与 gmtime 往返
import time
ts := 1748908800
t := time.gmtime(ts)
fmt.println(calendar.timeGm(t) == ts)  // true

// 7. 月份/星期名称
fmt.println(calendar.MONTH_NAME[6])    // "June"
fmt.println(calendar.DAY_ABBR[0])      // "Mon"
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `setFirstWeekday` 接收到 0–6 以外的值；`monthRange`/`monthCalendar` 月份非 1–12；`leapDays` y1 > y2 |
| `TypeError` | 参数类型不符（如 year 传入 float） |
