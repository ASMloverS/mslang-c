# http — HTTP 客户端与服务端

```ms
import http
```

## 概述

提供完整的 HTTP/1.1 和 HTTP/2 客户端与服务端实现。所有网络操作均为异步，
需在 `async func` 中配合 `await` 使用。

- **客户端**：模块级便捷函数（`http.get`、`http.post` 等）适合一次性请求；
  `http.Client` 对象适合复用连接与共享配置（超时、证书验证、默认请求头）。
- **服务端**：`http.Server` 注册路由处理器，通过 `listen_and_serve()` 启动。
  所有 handler 必须声明为 `async func`。

## 常量与类型

**HTTP 方法常量**

| 常量 | 值 |
|---|---|
| `http.MethodGet` | `"GET"` |
| `http.MethodPost` | `"POST"` |
| `http.MethodPut` | `"PUT"` |
| `http.MethodPatch` | `"PATCH"` |
| `http.MethodDelete` | `"DELETE"` |
| `http.MethodHead` | `"HEAD"` |
| `http.MethodOptions` | `"OPTIONS"` |

**HTTP 状态码常量**

| 常量 | 值 | 含义 |
|---|---|---|
| `http.StatusOK` | `200` | 请求成功 |
| `http.StatusCreated` | `201` | 已创建 |
| `http.StatusNoContent` | `204` | 无内容 |
| `http.StatusMovedPermanently` | `301` | 永久重定向 |
| `http.StatusFound` | `302` | 临时重定向 |
| `http.StatusBadRequest` | `400` | 请求有误 |
| `http.StatusUnauthorized` | `401` | 未认证 |
| `http.StatusForbidden` | `403` | 禁止访问 |
| `http.StatusNotFound` | `404` | 资源不存在 |
| `http.StatusInternalServerError` | `500` | 服务器内部错误 |

**客户端 Response 对象**（`http.get` 等函数的返回值）

| 属性/方法 | 类型 | 说明 |
|---|---|---|
| `.status` | `int` | HTTP 状态码 |
| `.status_text` | `str` | 状态描述，如 `"OK"` |
| `.ok` | `bool` | 状态码 200–299 时为 `true` |
| `.headers` | `map[str]str` | 响应头（键名小写） |
| `.body` | `bytes` | 原始响应体 |
| `.text` | `str` | 响应体按 UTF-8 解码的字符串 |
| `.url` | `str` | 最终 URL（重定向后） |
| `.json()` | `any` | 将响应体解析为 JSON；非合法 JSON 时抛 `ValueError` |

**服务端 Request 对象**（handler 第一个参数）

| 属性/方法 | 类型 | 说明 |
|---|---|---|
| `.method` | `str` | HTTP 方法，如 `"GET"` |
| `.url` | `str` | 完整请求 URL |
| `.path` | `str` | URL 路径部分 |
| `.query` | `map[str]str` | 查询参数（同名取最后一个） |
| `.headers` | `map[str]str` | 请求头（键名小写） |
| `.body` | `bytes` | 原始请求体 |
| `.text` | `str` | 请求体按 UTF-8 解码 |
| `.params` | `map[str]str` | 路由路径参数（pattern 使用 `{name}` 时填充） |
| `.remote_addr` | `str` | 客户端地址 `"host:port"` |
| `.json()` | `any` | 解析请求体为 JSON |

**服务端 Response 对象**（handler 第二个参数，可写）

| 方法 | 签名 | 说明 |
|---|---|---|
| `write` | `resp.write(data)` | 写入响应体（`bytes` 或 `str`） |
| `write_json` | `resp.write_json(obj)` | 序列化为 JSON 写入，自动设置 `Content-Type` |
| `set_header` | `resp.set_header(name, value)` | 设置响应头 |
| `set_status` | `resp.set_status(code)` | 设置状态码（默认 `200`） |
| `redirect` | `resp.redirect(url, code=302)` | 重定向到指定 URL |

## 函数签名速查

**模块级客户端函数（均为 async）**

| 函数 | 签名 |
|---|---|
| `get` | `await http.get(url, headers=nil) → Response` |
| `post` | `await http.post(url, data=nil, json=nil, headers=nil) → Response` |
| `put` | `await http.put(url, data=nil, json=nil, headers=nil) → Response` |
| `patch` | `await http.patch(url, data=nil, json=nil, headers=nil) → Response` |
| `delete` | `await http.delete(url, headers=nil) → Response` |
| `head` | `await http.head(url, headers=nil) → Response` |
| `request` | `await http.request(method, url, headers=nil, data=nil, json=nil, timeout=nil, follow_redirects=true, verify_ssl=true) → Response` |

