# socket — 低级 BSD 套接字

```ms
import socket
```

## 概述

低级 BSD socket API，适合需要精细控制套接字行为的场景，如原始套接字、自定义协议、
精细的套接字选项调整等。参考 Python `socket` 模块设计。

**常规 TCP/UDP 应优先使用 `net` 模块**；仅在需要直接操控套接字参数时才使用本模块。

`socket.socket()` 创建套接字对象，通过方法链完成绑定、监听、连接、收发数据。
套接字支持 `with` 语句，退出时自动调用 `close()`。

## 常量与类型

**地址族（Address Family）**

| 常量 | 说明 |
|---|---|
| `socket.AF_UNSPEC` | 未指定（任意地址族，用于 getAddrInfo） |
| `socket.AF_INET` | IPv4 |
| `socket.AF_INET6` | IPv6 |
| `socket.AF_UNIX` | Unix 域套接字 |

**套接字类型**

| 常量 | 说明 |
|---|---|
| `socket.SOCK_STREAM` | 面向连接（TCP） |
| `socket.SOCK_DGRAM` | 数据报（UDP） |
| `socket.SOCK_RAW` | 原始套接字 |

**套接字选项**

| 常量 | 层级 | 说明 |
|---|---|---|
| `socket.SOL_SOCKET` | — | 套接字层选项层级 |
| `socket.SO_REUSEADDR` | `SOL_SOCKET` | 允许复用本地地址 |
| `socket.SO_REUSEPORT` | `SOL_SOCKET` | 允许多个套接字绑定同一端口 |
| `socket.SO_KEEPALIVE` | `SOL_SOCKET` | 启用 TCP keepalive |
| `socket.IPPROTO_TCP` | — | TCP 协议层选项层级 |
| `socket.IPPROTO_UDP` | — | UDP 协议层选项层级 |
| `socket.TCP_NODELAY` | `IPPROTO_TCP` | 禁用 Nagle 算法 |

**关闭方向常量**

| 常量 | 说明 |
|---|---|
| `socket.SHUT_RD` | 关闭接收方向 |
| `socket.SHUT_WR` | 关闭发送方向 |
| `socket.SHUT_RDWR` | 同时关闭收发 |

## 函数签名速查

**构造函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `socket` | `socket.socket(family=AF_INET, type=SOCK_STREAM, proto=0) → Socket` | 创建套接字 |

**Socket 对象方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `bind` | `s.bind(address)` | 绑定地址 |
| `listen` | `s.listen(backlog=5)` | 开始监听 |
| `accept` | `s.accept() → (Socket, address)` | 接受连接 |
| `connect` | `s.connect(address)` | 连接到服务端 |
| `connectEx` | `s.connectEx(address) → int` | 连接，返回错误码 |
| `send` | `s.send(data) → int` | 发送数据，返回已发字节数 |
| `sendAll` | `s.sendAll(data)` | 循环发送直到全部发完 |
| `recv` | `s.recv(bufSize) → bytes` | 接收最多 `bufSize` 字节 |
| `recvFrom` | `s.recvFrom(bufSize) → (bytes, address)` | 接收数据及来源地址 |
| `sendTo` | `s.sendTo(data, address) → int` | 发送到指定地址（无连接模式） |
| `shutdown` | `s.shutdown(how)` | 半关闭连接 |
| `close` | `s.close()` | 关闭套接字 |
| `setSockOpt` | `s.setSockOpt(level, optname, value)` | 设置套接字选项 |
| `getSockOpt` | `s.getSockOpt(level, optname) → int\|bytes` | 读取套接字选项 |
| `getSockName` | `s.getSockName() → address` | 本端地址 |
| `getPeerName` | `s.getPeerName() → address` | 对端地址 |
| `setBlocking` | `s.setBlocking(flag)` | 设置阻塞模式 |
| `setTimeout` | `s.setTimeout(seconds)` | 设置超时 |
| `getTimeout` | `s.getTimeout() → float\|nil` | 获取当前超时 |
| `makeFile` | `s.makeFile(mode="r") → file` | 将套接字包装为文件对象 |

