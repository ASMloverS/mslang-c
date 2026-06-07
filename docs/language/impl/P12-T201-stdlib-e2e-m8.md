# P12-T201 stdlib 综合端到端测试（M8 里程碑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

M8 里程碑：验证 P12 全部 stdlib 模块（T133–T200）可正确协同工作，通过综合端到端测试套件。

---

## 前置依赖

全部 P12 任务（T133–T200）。

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-e2e-m8.md` | §1 模块 API |

---

## 测试场景清单

### 场景 1：数据处理管道（collections + itertools + functools + sort）

```ms
// 文件名：tests/ms/m8/data_pipeline.ms
import collections, itertools, functools, sort

// 模拟日志分析：统计每分钟的 HTTP 状态码分布
logs := [
    {"time": "10:00:01", "status": 200},
    {"time": "10:00:30", "status": 404},
    {"time": "10:01:15", "status": 200},
    {"time": "10:01:45", "status": 500},
    {"time": "10:01:58", "status": 200},
]

// 按分钟分组
by_minute := collections.defaultdict(list)
for log in logs:
    minute := log["time"][:5]   // "10:00" 或 "10:01"
    by_minute[minute].append(log["status"])

// 每分钟统计
for minute, statuses in sort.sorted(by_minute.items()):
    c := collections.Counter(statuses)
    print(minute, dict(c.mostCommon()))

// 期望：
// 10:00 {200: 1, 404: 1}
// 10:01 {200: 2, 500: 1}

// 使用 itertools + functools 管道
get_status := functools.partial(lambda key, d: d[key], "status")
statuses := list(map(get_status, logs))
print(collections.Counter(statuses))
// Counter({200: 3, 404: 1, 500: 1})
```

### 场景 2：JSON + datetime + hashlib（API 服务模拟）

```ms
// 文件名：tests/ms/m8/api_mock.ms
import json, datetime, hashlib, secrets

func generate_token(user_id) {
    payload := {
        "user_id": user_id,
        "issued_at": datetime.datetime.utcnow().isoformat(),
        "expires": (datetime.datetime.utcnow() + datetime.timedelta(hours=1)).isoformat(),
        "jti": secrets.token_hex(16)   // JWT ID
    }
    body := json.dumps(payload, sortKeys=true)
    signature := hashlib.sha256(body.encode()).hexdigest()
    return {"body": body, "sig": signature}

func verify_token(token) {
    body := token["body"]
    expected_sig := hashlib.sha256(body.encode()).hexdigest()
    return hmac.compareDigest(token["sig"], expected_sig)
}

token := generate_token(42)
print(verify_token(token))   // true

// 解析 payload
import hmac
payload := json.loads(token["body"])
print(payload["user_id"])   // 42
print(payload["jti"])       // 32字符hex

// 过期检查
issued := datetime.datetime.fromisoformat(payload["issued_at"])
expires := datetime.datetime.fromisoformat(payload["expires"])
print(expires - issued)   // 1:00:00
```

### 场景 3：HTTP 服务 + JSON + logging（mini web service）

```ms
// 文件名：tests/ms/m8/web_service.ms
import http.server as srv, http.client as cli
import json, logging, time

// 配置日志
logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
logger := logging.getLogger("api")

// 内存数据库
db := {}

server := srv.Server(":19900")

@server.get("/items")
func list_items(req, resp) {
    logger.info("GET /items")
    resp.json(list(db.values()))
}

@server.post("/items")
func create_item(req, resp) {
    item := req.json()
    item["id"] = len(db) + 1
    db[str(item["id"])] = item
    logger.info("POST /items id=%d", item["id"])
    resp.status = 201
    resp.json(item)
}

@server.get("/items/:id")
func get_item(req, resp) {
    id := req.params["id"]
    if id not in db:
        resp.status = 404
        resp.json({"error": "not found"})
        return
    resp.json(db[id])
}

go server.serveBackground()
time.sleep(0.1)

// 测试
r1 := cli.post("http://127.0.0.1:19900/items",
               data=json.dumps({"name":"item1","price":9.99}).encode(),
               headers={"Content-Type":"application/json"})
print(r1.status)            // 201
print(r1.json()["id"])      // 1

r2 := cli.post("http://127.0.0.1:19900/items",
               data=json.dumps({"name":"item2","price":4.99}).encode(),
               headers={"Content-Type":"application/json"})
print(r2.json()["id"])      // 2

r3 := cli.get("http://127.0.0.1:19900/items")
items := r3.json()
print(len(items))           // 2

r4 := cli.get("http://127.0.0.1:19900/items/1")
print(r4.json()["name"])    // "item1"

r5 := cli.get("http://127.0.0.1:19900/items/999")
print(r5.status)            // 404

server.close()
```

### 场景 4：并发爬虫（net + re + context + sync）

```ms
// 文件名：tests/ms/m8/concurrent_fetch.ms
// （使用 mock server 替代真实 HTTP）
import net, re, context, sync, time

// 并发 fetch 多个 URL（使用已启动的 T180 echo server）
urls := ["/page/1", "/page/2", "/page/3", "/page/4", "/page/5"]
results := sync.Mutex()
data := {}
wg := sync.WaitGroup()
wg.add(len(urls))

ctx, cancel := context.withTimeout(context.Background(), 5.0)
defer cancel()

for url in urls:
    go func(u) {
        r := cli.get("http://127.0.0.1:18888" + u)
        with results:
            data[u] = r.status
        wg.done()
    }(url)

wg.wait()
print(len(data))   // 5
print(all(v == 200 for v in data.values()))  // true（或 404）
```

### 场景 5：压缩 + 归档管道

```ms
// 文件名：tests/ms/m8/archive_pipeline.ms
import gzip, zipfile, tarfile, io, hashlib, tempfile, os

// 生成测试数据
test_data := {
    "file1.txt": b"Hello, World!" * 100,
    "file2.txt": b"Another file content" * 50,
    "config.json": json.dumps({"key": "value", "count": 42}).encode(),
}

// 1. gzip 每个文件
compressed := {}
for name, content in test_data.items():
    compressed[name + ".gz"] = gzip.compress(content)
    // 验证 round-trip
    assert gzip.decompress(compressed[name + ".gz"]) == content

// 2. 打包为 zip
with tempfile.NamedTemporaryFile(suffix=".zip", delete=false) as f:
    zip_path := f.name

with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for name, content in test_data.items():
        zf.writestr(name, content)

with zipfile.ZipFile(zip_path, "r") as zf:
    for name in zf.namelist():
        extracted := zf.read(name)
        assert extracted == test_data[name]

// 3. 打包为 tar.gz
with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=false) as f:
    tar_path := f.name

