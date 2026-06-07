# P12-T181 stdlib: http（WebSocket / Server-Sent Events）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

扩展 `http` 模块，实现 WebSocket（RFC 6455）和 Server-Sent Events（SSE）支持，完成异步 http 高级功能。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T179 | http.client |
| P12-T180 | http.server |
| P12-T163 | hashlib SHA-1（WebSocket 握手） |
| P12-T157 | base64（WebSocket 握手） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-http-async.md` | §1 模块 API |

---

## API 清单

```ms
// WebSocket 服务端
@server.websocket("/ws")
func handle_ws(ws) {
    while true {
        msg := ws.recv()    // 阻塞接收（协程友好）
        if msg == nil { break }    // 连接关闭
        ws.send(msg)        // echo
    }
}

// ws 对象
ws.send(data)               // str → text frame; bytes → binary frame
ws.recv() → str|bytes|nil   // nil = 连接关闭
ws.close(code=1000, reason="")
ws.ping(data=b"")
ws.pong(data=b"")
ws.closed → bool
ws.remoteAddr → str

// WebSocket 客户端
ws := http.websocket.connect("ws://127.0.0.1:8080/ws")
ws.send("hello")
msg := ws.recv()
ws.close()

// Server-Sent Events 服务端
@server.route("/events")
func sse_handler(req, resp) {
    resp.setHeader("Content-Type", "text/event-stream")
    resp.setHeader("Cache-Control", "no-cache")
    for i in range(10) {
        resp.writeEvent(data=str(i), event="count", id=str(i))
        time.sleep(1)
    }
}

// SSE 事件格式：
// data: ...\n
// event: ...\n（可选）
// id: ...\n（可选）
// retry: ...\n（可选）
// \n（空行=事件结束）

// SSE 客户端（EventSource）
es := http.EventSource("http://server/events")
for event in es {
    print(event.type, event.data)
}
es.close()
```

---

## 实现要点

```c
// WebSocket 握手（HTTP Upgrade）：
// 客户端发送：
//   GET /ws HTTP/1.1
//   Upgrade: websocket
//   Connection: Upgrade
//   Sec-WebSocket-Key: <random 16 bytes base64>
//   Sec-WebSocket-Version: 13
// 服务端响应 101 Switching Protocols：
//   Sec-WebSocket-Accept: SHA1(key + "258EAFA5-...") base64

// WebSocket 帧格式：
// byte0: FIN(1) RSV1-3(3) OPCODE(4)  → opcode: 0=cont 1=text 2=bin 8=close 9=ping 10=pong
// byte1: MASK(1) PAYLOAD_LEN(7)       → 0=0-125 126=uint16 127=uint64
// [Masking Key 4 bytes if MASK=1]
// [Payload XOR mask]
// 客户端发出帧必须掩码，服务端不掩码

// 分片消息（continuation frames）：
// FIN=0 cont frames，最后 FIN=1 合并

// SSE：普通 HTTP 响应流，Transfer-Encoding: chunked（或不关闭连接）
// resp.writeEvent()：格式化并立即 flush 到连接

// EventSource 客户端：
// HTTP GET + 解析 text/event-stream 格式，自动重连（reconnection）

typedef struct MsWsObj {
  MsObject  header;
  MsConnObj* conn;
  bool       isServer;   // 影响掩码行为
  bool       closed;
  uint32_t   closeCode;
} MsWsObj;
```

---

## 验收标准（checklist）

- [ ] WebSocket echo：client.send("hello") → server 收到 → echo 回 → client.recv() = "hello"。
- [ ] WebSocket close 握手（双向关闭帧）。
- [ ] WebSocket ping/pong 维持连接活跃。
- [ ] SSE：客户端依次接收 10 个事件（每秒一个）。
- [ ] WebSocket 握手 SHA-1 计算正确（RFC 6455 §1.3 示例验证）。

---

## 测试用例（.ms）

```ms
import http.server as srv, http.websocket as ws, http.client as cli, time

server := srv.Server(":18889")

// WebSocket echo server
@server.websocket("/ws")
func ws_handler(conn) {
    while true {
        msg := conn.recv()
        if msg == nil { break }
        conn.send("echo: " + msg)
    }
}

// SSE 事件流
@server.get("/events")
func events_handler(req, resp) {
    for i in range(5) {
        resp.writeEvent(data=str(i))
        time.sleep(0.1)
    }
}

go server.serveBackground()
time.sleep(0.1)

// WebSocket 客户端测试
c := ws.connect("ws://127.0.0.1:18889/ws")
c.send("hello")
print(c.recv())   // "echo: hello"
c.send("world")
print(c.recv())   // "echo: world"
c.close()

// SSE 客户端测试
es := http.EventSource("http://127.0.0.1:18889/events")
events := []
for ev in es { events.append(ev.data) }
print(events)  // ["0","1","2","3","4"]

server.close()
```