**工具函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `getHostName` | `socket.getHostName() → str` | 本机主机名 |
| `getHostByName` | `socket.getHostByName(hostname) → str` | DNS 查询（同步，可能阻塞） |
| `getFqdn` | `socket.getFqdn(name=nil) → str` | 获取完全限定域名 |
| `getAddrInfo` | `socket.getAddrInfo(host, port, family=0, type=0, proto=0, flags=0) → list` | 地址信息查询 |
| `getServByName` | `socket.getServByName(service, proto=nil) → int` | 服务名转端口号 |
| `htons` | `socket.htons(n) → int` | 主机序 → 网络序（16 位） |
| `ntohs` | `socket.ntohs(n) → int` | 网络序 → 主机序（16 位） |
| `htonl` | `socket.htonl(n) → int` | 主机序 → 网络序（32 位） |
| `ntohl` | `socket.ntohl(n) → int` | 网络序 → 主机序（32 位） |
| `inetAton` | `socket.inetAton(ip) → bytes` | IPv4 字符串 → 4 字节 |
| `inetNtoa` | `socket.inetNtoa(b) → str` | 4 字节 → IPv4 字符串 |
| `setDefaultTimeout` | `socket.setDefaultTimeout(timeout)` | 设置全局默认超时 |

## 详细语义

### socket.socket

```
socket.socket(family=AF_INET, type=SOCK_STREAM, proto=0) → Socket
```

创建新套接字。`proto` 通常为 `0`（由系统根据 `family` 和 `type` 选择默认协议）；
创建原始套接字时需显式指定，例如 `socket.IPPROTO_ICMP`。

---

### s.bind

```
s.bind(address)
```

将套接字绑定到本地地址。`address` 类型取决于地址族：
- `AF_INET` / `AF_INET6`：`(host, port)` 元组，`host` 为空字符串表示监听所有网卡。
- `AF_UNIX`：字符串路径。

---

### s.accept

```
s.accept() → (Socket, address)
```

从监听队列中取出一个连接，返回 `(clientSocket, clientAddress)` 元组。
在阻塞模式下等待连接到来；非阻塞模式下若队列为空抛 `BlockingIOError`。

---

### s.connectEx

```
s.connectEx(address) → int
```

与 `connect` 功能相同，但连接失败时返回错误码整数而非抛出异常。
成功时返回 `0`，失败时返回系统 errno 值。

---

### s.setTimeout

```
s.setTimeout(seconds)
```

- `nil`：阻塞模式（默认）。
- `0`：非阻塞模式，等同于 `setBlocking(false)`。
- 正浮点数：超时模式，操作超时时抛 `TimeoutError`。

---

### s.makeFile

```
s.makeFile(mode="r") → file
```

将套接字包装为类文件对象，支持 `read()`、`readLine()`、`write()` 等接口。
`mode` 可为 `"r"`（文本读）、`"rb"`（二进制读）、`"wb"`（二进制写）等。
注意：关闭文件对象不等于关闭底层套接字，需单独调用 `s.close()`。

---

### socket.getAddrInfo

```
socket.getAddrInfo(host, port, family=0, type=0, proto=0, flags=0) → list
```

返回可用于 `bind`/`connect` 的地址信息列表，每项为
`(family, type, proto, canonname, sockaddr)` 元组，是创建可移植套接字代码的推荐方式。

## 示例

```ms
import socket

// 1. TCP 服务端（单线程 echo）
s := socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setSockOpt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("", 9000))
s.listen(5)
fmt.println("listening on", s.getSockName())
conn, addr := s.accept()
fmt.println("connected:", addr)
data := conn.recv(1024)
conn.sendAll(data)
conn.close()
s.close()

// 2. TCP 客户端
with socket.socket() as c {
    c.connect(("127.0.0.1", 9000))
    c.sendAll(bytes("hello"))
    resp := c.recv(1024)
    fmt.println(str(resp))
}

// 3. UDP 数据报收发
srv := socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
srv.bind(("", 9001))
data, addr := srv.recvFrom(4096)
srv.sendTo(data, addr)  // echo back
srv.close()

// 4. 禁用 Nagle 算法，降低 TCP 延迟
c2 := socket.socket()
c2.setSockOpt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
c2.connect(("example.com", 80))

// 5. 使用 getAddrInfo 构造可移植连接
infos := socket.getAddrInfo("example.com", 80, socket.AF_UNSPEC, socket.SOCK_STREAM)
family, stype, proto, _, addr := infos[0]
c3 := socket.socket(family, stype, proto)
c3.connect(addr)
c3.close()

// 6. 字节序转换与 IP 编解码
n := socket.htons(8080)
fmt.println(socket.ntohs(n))  // 8080
b := socket.inetAton("192.168.1.1")
fmt.println(socket.inetNtoa(b))  // "192.168.1.1"
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `OSError` | 绑定失败、连接被拒绝、DNS 查询失败等系统级错误（含所有 `socket.error` 子类） |
| `TimeoutError` | `setTimeout` 设置的超时期满后操作仍未完成 |
| `BlockingIOError` | 非阻塞模式下操作会阻塞（`accept`/`recv` 无数据可用） |
| `ValueError` | `inetAton` 收到无效 IPv4 字符串；`inetNtoa` 收到长度非 4 的 bytes |
