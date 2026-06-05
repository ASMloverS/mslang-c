# net — TCP/UDP 拨号与监听（低级网络）

```ms
import net
```

## 概述

提供 TCP 和 UDP 连接的建立与监听原语。所有 I/O 操作均为异步，需在 `async func`
中使用 `await`，或通过 goroutine 调用。

- **客户端**：通过 `net.dial` / `net.dialTimeout` 建立连接，返回 `Conn` 对象。
- **服务端**：通过 `net.listen` 绑定端口，返回 `Listener`，循环 `accept()` 接收连接。
- **UDP 数据报**：通过 `net.dialPacket` 返回 `PacketConn`，支持 `readFrom` / `writeTo`。
- **工具函数**：DNS 查询、IP 解析与地址格式化。

## 常量与类型

**Conn**（TCP/UDP 连接对象）

| 方法 | 签名 | 说明 |
|---|---|---|
| `write` | `await conn.write(data) → int` | 写入 `bytes` 数据，返回已写字节数 |
| `read` | `await conn.read(n=-1) → bytes` | 读取最多 `n` 字节；`-1` 读取当前可用数据 |
| `readLine` | `await conn.readLine() → bytes` | 读取直到换行符（含 `\n`） |
| `close` | `conn.close()` | 关闭连接 |
| `localAddr` | `conn.localAddr() → str` | 本端地址，格式 `"host:port"` |
| `remoteAddr` | `conn.remoteAddr() → str` | 对端地址，格式 `"host:port"` |
| `setDeadline` | `conn.setDeadline(seconds)` | 设置读写超时（秒）；`0` 表示不超时 |
| `setReadDeadline` | `conn.setReadDeadline(seconds)` | 仅设置读超时 |
| `setWriteDeadline` | `conn.setWriteDeadline(seconds)` | 仅设置写超时 |

**Listener**（TCP 监听器）

| 方法 | 签名 | 说明 |
|---|---|---|
| `accept` | `await listener.accept() → Conn` | 等待并接受下一个连接 |
| `close` | `listener.close()` | 停止监听，释放端口 |
| `addr` | `listener.addr() → str` | 监听地址，格式 `"host:port"` |

**PacketConn**（UDP 数据报连接）

| 方法 | 签名 | 说明 |
|---|---|---|
| `readFrom` | `await packetConn.readFrom(n) → (bytes, addr)` | 读取最多 `n` 字节，同时返回发送方地址 |
| `writeTo` | `await packetConn.writeTo(data, addr)` | 向指定地址发送数据报 |
| `close` | `packetConn.close()` | 关闭数据报连接 |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `dial` | `await net.dial(network, address) → Conn` | 建立连接 |
| `dialTimeout` | `await net.dialTimeout(network, address, timeout) → Conn` | 带超时的连接 |
| `listen` | `await net.listen(network, address) → Listener` | 绑定并监听 |
| `dialPacket` | `await net.dialPacket(network, address) → PacketConn` | UDP 数据报连接 |
| `lookupHost` | `await net.lookupHost(host) → list[str]` | DNS 正向查询 |
| `lookupAddr` | `await net.lookupAddr(ip) → list[str]` | DNS 反向查询 |
| `parseIp` | `net.parseIp(s) → str\|nil` | 规范化 IP 字符串 |
| `isIp` | `net.isIp(s) → bool` | 判断字符串是否为合法 IP |
| `joinHostPort` | `net.joinHostPort(host, port) → str` | 拼接地址与端口 |
| `splitHostPort` | `net.splitHostPort(hostport) → (host, port)` | 拆分地址与端口 |

## 详细语义

### net.dial

```
await net.dial(network, address) → Conn
```

建立到 `address` 的连接。`network` 可选值：

| 值 | 协议 |
|---|---|
| `"tcp"` | TCP（IPv4 优先） |
| `"tcp4"` | 仅 IPv4 TCP |
| `"tcp6"` | 仅 IPv6 TCP |
| `"udp"` | UDP（IPv4 优先） |
| `"udp4"` | 仅 IPv4 UDP |
| `"udp6"` | 仅 IPv6 UDP |

`address` 格式为 `"host:port"`，IPv6 地址须用方括号括起，例如 `"[::1]:8080"`。

