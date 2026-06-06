# P12-T180 stdlib: http.server

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `http.server` 模块（对齐 `stdlib/http.md` 服务端部分）：HTTP/1.1 服务器框架，协程驱动，支持请求路由、中间件、静态文件。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T176 | socket |
| P12-T177 | net |
| P12-T178 | url |
| P9-T107 | go stmt（每连接一协程） |

---

## API 清单

```ms
// 核心服务器
server := http.server.Server(addr=":8080", handler=nil)
server.serve()                  // 阻塞服务（协程调度器运行）
server.serveBackground()        // 后台协程运行
server.close()

// Handler（可调用对象 or 实现 handle(req,resp) 的类实例）
// 函数式路由
srv := http.server.Server(":8080")
srv.route("/",         handler_func)   // GET / 默认
srv.route("/api/user", handler_func, methods=["GET","POST"])

@srv.get("/")
func home(req, resp) {
    resp.write("Hello World")
}

@srv.post("/api/echo")
func echo(req, resp) {
    resp.json(req.json())
}

// Request 对象
req.method     // "GET" "POST" ...
req.path       // "/api/user"
req.query      // dict（URL 查询参数）
req.headers    // dict（大小写不敏感）
req.body       // bytes（已读取完毕）
req.text       // str（body 解码）
req.json()     // 解析 JSON body
req.form()     // 解析 application/x-www-form-urlencoded
req.remoteAddr // "127.0.0.1:54321"

// Response 对象
resp.status = 200
resp.setHeader(name, value)
resp.write(body)       // body 可为 bytes 或 str
resp.json(obj)         // Content-Type: application/json + json.dumps
resp.redirect(url, code=302)
resp.sendFile(path)    // 发送静态文件

// 内置 handler：静态文件服务
static_handler := http.server.FileServer("/static/", "/var/www/static")
srv.route("/static/", static_handler)

// 中间件（装饰器风格）
@srv.middleware
func logger(req, resp, next) {
    print(req.method, req.path)
    next()
}

// BaseHTTPRequestHandler（低层，兼容 CPython 风格）
class MyHandler(http.server.BaseHTTPRequestHandler):
    func do_GET(self) {
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"hello")
    }
http.server.HTTPServer(("", 8000), MyHandler).serve_forever()
```

---

## 实现要点

```c
// 服务器主循环：
// net.listen() → while true: conn = ln.accept() → go handle_conn(conn)
// 每连接一协程（M:N 调度器并发）

// HTTP/1.1 请求解析（per conn 协程）：
// 1. 读请求行（METHOD PATH VERSION\r\n）
// 2. 读头部（K: V\r\n ... \r\n）
// 3. 读 body（按 Content-Length 或 chunked）
// Keep-Alive：连接持久，循环解析多次请求

// 路由表：Trie 或 list[(pattern, methods, handler)]
// 模式匹配：精确匹配 + 前缀匹配 + 参数（/user/:id）
// req.params["id"] 提供 URL 参数

// 静态文件服务：
// 检查 path traversal（防止 ../../ 攻击）
// 支持 Range、Content-Type 猜测（mime 类型表）
// ETags + Last-Modified + 304 Not Modified

// 中间件链：
// srv.use(mw)：mw 包裹 handler，形成洋葱模型
// next() 调用下一个 mw 或最终 handler

// 响应写入：
// 首次 write 前：写状态行 + 头部（延迟写，允许修改状态码）
// 支持 Transfer-Encoding: chunked（流式响应）
```

---

## 验收标准（checklist）

- [ ] 1000 并发请求下服务器不崩溃，正确响应（借助 T114 并发测试）。
- [ ] `@srv.get("/")`路由注册并命中。
- [ ] 静态文件服务正确发送文件（Content-Length 正确，304 缓存）。
- [ ] POST body（JSON/form-data）正确解析。
- [ ] 中间件按注册顺序执行，next() 调用链正确。
- [ ] Keep-Alive：单连接发送多次请求，全部正确响应。

---

## 测试用例（.ms）

```ms
import http.server as srv, http.client as cli, time

// 启动服务器（后台协程）
server := srv.Server(":18888")

@server.get("/hello")
func hello(req, resp) {
    resp.write("hello")
}

@server.post("/echo")
func echo(req, resp) {
    resp.json(req.json())
}

@server.get("/users/:id")
func get_user(req, resp) {
    resp.json({"id": int(req.params["id"]), "name": "Alice"})
}

go server.serveBackground()
time.sleep(0.1)  // 等待服务器启动

// 客户端测试
r1 := cli.get("http://127.0.0.1:18888/hello")
print(r1.status, r1.text)   // 200 hello

import json
r2 := cli.post("http://127.0.0.1:18888/echo",
               data=json.dumps({"msg":"hi"}).encode(),
               headers={"Content-Type":"application/json"})
print(r2.json())             // {"msg":"hi"}

r3 := cli.get("http://127.0.0.1:18888/users/42")
print(r3.json())             // {"id":42,"name":"Alice"}

server.close()
```