with tarfile.open(tar_path, "w:gz") as tf:
    for name, content in test_data.items():
        ti := tarfile.TarInfo(name=name)
        ti.size = len(content)
        tf.addfile(ti, io.BytesIO(content))

with tarfile.open(tar_path, "r:gz") as tf:
    print(sorted(tf.getnames()))  // 按字母排序

os.unlink(zip_path)
os.unlink(tar_path)
print("Archive pipeline: PASS")
```

---

## M8 里程碑验收标准

- [ ] 上述 5 个场景全部通过，无崩溃、无断言失败。
- [ ] `mslang test tests/ms/m8/` 发现并运行所有场景，全部 PASS。
- [ ] stdlib 覆盖率：P12 全部 68 个任务（T133–T200）对应模块均有至少一个测试用例通过。
- [ ] 性能：所有测试套件总运行时间 < 60 秒。
- [ ] 内存：运行 5 个场景后无 GC 泄漏（gc.stats().uncollectable == 0）。

---

## M8 里程碑 Benchmark 套件

```ms
// 文件名：benchmarks/m8_stdlib.ms
import time, json, hashlib, re, sort, collections

func bench(name, fn, n=1000) {
    t0 := time.perfCounter()
    for _ in range(n) { fn() }
    t1 := time.perfCounter()
    print($"{name}: {(t1-t0)*1000/n:.3f} ms/op")
}

// JSON
bench("json.dumps 小对象",
    lambda: json.dumps({"x":1,"y":2,"z":3}))
bench("json.loads 小 JSON",
    lambda: json.loads('{"x":1,"y":2,"z":3}'))

// hashlib
data := b"benchmark data" * 100
bench("sha256 1.4KB", lambda: hashlib.sha256(data))

// re
pat := re.compile(r"\b\w{5,10}\b")
text := "The quick brown fox jumps over the lazy dog." * 10
bench("re.findall", lambda: re.findall(pat, text))

// sort
import random
random.seed(42)
lst := [random.randint(0,1000) for _ in range(1000)]
bench("sort 1K ints", lambda: sort.sorted(lst), n=100)

// Counter
words := "the quick brown fox jumps over the lazy dog".split() * 100
bench("Counter", lambda: collections.Counter(words))
```
