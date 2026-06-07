# P12-T170 stdlib: datetime（date / time 类型）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `datetime` 模块的基础类型：`date` 和 `time`（对齐 `stdlib/datetime.md`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T142 | time 模块（时间戳） |
| P5-T072 | class 系统 |

---

## API 清单（本任务：date + time）

```ms
// date（不含时间的日期）
d := datetime.date(2024, 1, 15)     // year, month, day
datetime.date.today() → date        // 当前日期（本地时区）
datetime.date.fromtimestamp(ts)     // Unix 时间戳 → date
datetime.date.fromordinal(n)        // 儒略日 → date（1=公元1年1月1日）
datetime.date.fromisoformat("2024-01-15") → date

d.year   d.month   d.day
d.timetuple()        // struct_time
d.toordinal() → int  // 儒略日序数
d.weekday() → int    // 0=Monday ... 6=Sunday
d.isoweekday() → int // 1=Monday ... 7=Sunday
d.isocalendar() → IsoCalendarDate  // (year, week, weekday)
d.isoformat() → str  // "2024-01-15"
d.strftime(format) → str
d.replace(year=nil, month=nil, day=nil) → date
d + timedelta → date
d - timedelta → date
d - d2 → timedelta
d == d2  d < d2  ...（比较）
datetime.date.min   datetime.date.max   datetime.date.resolution

// time（不含日期的时间）
t := datetime.time(12, 30, 0, 500000)  // hour, min, sec, microsecond
t := datetime.time(12, 30, tzinfo=tz)
datetime.time.fromisoformat("12:30:00.500000")

t.hour   t.minute   t.second   t.microsecond   t.tzinfo
t.isoformat(timespec="auto") → str  // "12:30:00.500000"
t.strftime(format) → str
t.replace(**kwargs) → time
t.utcoffset() → timedelta|nil
t.tzname() → str|nil
t.dst() → timedelta|nil
t == t2  t < t2  ...
datetime.time.min   datetime.time.max   datetime.time.resolution
```

---

## 实现要点

```c
// date 内部：year(int16_t), month(uint8_t), day(uint8_t) → 4 字节
// 合法性检查：1<=year<=9999, 1<=month<=12, 1<=day<=days_in_month(y,m)
// 闰年：(y%4==0 && y%100!=0) || y%400==0

typedef struct MsDateObj {
  MsObject header;
  int16_t  year;    // 1..9999
  uint8_t  month;   // 1..12
  uint8_t  day;     // 1..31
} MsDateObj;

// toordinal：Proleptic Gregorian calendar 序数
// 算法：累加每年天数 + 本年月天数 + day
// fromordinal：逆推（二分搜索或直接计算）

// days_in_month 查表：[0,31,28,31,30,31,30,31,31,30,31,30,31]
// 闰年 2 月 = 29 天

// time 内部：hour(uint8_t), min(uint8_t), sec(uint8_t),
//           usec(uint32_t), tzinfo(MsValue)
typedef struct MsTimeObj {
  MsObject header;
  uint8_t  hour, min, sec;
  uint32_t usec;      // 微秒（0..999999）
  MsValue  tzinfo;    // nil = naive
} MsTimeObj;

// strftime：支持标准格式码
// %Y %m %d %H %M %S %f %z %Z %a %A %b %B %p %c %x %X %%
// 内部调用 C strftime 或自实现
```

---

## 验收标准（checklist）

- [ ] `date(2024, 2, 29)` 有效（2024 是闰年），`date(2023, 2, 29)` 抛 ValueError。
- [ ] `date.today()` 返回今天的日期（与系统时间一致）。
- [ ] `date(2024, 1, 15).toordinal()` 与 Python 标准一致。
- [ ] `date.fromordinal(n).toordinal() == n`（round-trip）。
- [ ] `date.fromisoformat("2024-01-15")` 正确解析。
- [ ] `date(2024,1,15).weekday()` → `0`（星期一）。

---

## 测试用例（.ms）

```ms
import datetime

// date 基础
d := datetime.date(2024, 1, 15)
print(d.year, d.month, d.day)  // 2024 1 15
print(d.weekday())              // 0 (Monday)
print(d.isoformat())            // 2024-01-15
print(d.strftime("%B %d, %Y")) // January 15, 2024

// 闰年
print(datetime.date(2024, 2, 29))  // 合法
try:
    datetime.date(2023, 2, 29)
catch e:
    print("ValueError:", e)

// ordinal round-trip
ord := d.toordinal()
print(datetime.date.fromordinal(ord) == d)  // true

// time 基础
t := datetime.time(14, 30, 15, 500000)
print(t.hour, t.minute, t.second, t.microsecond)
// 14 30 15 500000
print(t.isoformat())  // 14:30:15.500000
print(t.isoformat(timespec="seconds"))  // 14:30:15

// time 比较
t1 := datetime.time(10, 0)
t2 := datetime.time(12, 0)
print(t1 < t2)  // true
```