连接失败时抛 `IOError`；主机无法解析时抛 `OSError`。

---

### net.dialTimeout

```
await net.dialTimeout(network, address, timeout) → Conn
```

与 `net.dial` 相同，但若在 `timeout` 秒内未建立连接则抛 `TimeoutError`。
`timeout` 为正浮点数（秒）。

---

### net.listen

```
await net.listen(network, address) → Listener
```

在 `address` 上绑定并开始监听。`network` 同 `net.dial`（通常使用 `"tcp"`）。
`address` 可以省略主机部分（如 `":8080"`）以监听所有网卡。

端口已被占用时抛 `OSError`。

---

### net.dialPacket

```
await net.dialPacket(network, address) → PacketConn
```

创建 UDP 数据报连接。`network` 为 `"udp"`、`"udp4"` 或 `"udp6"`。
与 `net.dial` 不同，`PacketConn` 支持无连接的多目标收发（`readFrom` / `writeTo`）。

---

### net.lookupHost

```
await net.lookupHost(host) → list[str]
```

对 `host` 进行 DNS 正向查询，返回所有解析到的 IP 地址字符串列表。
若主机名不存在，抛 `OSError`。

---

### net.lookupAddr

```
await net.lookupAddr(ip) → list[str]
```

对 `ip` 进行反向 DNS 查询，返回关联的主机名列表。
若无反向记录则返回空列表。

---

### net.parseIp

```
net.parseIp(s) → str | nil
```

将 IP 字符串（IPv4 或 IPv6）规范化为标准文本形式；若 `s` 不是合法 IP 地址则返回 `nil`。

---

### net.isIp

```
net.isIp(s) → bool
```

返回 `s` 是否为合法的 IPv4 或 IPv6 地址字符串。

---

### net.joinHostPort

```
net.joinHostPort(host, port) → str
```

将 `host` 与 `port`（字符串或 int）拼合为 `"host:port"`；若 `host` 为 IPv6 地址则自动加方括号，返回 `"[ipv6]:port"`。

---

### net.splitHostPort

```
net.splitHostPort(hostport) → (host, port)
```

将 `"host:port"` 或 `"[ipv6]:port"` 拆分为 `(host, port)` 元组，`port` 为字符串。
格式无效时抛 `ValueError`。

## 示例

```ms
import net

// 1. TCP 客户端：连接并发送 HTTP 请求
async func fetchRaw(host) {
    conn := await net.dial("tcp", host + ":80")
    await conn.write(bytes("GET / HTTP/1.0\r\nHost: " + host + "\r\n\r\n"))
    data := await conn.read()
    conn.close()
    return str(data)
}

// 2. TCP 服务端：echo server
async func runEchoServer() {
    ln := await net.listen("tcp", ":9000")
    fmt.println("listening on", ln.addr())
    for {
        conn := await ln.accept()
        go handleConn(conn)
    }
}

async func handleConn(conn) {
    for {
        data := await conn.read(4096)
        if len(data) == 0 {
            break
        }
        await conn.write(data)
    }
    conn.close()
}

// 3. 带超时的连接
async func connectWithTimeout() {
    conn := await net.dialTimeout("tcp", "example.com:80", 5.0)
    conn.setDeadline(10.0)
    await conn.write(bytes("ping"))
    resp := await conn.read()
    conn.close()
}

// 4. DNS 查询与工具函数
async func dnsExample() {
    addrs := await net.lookupHost("example.com")
    fmt.println(addrs)  // ["93.184.216.34", ...]

    ip := net.parseIp("192.168.1.1")
    fmt.println(ip)  // "192.168.1.1"

    fmt.println(net.isIp("not-an-ip"))  // false

    addr := net.joinHostPort("192.168.1.1", "8080")
    host, port := net.splitHostPort(addr)
    fmt.println(host, port)  // "192.168.1.1" "8080"
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `IOError` | 连接被拒绝、对端关闭、写入失败等 I/O 错误 |
| `OSError` | DNS 解析失败、端口绑定失败、系统级网络错误 |
| `TimeoutError` | `dialTimeout` 超过指定超时时间；`setDeadline` 到期后的读写操作 |
| `ValueError` | `splitHostPort` 收到格式无效的地址字符串 |
