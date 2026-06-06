# P12-T179 stdlib: http.client

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `http.client` 模块（对齐 `stdlib/http.md` 客户端部分）：HTTP/1.1 客户端，支持 GET/POST 等请求，Keep-Alive，chunked 传输。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T176 | socket |
| P12-T177 | net |
| P12-T178 | url |

---

## API 清单

```ms
// 便捷函数（顶层）
resp := http.get("https://example.com/api")
resp := http.post("https://example.com/api", data=b"body")
resp := http.request("PUT", url, headers={}, data=nil, timeout=30.0)

// Response 对象
resp.status       // int（200, 404...）
resp.reason       // str（"OK", "Not Found"...）
resp.headers      // dict（Header 对象，大小写不敏感）
resp.url          // 最终 URL（重定向后）
resp.text         // str（自动解码）
resp.content      // bytes
resp.json()       // 解析 JSON（调用 json.loads）
resp.iter_content(chunk_size=1024)  // 流式读取
resp.close()

// HTTPConnection（低层）
conn := http.client.HTTPConnection("example.com", port=80, timeout=30)
conn.request("GET", "/path", headers={}, body=nil)
resp := conn.getresponse()
resp.read(n=-1) → bytes
resp.getheader(name, default=nil)
resp.getheaders() → list[(name,value)]
resp.status   resp.reason
conn.close()

// HTTPSConnection（TLS，可选：若无 TLS 自实现，标注 NotImplemented/依赖 OpenSSL）
// 简化：先实现 HTTP，HTTPS 在 P13+ 阶段

// Session（连接复用，Keep-Alive 管理）
sess := http.Session()
sess.get(url, **kwargs) → Response
sess.post(url, **kwargs) → Response
sess.headers["User-Agent"] = "mslang/1.0"
sess.close()

// 常量
http.client.HTTP_PORT   // 80
http.client.HTTPS_PORT  // 443
```

---

## 实现要点

```c
// HTTP/1.1 请求格式：
// "METHOD /path HTTP/1.1\r\n"
// "Host: host:port\r\n"
// "Connection: keep-alive\r\n"
// [Other headers]
// "Content-Length: N\r\n"（若有 body）
// "\r\n"
// [body bytes]

// HTTP/1.1 响应解析：
// 状态行："HTTP/1.1 200 OK\r\n"
// 头部：读行直到空行
// body：按 Content-Length 读取，或 chunked 传输解码

// chunked 解码（RFC 7230 §4.1）：
// 循环：读 hex_size\r\n → 读 hex_size 字节 → 读 \r\n
// hex_size=0 → 结束（可选 trailer）

// Keep-Alive：连接复用
// 若响应头包含 Connection: keep-alive，保持 socket 打开
// Session 管理 host→socket 池（简单版：每 host 一个连接）

// 重定向处理（http.get 便捷函数）：
// 3xx 响应 → 提取 Location → 最多 30 次重定向
// 跨协议重定向（https→http）按规范处理

// 请求头自动添加：
// Host（必须）, Content-Length（有 body 时）, User-Agent

typedef struct MsHttpConnObj {
    MsObject header;
    MsConnObj* conn;      // net.Conn
    char*      host;
    int        port;
    double     timeout;
    bool       persistent;  // keep-alive
    bool       busy;
} MsHttpConnObj;
```

---

## 验收标准（checklist）

- [ ] `http.get("http://httpbin.org/get")` 返回 200 响应。（集成测试，可跳过）
- [ ] 本地 HTTP 测试：`http.get("http://127.0.0.1:PORT/")` 与 T180 server 配合。
- [ ] chunked 响应正确拼接为完整 body。
- [ ] Keep-Alive：同一 Session 发送 3 次请求，只建立 1 个 TCP 连接。
- [ ] `resp.json()` 正确解析 JSON 响应体。
- [ ] timeout 超时后抛 `socket.timeout`。

---

## 测试用例（.ms）

```ms
import http.client as http, json

// 基础 GET（需要本地 echo server）
// 参见 T180（http.server）
// 本地测试：先启动 T180 的 echo server，再运行此

resp := http.get("http://127.0.0.1:18888/hello")
print(resp.status)   // 200
print(resp.text)     // "hello"

// POST with JSON body
import json as js
payload := js.dumps({"name": "alice", "age": 30})
resp2 := http.post("http://127.0.0.1:18888/echo",
                   data=payload.encode(),
                   headers={"Content-Type": "application/json"})
print(resp2.json())   // {"name":"alice","age":30}

// Session（Keep-Alive）
with http.Session() as sess:
    for _ in range(3) {
        r := sess.get("http://127.0.0.1:18888/ping")
        print(r.status)  // 200
}
// 上述 3 次请求复用同一 TCP 连接
```
