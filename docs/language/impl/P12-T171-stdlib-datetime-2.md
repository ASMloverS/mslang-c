# P12-T171 stdlib: datetime（datetime / timedelta）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `datetime.datetime` 和 `datetime.timedelta`（对齐 `stdlib/datetime.md`），完成核心日期时间计算。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T170 | datetime date/time |

---

## API 清单

```ms
// timedelta（时间间隔）
td := datetime.timedelta(days=0, seconds=0, microseconds=0,
                         milliseconds=0, minutes=0, hours=0, weeks=0)
// 内部规范化为：days(-999999999..999999999) + seconds(0..86399) + microseconds(0..999999)

td.days   td.seconds   td.microseconds
td.total_seconds() → float   // 总秒数（含小数）

td + td2   td - td2   td * n   n * td   td / n   td // n   td % td2
-td   abs(td)   td == td2   td < td2   ...
datetime.timedelta.min   datetime.timedelta.max   datetime.timedelta.resolution

// datetime（date + time 组合）
dt := datetime.datetime(2024, 1, 15, 14, 30, 0)
datetime.datetime.now(tz=nil)          // 当前本地/UTC 时间
datetime.datetime.utcnow()             // UTC 时间（naive）
datetime.datetime.today()              // 同 now()（无 tz 参数）
datetime.datetime.fromtimestamp(ts, tz=nil)
datetime.datetime.utcfromtimestamp(ts)
datetime.datetime.fromisoformat("2024-01-15T14:30:00.000000")
datetime.datetime.strptime(date_str, format)
datetime.datetime.combine(date, time, tzinfo=nil)
datetime.datetime.fromordinal(n)

dt.year  dt.month  dt.day  dt.hour  dt.minute  dt.second
dt.microsecond  dt.tzinfo  dt.fold

dt.date() → date
dt.time() → time
dt.timetz() → time  // 含 tzinfo

dt.replace(**kwargs) → datetime
dt.astimezone(tz=nil) → datetime   // 转换时区
dt.utcoffset() → timedelta|nil
dt.dst() → timedelta|nil
dt.tzname() → str|nil

dt.timetuple()
dt.toordinal() → int
dt.timestamp() → float   // Unix 时间戳
dt.weekday() → int
dt.isoweekday() → int
dt.isocalendar() → IsoCalendarDate

dt.isoformat(sep="T", timespec="auto") → str
dt.strftime(format) → str

dt + timedelta → datetime
dt - timedelta → datetime
dt - dt2 → timedelta
dt == dt2  dt < dt2  ...
```

---

## 实现要点

```c
// timedelta 内部表示（规范化后）：
// days: int32_t, seconds: int32_t, microseconds: int32_t
// 规范化：先将所有参数转为 usec，再分解
// total_microseconds = days*86400*1e6 + seconds*1e6 + microseconds

typedef struct MsTimeDeltaObj {
  MsObject header;
  int32_t  days;         // -999999999 .. 999999999
  int32_t  seconds;      // 0 .. 86399
  int32_t  microseconds; // 0 .. 999999
} MsTimeDeltaObj;

// datetime 内部（继承 date 的字段，追加 time 字段）
typedef struct MsDateTimeObj {
  MsObject header;
  int16_t  year;
  uint8_t  month, day;
  uint8_t  hour, min, sec;
  uint32_t usec;
  MsValue  tzinfo;    // nil = naive
  uint8_t  fold;      // 0 or 1（DST fold）
} MsDateTimeObj;

// timestamp()：
// naive：假定本地时间，使用 mktime() 转 UTC
// aware：(dt - epoch).total_seconds()，epoch = datetime(1970,1,1,tzinfo=UTC)

// strptime：解析格式字符串，与 strftime 格式码对应
// %Y %m %d %H %M %S %f %Z %z %a %A %b %B
```

---

## 验收标准（checklist）

- [ ] `timedelta(days=1, hours=2).total_seconds()` → `93600.0`。
- [ ] `timedelta(seconds=86401)` 规范化为 `days=1, seconds=1`。
- [ ] `datetime.now() - datetime.now()` 接近 `timedelta(0)`（< 1ms）。
- [ ] `datetime(2024,1,15) + timedelta(days=30)` → `datetime(2024,2,14)`。
- [ ] `datetime.strptime("2024-01-15 14:30", "%Y-%m-%d %H:%M")` 正确解析。
- [ ] `dt.timestamp()` round-trip：`fromtimestamp(dt.timestamp()) ≈ dt`。

---

## 测试用例（.ms）

```ms
import datetime

// timedelta 计算
td := datetime.timedelta(hours=25, minutes=90)
print(td.days)         // 1
print(td.seconds)      // 5400（1.5小时=5400秒，超出1天的部分）
print(td.total_seconds())  // 96600.0

// datetime 算术
start := datetime.datetime(2024, 1, 15, 12, 0, 0)
end := start + datetime.timedelta(days=30, hours=6)
print(end)             // 2024-02-14 18:00:00
diff := end - start
print(diff.days)       // 30
print(diff.seconds)    // 21600（6小时）

// strptime / strftime
dt := datetime.datetime.strptime("2024-03-25 09:30:00", "%Y-%m-%d %H:%M:%S")
print(dt.strftime("%A, %B %d, %Y"))  // Monday, March 25, 2024

// timestamp round-trip
import time
ts := time.now()
dt2 := datetime.datetime.fromtimestamp(ts)
print(abs(dt2.timestamp() - ts) < 0.001)  // true

// 跨年计算
ny := datetime.datetime(2024, 12, 31, 23, 0, 0)
next_day := ny + datetime.timedelta(hours=2)
print(next_day.isoformat())  // 2025-01-01T01:00:00
```