**Client 对象**

| 构造 / 方法 | 签名 |
|---|---|
| 构造 | `http.Client(timeout=30, follow_redirects=true, verify_ssl=true, headers=nil)` |
| 请求 | `await client.get(url, headers=nil) → Response`（同模块级，其余方法类似） |

**Server 对象**

| 构造 / 方法 | 签名 |
|---|---|
| 构造 | `http.Server(addr) → Server` |
| `handle` | `srv.handle(pattern, handler)` |
| `handle_func` | `srv.handle_func(pattern, fn)`（`handle` 的别名） |
| `use` | `srv.use(middleware)` |
| `listen_and_serve` | `await srv.listen_and_serve()` |
| `close` | `srv.close()` |

## 详细语义

### http.request

```
await http.request(method, url, headers=nil, data=nil, json=nil,
                   timeout=nil, follow_redirects=true, verify_ssl=true) → Response
```

所有便捷函数（`get`/`post` 等）均为本函数的封装。

- `data`：`bytes` 或 `str`，作为原始请求体发送。`str` 自动以 UTF-8 编码。
- `json`：任意可序列化对象，自动序列化为 JSON 并设置 `Content-Type: application/json`。
  `data` 与 `json` 互斥，同时传入时抛 `ValueError`。
- `timeout`：超时秒数（浮点）；`nil` 表示不设超时。超时时抛 `TimeoutError`。
- `follow_redirects`：`true` 时自动跟随 3xx 重定向（最多 10 次）。
- `verify_ssl`：`false` 时跳过 TLS 证书验证（仅用于开发/测试）。

---

### http.Client

```
http.Client(timeout=30, follow_redirects=true, verify_ssl=true, headers=nil)
```

创建可复用的 HTTP 客户端。`headers` 为所有请求附加的默认请求头 `map[str]str`。
调用方法时传入的 `headers` 与默认头合并，同名键以调用时的值为准。

---

### srv.handle（路由模式）

```
srv.handle(pattern, handler)
```

`pattern` 支持精确路径（`"/api/users"`）和路径参数（`"/api/users/{id}"`）。
路径参数在 handler 内通过 `req.params["id"]` 访问。

- 未匹配任何 pattern 时，服务器自动返回 `404`。
- handler 签名：`async func handler(req, resp)`，必须为 `async func`。

---

### 中间件（srv.use）

```
srv.use(middleware)
```

中间件签名：`async func middleware(req, resp, next)`，调用 `await next()` 将控制权
传递给下一个中间件或最终 handler。中间件按注册顺序执行。

## 示例

```ms
import http

// 1. 简单 GET 请求
async func example_get() {
    resp := await http.get("https://httpbin.org/get")
    if resp.ok {
        fmt.println(resp.status, resp.text)
    }
}

// 2. POST JSON 数据
async func example_post() {
    payload := {"name": "mslang", "version": 1}
    resp := await http.post("https://httpbin.org/post", json=payload)
    data := resp.json()
    fmt.println(data["json"])
}

// 3. 使用 Client 复用配置
async func example_client() {
    client := http.Client(
        timeout=10,
        headers={"Authorization": "Bearer my-token"},
    )
    resp := await client.get("https://api.example.com/users")
    fmt.println(resp.json())
}

// 4. 完整请求控制
async func example_request() {
    resp := await http.request(
        http.MethodPut,
        "https://api.example.com/items/42",
        json={"price": 9.99},
        timeout=5.0,
        verify_ssl=false,
    )
    fmt.println(resp.status)
}

// 5. REST 服务端
async func start_server() {
    srv := http.Server(":8080")

    srv.use(async func(req, resp, next) {
        fmt.println(req.method, req.path)
        await next()
    })

    srv.handle("/api/hello", async func(req, resp) {
        resp.write_json({"message": "hello from mslang"})
    })

    srv.handle("/api/users/{id}", async func(req, resp) {
        id := req.params["id"]
        if req.method == http.MethodGet {
            resp.write_json({"id": id, "name": "Alice"})
        } else if req.method == http.MethodDelete {
            resp.set_status(http.StatusNoContent)
        } else {
            resp.set_status(http.StatusBadRequest)
            resp.write_json({"error": "method not supported"})
        }
    })

    fmt.println("server running on :8080")
    await srv.listen_and_serve()
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `IOError` | 网络连接失败、连接中断、读写错误 |
| `TimeoutError` | 请求超过 `timeout` 指定的秒数 |
| `ValueError` | `data` 与 `json` 参数同时传入；`resp.json()` 或 `req.json()` 解析非法 JSON |
| `OSError` | DNS 解析失败、TLS 握手失败等系统级网络错误 |
