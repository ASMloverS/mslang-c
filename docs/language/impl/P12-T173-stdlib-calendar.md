# P12-T173 stdlib: calendar

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `calendar` 模块（对齐 `stdlib/calendar.md`）：日历计算、格式化、日期工具函数。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T170 | datetime.date |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-calendar.md` | §1 模块 API |

---

## API 清单

```ms
// 基础工具函数
calendar.isleap(year) → bool           // 是否闰年
calendar.leapdays(y1, y2) → int        // [y1, y2) 区间内闰年数
calendar.weekday(year, month, day) → int  // 0=Mon..6=Sun
calendar.weekheader(n) → str           // 星期列标题（n=2,3...）
calendar.monthrange(year, month) → (weekday, ndays)  // 月份第一天星期+天数
calendar.monthcalendar(year, month) → list[list[int]]  // 月历二维表
calendar.month(year, month, w=0, l=0) → str  // 格式化月份文字
calendar.prmonth(year, month)          // 打印月份
calendar.calendar(year, w=0, l=0, c=0) → str  // 全年日历字符串
calendar.prcal(year)                   // 打印全年日历

// 常量
calendar.MONDAY    // = 0
calendar.TUESDAY   // = 1
// ...
calendar.SUNDAY    // = 6
calendar.month_name   // ["","January","February",...,"December"]
calendar.month_abbr   // ["","Jan","Feb",...]
calendar.day_name     // ["Monday","Tuesday",...,"Sunday"]
calendar.day_abbr     // ["Mon","Tue",...]

// 迭代器
calendar.itermonthdates(year, month) → iterator[date]
// 包含完整周（前后可能含相邻月日期）
calendar.itermonthdays(year, month) → iterator[int]
// 同上，但非本月天数用 0 表示
calendar.itermonthdays2(year, month) → iterator[(day, weekday)]

// TextCalendar / HTMLCalendar
cal := calendar.TextCalendar(firstweekday=0)
cal.formatmonth(year, month, w=0, l=0) → str
cal.formatyear(year, w=0, l=0, c=0) → str
cal.prmonth(year, month)
cal.pryear(year)
```

---

## 实现要点

```c
// Zeller 公式计算星期（或调用 datetime.date.weekday）
// monthrange(year, month)：
//   d = date(year, month, 1).weekday()
//   n = 28/29/30/31（视月份+闰年）
// monthcalendar：按周分组，每行 7 个，0 表示该格无日期

// TextCalendar 格式化：
// 列宽 = max(w, 2)；行间距 = max(l, 1)
// 月份标题居中、星期标题、日期网格

// 月份名称和星期名称：使用 setlocale 系统本地化（简化：默认英文，
// 后续 locale 模块可覆盖 month_name 等）
```

---

## 验收标准（checklist）

- [ ] `calendar.isleap(2024)` → `true`，`calendar.isleap(1900)` → `false`。
- [ ] `calendar.monthrange(2024, 2)` → `(3, 29)`（2024年2月1日是周四=3，29天）。
- [ ] `calendar.month(2024, 1)` 格式化输出与标准 Python 一致。
- [ ] `calendar.leapdays(2000, 2025)` → `7`。
- [ ] `itermonthdates` 包含完整周（首尾可含相邻月）。

---

## 测试用例（.ms）

```ms
import calendar

// 基础
print(calendar.isleap(2024))     // true
print(calendar.leapdays(2000, 2025))  // 7
print(calendar.weekday(2024, 1, 15))  // 0 (Monday)
print(calendar.monthrange(2024, 2))   // (3, 29)

// 格式化
print(calendar.month(2024, 1))
//    January 2024
// Mo Tu We Th Fr Sa Su
//  1  2  3  4  5  6  7
//  8  9 10 11 12 13 14
// 15 16 17 18 19 20 21
// 22 23 24 25 26 27 28
// 29 30 31

// monthcalendar
mc := calendar.monthcalendar(2024, 1)
print(len(mc))        // 5（5行）
print(mc[0])          // [1,2,3,4,5,6,7]（第一周）

// 迭代器
from datetime import date
dates := list(calendar.itermonthdates(2024, 1))
print(dates[0])   // 2024-01-01 或 2023-12-28（取决于 firstweekday）
print(len(dates)) // 35 或 28 或 42（取决于月份排列）
```
