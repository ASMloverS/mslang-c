# P12-T177 stdlib: net

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `net` 模块（对齐 `stdlib/net.md`）：高层网络抽象，封装 TCP/UDP dial/listen，协程原生设计，类 Go 风格接口。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T176 | socket 模块（底层实现） |
| P9-T108 | channel（Conn 读写返回 channel） |

---

## API 清单

```ms
// TCP 连接（客户端）
conn := net.dial("tcp", "example.com:80")
conn := net.dial("tcp4", "127.0.0.1:8080")
conn := net.dial("tcp6", "[::1]:8080")
conn := net.dial("udp", "8.8.8.8:53")
conn := net.dial("unix", "/tmp/sock.sock")

// TCP 监听（服务端）
ln := net.listen("tcp", ":8080")
ln := net.listen("tcp4", "0.0.0.0:8080")

// Listener 接口
conn := ln.accept()    // 阻塞直到新连接（协程让出）
ln.close()
ln.addr() → Addr       // 监听地址

// Conn 接口（TCP/UDP/Unix）
conn.read(n) → bytes    // 读取最多 n 字节
conn.write(data) → int  // 写入
conn.close()
conn.localAddr() → Addr
conn.remoteAddr() → Addr
conn.setDeadline(t)           // 设置绝对超时（datetime 或 float）
conn.setReadDeadline(t)
conn.setWriteDeadline(t)

// Addr 对象
addr.network() → str   // "tcp" "udp" "unix"
addr.string() → str    // "127.0.0.1:8080"

// UDP（PacketConn）
pc := net.listenPacket("udp", ":1234")
data, addr := pc.readFrom(1024)   // 接收数据 + 发送方地址
pc.writeTo(data, addr)             // 发送给特定地址
pc.close()

// 便捷函数
ip := net.lookupHost("example.com") → list[str]   // DNS 解析（协程友好）
ips := net.lookupIP("example.com")  → list[str]
net.joinHostPort(host, port) → str   // "host:port"
host, port := net.splitHostPort("host:port")

// 地址解析
net.parseIP("192.168.1.1") → IP|nil
net.parseCIDR("192.168.1.0/24") → (IP, IPNet)
```

---

## 实现要点

```c
// net.dial 实现：
// 1. 解析 network 字符串（tcp/udp/unix + 4/6/无版本）
// 2. 调用 socket.getaddrinfo（DNS + 地址族选择）
// 3. 创建 socket，设置非阻塞，连接
// 4. 协程等待连接就绪（epoll WRITE 事件）
// 5. 包装为 MsConnObj 返回

typedef struct MsConnObj {
  MsObject  header;
  MsSocketObj* sock;   // 底层 socket
  MsValue   localAddr;
  MsValue   remoteAddr;
  double    deadline;      // -1 = no deadline
  double    readDeadline;
  double    writeDeadline;
} MsConnObj;

// 协程友好读：
// conn.read(n)：尝试 recv，若 EAGAIN → epoll 注册 + yield
// 附 deadline：若到期则取消注册 + 抛 net.deadline_exceeded

// lookupHost：在线程池调用 getaddrinfo（非阻塞），结果通过 channel 返回
// 使用 T112 M:N 调度器的 go 辅助线程池

// net.dial with timeout：
// net.dialTimeout("tcp", "addr:port", timeout=5.0)
// 内部：setDeadline(time.now() + timeout)
```

---

## 验收标准（checklist）

- [ ] `net.dial("tcp", "127.0.0.1:port")` 建立 TCP 连接并读写。
- [ ] `net.listen + accept` 可接受多个并发连接（每个 go 一个协程）。
- [ ] deadline 超时后 read 抛 `net.deadline_exceeded`（类似 i/o timeout）。
- [ ] UDP `listenPacket/readFrom/writeTo` 正确。
- [ ] `net.lookupHost("localhost")` 返回 `["127.0.0.1"]`。
- [ ] 并发 100 连接到 echo server，全部正确响应。

---

## 测试用例（.ms）

```ms
import net

// 并发 echo server
func handleConn(conn) {
    while true {
        data := conn.read(1024)
        if len(data) == 0 { break }
        conn.write(data)
    }
    conn.close()
}

func serve(ln) {
    while true {
        conn := ln.accept()
        go handleConn(conn)
    }
}

ln := net.listen("tcp", ":19998")
go serve(ln)

import time; time.sleep(0.05)

// 多个并发客户端
results := make(chan, 10)
for i in range(10) {
    go func() {
        c := net.dial("tcp", "127.0.0.1:19998")
        c.write(b"ping")
        data := c.read(4)
        c.close()
        results <- data
    }()
}
for i in range(10) {
    print(<-results)  // b"ping"
}

ln.close()
```
