# P12-T172 stdlib: datetime（timezone / 解析）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

完成 `datetime` 模块的时区支持（`timezone`、`tzinfo` 基类）和高级解析（ISO 8601 完整格式、时区转换）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T171 | datetime/timedelta |

---

## API 清单

```ms
// tzinfo 抽象基类（用于自定义时区）
class MyTZ(datetime.tzinfo):
    func utcoffset(dt) → timedelta
    func dst(dt) → timedelta|nil    // 夏令时偏移
    func tzname(dt) → str

// timezone（固定偏移时区，tzinfo 的具体实现）
datetime.timezone(offset, name=nil)      // offset 为 timedelta
datetime.timezone.utc                    // UTC（+00:00）
datetime.timezone(datetime.timedelta(hours=8))  // CST（+08:00）

// 典型用法
UTC := datetime.timezone.utc
now_utc := datetime.datetime.now(tz=UTC)
now_local := now_utc.astimezone()  // 转本地时区

// ISO 8601 扩展解析
datetime.datetime.fromisoformat("2024-01-15T14:30:00+08:00")
datetime.datetime.fromisoformat("2024-01-15T14:30:00.123456Z")
datetime.time.fromisoformat("14:30:00+08:00")
datetime.date.fromisoformat("2024-W03-1")  // ISO week date

// 时区转换
dt_utc := datetime.datetime(2024, 1, 15, 6, 30, tzinfo=datetime.timezone.utc)
dt_cst := dt_utc.astimezone(datetime.timezone(datetime.timedelta(hours=8)))
print(dt_cst.hour)  // 14（6 UTC + 8 = 14 CST）

// aware vs naive
dt_naive := datetime.datetime(2024, 1, 15, 12, 0)
dt_aware := datetime.datetime(2024, 1, 15, 12, 0, tzinfo=datetime.timezone.utc)
print(dt_naive.tzinfo)   // nil
print(dt_aware.utcoffset())  // 0:00:00

// 本地时区（使用系统时区）
datetime.timezone.local = datetime.timezone(local_utc_offset)
// 从 time.localtime() 获取本地偏移
```

---

## 实现要点

```c
// timezone 对象：固定偏移，存储 timedelta offset + name
typedef struct MsTzInfoObj {
  MsObject  header;
  MsValue   offset;  // timedelta
  MsValue   name;    // str|nil
} MsTzInfoObj;

// astimezone 转换：
// 1. 若 dt 是 naive：先假定为本地时间，加本地 utcoffset，变为 UTC aware
// 2. dt 减去 self.utcoffset() → UTC datetime
// 3. 加目标 tz 的 utcoffset() → 目标时区 datetime

// ISO 8601 解析扩展（fromisoformat）：
// 支持 Z 后缀（= +00:00）
// 支持 ±HH:MM / ±HHMM / ±HH 偏移格式
// 支持 ISO week date：2024-W03-1
// 支持紧凑格式：20240115T143000

// 本地 UTC 偏移：
// POSIX：从 tm_gmtoff（tm 结构扩展）
// Windows：GetTimeZoneInformation → Bias（分钟）

// fold 处理（夏令时回拨时重叠小时）：
// 见 PEP 495；fold=0 表示第一次出现，fold=1 表示第二次
// 固定偏移时区不产生 fold
```

---

## 验收标准（checklist）

- [ ] `datetime.timezone.utc.utcoffset(nil)` → `timedelta(0)`。
- [ ] `fromisoformat("2024-01-15T14:30:00Z")` 解析为 UTC aware datetime。
- [ ] `fromisoformat("2024-01-15T14:30:00+08:00").utcoffset()` → `timedelta(hours=8)`。
- [ ] `astimezone` 在两个固定偏移时区间正确转换（验证时间差恒定）。
- [ ] 两个 aware datetime 相减得到正确 timedelta（跨时区）。
- [ ] naive 与 aware datetime 混合运算抛 TypeError。

---

## 测试用例（.ms）

```ms
import datetime

UTC = datetime.timezone.utc
CST = datetime.timezone(datetime.timedelta(hours=8), "CST")

// 时区转换
dt_utc := datetime.datetime(2024, 1, 15, 6, 30, 0, tzinfo=UTC)
dt_cst := dt_utc.astimezone(CST)
print(dt_cst.hour)         // 14
print(dt_cst.tzname())     // "CST"
print(dt_cst.utcoffset())  // 8:00:00

// aware 相减（跨时区）
dt1 := datetime.datetime(2024, 1, 1, 0, 0, tzinfo=UTC)
dt2 := datetime.datetime(2024, 1, 1, 8, 0, tzinfo=CST)
diff := dt1 - dt2
print(diff)       // 0:00:00（同一时刻）

// ISO 解析
dt3 := datetime.datetime.fromisoformat("2024-01-15T14:30:00.123456+08:00")
print(dt3.microsecond)   // 123456
print(dt3.utcoffset())   // 8:00:00

// naive vs aware
naive := datetime.datetime(2024, 1, 1)
aware := datetime.datetime(2024, 1, 1, tzinfo=UTC)
try:
    _ = naive - aware
catch e:
    print("TypeError:", e)   // 预期错误

// Z 后缀
dt4 := datetime.datetime.fromisoformat("2024-01-15T00:00:00Z")
print(dt4.tzinfo == UTC)  // true
```
