# P12-T178 stdlib: url

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `url` 模块（对齐 `stdlib/url.md`）：URL 解析、编码/解码、查询字符串操作。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj |
| P12-T141 | strings.Builder |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-url.md` | §1 模块 API |

---

## API 清单

```ms
// URL 解析
parsed := url.parse("https://user:pass@example.com:8080/path?q=1&r=2#frag")
parsed.scheme    // "https"
parsed.netloc    // "user:pass@example.com:8080"
parsed.path      // "/path"
parsed.params    // "" （;后内容，罕见）
parsed.query     // "q=1&r=2"
parsed.fragment  // "frag"
parsed.username  // "user"
parsed.password  // "pass"
parsed.hostname  // "example.com"
parsed.port      // 8080（int 或 nil）
parsed.geturl()  → str   // 重新组装 URL

// 拆分/组装
url.urlparse(urlstring, scheme="", allow_fragments=true)   // → ParseResult
url.urlunparse((scheme, netloc, path, params, query, fragment)) → str
url.urlsplit(urlstring)   // → SplitResult（无 params）
url.urlunsplit(components)

// URL 编码/解码
url.urlencode(query, doseq=false) → str
// query 可为 dict 或 list[(k,v)]
// urlencode({"a":1, "b":"hello world"}) → "a=1&b=hello+world"

url.urldecode(qs, keep_blank_values=false) → dict[str, list[str]]
// "a=1&b=2&b=3" → {"a":["1"], "b":["2","3"]}
url.parse_qs  // 同 urldecode

url.quote(string, safe="/") → str      // 百分号编码
url.quote_plus(string, safe="") → str // 同 quote，空格→+
url.unquote(string) → str              // 解码 %xx
url.unquote_plus(string) → str         // 同 unquote，+→空格

// URL 拼接（相对 URL 解析）
url.urljoin(base, url, allow_fragments=true) → str
// urljoin("http://a.com/b/c", "../d") → "http://a.com/d"

// 路径规范化
url.urldefrag(url) → (defrag_url, fragment)  // 去除 # 部分
```

---

## 实现要点

```c
// URL 解析（RFC 3986）：
// 正则：^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?
// 或手动 FSM 解析（更快）：
// 1. 找 scheme（第一个 ':'）
// 2. 若有 '//'，解析 authority（user:pass@host:port）
// 3. 路径（到 ? 或 #）
// 4. 查询（到 #）
// 5. 片段（剩余）

// 百分号编码：
// quote(s)：对非 unreserved 字符（ALPHA DIGIT - . _ ~）编码为 %XX
// 大写十六进制，UTF-8 编码

// urljoin（RFC 3986 §5.2.2）：
// 若 url 有 scheme → 返回 url
// 若 url 有 authority → base scheme + url 余下部分
// 若 url 是绝对路径 → base scheme + authority + url
// 否则合并路径（处理 .. 和 .）

// urlencode：
// 对每个 k,v 对调用 quote_plus，用 & 连接
// doseq=true：value 若为 list 则展开多个 k=v

// parse_qs：
// 按 & 分割，再按 = 分割，decode key/value（unquote_plus）
// 收集到 dict[str, list[str]]
```

---

## 验收标准（checklist）

- [ ] `url.parse("https://example.com/path?q=1#frag")` 各字段正确。
- [ ] `url.quote("hello world/path")` → `"hello%20world/path"`（safe="/"）。
- [ ] `url.urlencode({"q": "hello world", "page": "1"})` → `"q=hello+world&page=1"`。
- [ ] `url.urldecode("a=1&b=2&b=3")` → `{"a":["1"],"b":["2","3"]}`。
- [ ] `url.urljoin("http://a.com/b/c", "../d")` → `"http://a.com/d"`。
- [ ] round-trip：`url.urlunparse(url.urlparse(u)) == u`（标准 URL）。

---

## 测试用例（.ms）

```ms
import url

// 解析
p := url.parse("https://alice:secret@api.example.com:443/v1/users?sort=name&page=2#top")
print(p.scheme)    // "https"
print(p.username)  // "alice"
print(p.hostname)  // "api.example.com"
print(p.port)      // 443
print(p.path)      // "/v1/users"
print(p.query)     // "sort=name&page=2"
print(p.fragment)  // "top"

// 编码
print(url.quote("path with spaces/&special"))
// path%20with%20spaces/%26special

print(url.quote_plus("key value"))  // key+value
print(url.unquote_plus("key+value"))  // key value

// 查询字符串
params := url.urldecode("a=1&b=hello+world&b=two")
print(params["b"])  // ["hello world", "two"]

encoded := url.urlencode({"name": "Alice", "tags": ["a", "b"]}, doseq=true)
print(encoded)  // "name=Alice&tags=a&tags=b"

// urljoin
base := "http://www.cwi.nl/%7Eguido/Python.html"
print(url.urljoin(base, "//www.python.org/%7Eguido"))
// http://www.python.org/%7Eguido
print(url.urljoin(base, "FAQ.html"))
// http://www.cwi.nl/%7Eguido/FAQ.html
```
