# url — URL 解析与编码

```ms
import url
```

## 概述

URL 解析、构建与编码工具。参考 Python `urllib.parse` 模块设计。

- **解析**：将 URL 字符串拆分为结构化组件（scheme、host、path、query 等）。
- **构建**：将组件重新组合为合法 URL 字符串；支持相对 URL 解析（`urljoin`）。
- **查询字符串**：编码 / 解码 `key=value&...` 格式，支持多值参数。
- **百分比编码**：`quote` / `unquote` 系列函数处理路径段和查询值中的特殊字符。

## 常量与类型

**ParseResult**（`url.parse()` 返回值）

| 属性 | 类型 | 示例 |
|---|---|---|
| `.scheme` | `str` | `"https"` |
| `.netloc` | `str` | `"user:pass@example.com:8080"` |
| `.path` | `str` | `"/api/v1/items"` |
| `.params` | `str` | 路径参数（`;` 后的部分，通常为空） |
| `.query` | `str` | `"page=1&size=20"` |
| `.fragment` | `str` | `"section-2"` |
| `.username` | `str\|nil` | `"user"` |
| `.password` | `str\|nil` | `"pass"` |
| `.hostname` | `str\|nil` | `"example.com"` |
| `.port` | `int\|nil` | `8080` |

方法：`.geturl() → str` — 将各字段重新拼接为 URL 字符串。

**SplitResult**（`url.urlsplit()` 返回值）

与 `ParseResult` 相同，但不含 `.params`（不从 `.path` 中拆分路径参数）。
属性：`.scheme`、`.netloc`、`.path`、`.query`、`.fragment`；方法：`.geturl()`。

## 函数签名速查

**解析**

| 函数 | 签名 | 说明 |
|---|---|---|
| `parse` | `url.parse(u) → ParseResult` | 解析 URL 为全量结构 |
| `urlsplit` | `url.urlsplit(u) → SplitResult` | 解析 URL，不拆分路径参数 |
| `urldefrag` | `url.urldefrag(u) → (url, fragment)` | 分离 fragment |

**构建**

| 函数 | 签名 | 说明 |
|---|---|---|
| `urljoin` | `url.urljoin(base, ref) → str` | 基于 base 解析相对 URL |
| `urlunparse` | `url.urlunparse(components) → str` | 从 6 元组重组 URL |

**查询字符串**

| 函数 | 签名 | 说明 |
|---|---|---|
| `urlencode` | `url.urlencode(query, doseq=false) → str` | 编码为查询字符串 |
| `urldecode` | `url.urldecode(query) → map[str]str` | 解码查询字符串（单值） |
| `parse_qs` | `url.parse_qs(query, keep_blank_values=false) → map[str]list[str]` | 解码查询字符串（多值） |
| `parse_qsl` | `url.parse_qsl(query, keep_blank_values=false) → list[(str,str)]` | 解码为键值对列表 |

**编码/解码**

| 函数 | 签名 | 说明 |
|---|---|---|
| `quote` | `url.quote(s, safe="/") → str` | 百分比编码 |
| `quote_plus` | `url.quote_plus(s, safe="") → str` | 百分比编码，空格编为 `+` |
| `unquote` | `url.unquote(s) → str` | 解码百分比编码 |
| `unquote_plus` | `url.unquote_plus(s) → str` | 解码百分比编码，`+` 解为空格 |

## 详细语义

### url.parse

```
url.parse(u) → ParseResult
```

将 `u` 解析为 RFC 3986 各组成部分。相对 URL（无 scheme）同样可以解析，
此时 `.scheme` 和 `.netloc` 为空字符串。

格式明显无效时（如包含非法字符）抛 `ValueError`；
语法上合法但逻辑无意义的 URL（如 scheme 含空格）不保证抛出异常。

---

### url.urlsplit

```
url.urlsplit(u) → SplitResult
```

与 `url.parse` 类似，但**不**从 `.path` 中拆分以 `;` 分隔的路径参数段。
适用于 HTTP URL（HTTP 不使用路径参数），性能略优于 `parse`。

---

### url.urldefrag

```
url.urldefrag(u) → (url, fragment)
```

将 `u` 拆分为不含 fragment 的 URL 和 fragment 字符串。
若原 URL 无 fragment，`fragment` 为空字符串。

---

### url.urljoin

```
url.urljoin(base, ref) → str
```

