# P12-T176 stdlib: socket

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `socket` 模块（对齐 `stdlib/socket.md`）：低层 BSD socket API 包装，非阻塞 I/O 与调度器集成（协程友好）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 | 协程调度器（非阻塞 I/O 注册） |
| P12-T134 | io 模块 |

---

## API 清单

```ms
// 创建 socket
sock := socket.socket(family=AF_INET, type=SOCK_STREAM, proto=0)
socket.socket(AF_INET6, SOCK_DGRAM)

// 地址族常量
socket.AF_INET, socket.AF_INET6, socket.AF_UNIX

// socket 类型
socket.SOCK_STREAM, socket.SOCK_DGRAM, socket.SOCK_RAW

// 选项
socket.SOL_SOCKET, socket.SO_REUSEADDR, socket.SO_REUSEPORT
socket.IPPROTO_TCP, socket.TCP_NODELAY

// 基础操作（均为协程友好，不阻塞 OS 线程）
sock.connect((host, port))
sock.bind((host, port))
sock.listen(backlog=128)
sock.accept() → (conn, addr)
sock.send(data) → int       // 发送字节数
sock.sendall(data)           // 确保全部发送
sock.recv(bufsize) → bytes
sock.recvfrom(bufsize) → (data, addr)
sock.sendto(data, addr)

sock.close()
sock.shutdown(how)    // SHUT_RD SHUT_WR SHUT_RDWR
sock.setsockopt(level, optname, value)
sock.getsockopt(level, optname) → value
sock.setblocking(flag)   // false = 非阻塞
sock.settimeout(timeout)  // 超时（nil=阻塞）
sock.gettimeout() → float|nil
sock.fileno() → int          // 文件描述符

sock.getpeername() → (host, port)
sock.getsockname() → (host, port)

// 地址工具
socket.gethostname() → str
socket.gethostbyname(hostname) → str   // DNS 解析（阻塞，需异步包装）
socket.getaddrinfo(host, port, family=0, type=0, proto=0, flags=0)
// → [(family, type, proto, canonname, sockaddr), ...]

socket.inet_aton(ip_str) → bytes   // "127.0.0.1" → b'\x7f\x00\x00\x01'
socket.inet_ntoa(packed) → str
socket.htons / socket.ntohs / socket.htonl / socket.ntohl

// 上下文管理器
with socket.socket() as sock:
    sock.connect(("example.com", 80))
```

---

## 实现要点

```c
// MsSocketObj 包裹 int fd（POSIX）或 SOCKET（Windows）
typedef struct MsSocketObj {
  MsObject header;
  int      fd;            // POSIX: socket fd; Windows: SOCKET cast
  int      family, type_, proto;
  bool     closed;
  double   timeout;       // -1.0 = blocking, 0.0 = non-blocking, >0 = timeout
} MsSocketObj;

// 协程友好 I/O：
// 设置 fd 为非阻塞（O_NONBLOCK / FIONBIO）
// connect/accept/recv/send 遇到 EAGAIN/EWOULDBLOCK 时：
//   1. 将 fd + 事件（READ/WRITE）注册到调度器的 I/O 多路复用（epoll/kqueue/IOCP）
//   2. 让出当前协程（yield）
//   3. 调度器 I/O 就绪时唤醒协程继续

// I/O 多路复用（后端选择）：
// Linux：epoll（EPOLLET）
// macOS/BSD：kqueue
// Windows：IOCP（CreateIoCompletionPort）

// epoll 集成（MsIOPoller）：
// gVM.ioPoller：MsHashMap<fd, waiting_coro>
// 调度器主循环：epoll_wait(timeout=next_timer) → 唤醒等待协程

// timeout：使用定时器 + select 组合
// sock.recv() with timeout：同时等 fd 可读 和 时间超时

// gethostbyname：
// 在调度器线程池（单独线程）中调用阻塞 getaddrinfo，结果通过 channel 传回协程
```

---

## 验收标准（checklist）

- [ ] TCP echo server + client：client 发送数据，收到相同数据回显。
- [ ] `socket.gethostbyname("localhost")` → `"127.0.0.1"`。
- [ ] 非阻塞 accept：多个协程连接，不阻塞调度器主线程。
- [ ] `settimeout(1.0)` 后 recv 超时 1 秒抛 `socket.timeout`。
- [ ] `SO_REUSEADDR` 选项防止 TIME_WAIT 重绑定失败。
- [ ] UDP `sendto`/`recvfrom` 正确。

---

## 测试用例（.ms）

```ms
import socket

// TCP echo server（协程）
func run_server() {
    srv := socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 19999))
    srv.listen(5)
    conn, addr := srv.accept()
    data := conn.recv(1024)
    conn.sendall(data)   // echo
    conn.close()
    srv.close()
}

func run_client() {
    sock := socket.socket()
    sock.connect(("127.0.0.1", 19999))
    sock.sendall(b"hello")
    reply := sock.recv(1024)
    print(reply)   // b"hello"
    sock.close()
}

go run_server()
import time; time.sleep(0.05)  // 等 server 启动
run_client()

// UDP
s := socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", 20000))
// ... sendto / recvfrom ...
s.close()
```