按 RFC 3986 §5.2 将相对引用 `ref` 解析为绝对 URL。常见规则：

- `ref` 为绝对 URL（含 scheme）时直接返回 `ref`。
- `ref` 以 `/` 开头时替换 `base` 的路径部分。
- `ref` 为相对路径时相对于 `base` 的当前目录解析。
- `ref` 为空字符串时返回 `base`（去除 fragment）。

---

### url.urlunparse

```
url.urlunparse(components) → str
```

`components` 为 `(scheme, netloc, path, params, query, fragment)` 六元素列表或元组。
任何为空字符串的字段将被忽略（不产生对应分隔符）。

---

### url.urlencode

```
url.urlencode(query, doseq=false) → str
```

将 `query` 编码为 `application/x-www-form-urlencoded` 格式字符串。

- `query` 为 `map[str]str`：每个键值对编码为 `key=value`，以 `&` 连接。
- `query` 为 `list[(str, str)]`：按列表顺序编码，允许重复键。
- `doseq=true`：`query` 中值为 `list` 时，每个元素单独编码为同名参数。

键和值均使用 `quote_plus` 编码（空格 → `+`）。

---

### url.parse_qs

```
url.parse_qs(query, keep_blank_values=false) → map[str]list[str]
```

将查询字符串解码为 `map[str]list[str]`，相同键的多个值合并为列表。
`keep_blank_values=false`（默认）时，值为空字符串的参数被丢弃。

---

### url.quote

```
url.quote(s, safe="/") → str
```

对 `s` 中非字母数字、非 `safe` 字符集内的字符进行百分比编码（`%XX`）。
`safe` 默认为 `"/"`，使路径字符串可直接传入而不编码斜杠。
如需编码整个路径段，应传入 `safe=""`。

---

### url.quote_plus

```
url.quote_plus(s, safe="") → str
```

与 `quote` 相同，但额外将空格编码为 `+` 而非 `%20`。
适用于 HTML 表单值（`application/x-www-form-urlencoded`）。

## 示例

```ms
import url

// 1. 解析 URL 并访问各组件
result := url.parse("https://user:pass@example.com:8080/api/v1/items?page=2&size=10#top")
fmt.println(result.scheme)    // "https"
fmt.println(result.hostname)  // "example.com"
fmt.println(result.port)      // 8080
fmt.println(result.path)      // "/api/v1/items"
fmt.println(result.query)     // "page=2&size=10"
fmt.println(result.fragment)  // "top"
fmt.println(result.username)  // "user"

// 2. 构建查询字符串
params := {"q": "mslang lang", "page": "1"}
qs := url.urlencode(params)
fmt.println(qs)  // "page=1&q=mslang+lang"（顺序由 map 决定）

// 多值参数
multi := [("tag", "go"), ("tag", "python"), ("tag", "mslang")]
fmt.println(url.urlencode(multi))  // "tag=go&tag=python&tag=mslang"

// 3. 解析多值查询字符串
qs2 := "tag=go&tag=python&tag=mslang&page=1"
all := url.parse_qs(qs2)
fmt.println(all["tag"])  // ["go", "python", "mslang"]
fmt.println(all["page"]) // ["1"]

// 4. 拼接 base + 相对 URL
base := "https://example.com/api/v1/users/"
fmt.println(url.urljoin(base, "42"))           // "https://example.com/api/v1/users/42"
fmt.println(url.urljoin(base, "/health"))      // "https://example.com/health"
fmt.println(url.urljoin(base, "../products/")) // "https://example.com/api/v1/products/"

// 5. 百分比编码路径段
segment := "café & bar/baz"
encoded := url.quote(segment, safe="")
fmt.println(encoded)  // "caf%C3%A9%20%26%20bar%2Fbaz"
fmt.println(url.unquote(encoded))  // "café & bar/baz"

// 6. 重组 URL
parts := url.urlsplit("https://example.com/old/path?x=1")
rebuilt := url.urlunparse(["https", "example.com", "/new/path", "", "x=1", ""])
fmt.println(rebuilt)  // "https://example.com/new/path?x=1"

// 7. 分离 fragment
clean, frag := url.urldefrag("https://example.com/page#section-3")
fmt.println(clean)  // "https://example.com/page"
fmt.println(frag)   // "section-3"
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `url.parse` 收到格式明显无效的 URL 字符串 |
