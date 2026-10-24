# http - HTTP 客户端、服务端与文件传输

+ 名称: http
+ 命名空间: `network::http`；文件传输辅助函数在 `network::io`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `network_http`（客户端与服务端）；文件传输辅助函数在 `network_io` |
| 依赖 | `basic`、`network_tcp`；`network_io` 另用 `uri` 与 `fileio` |

包含方式：

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::network_http)   # 客户端 / 服务端
target_link_libraries(target SharedCppLib2::network_io)     # download / upload 辅助函数
```

```cpp
#include <SharedCppLib2/http.hpp>         // request、response、方法与状态名
#include <SharedCppLib2/httpclient.hpp>   // network::http::client
#include <SharedCppLib2/httpserver.hpp>   // network::http::server
#include <SharedCppLib2/network_io.hpp>   // download、upload、parse_url
```

> [!NOTE]
> API 是同步的：`get()` / `post()` / `send_request()` 在读完整个响应后返回，读不到时抛异常。
> 没有回调。

## 描述

`network::http` 是 HTTP/1.x 协议层：请求与响应类型、在 `scl2::transport_interface` 上收发
它们的客户端，以及把请求路由到处理函数的服务端。

- `http.hpp` — 公共类型：`request`、`response`、`http_method`、`http_status`
- `httpclient.hpp` — `network::http::client`
- `httpserver.hpp` — `network::http::server`
- `network_io.hpp` — 整文件 `download` / `upload` 辅助函数

支持 HTTP/1.0 与 HTTP/1.1。没有 HTTP/2、没有连接复用，也不跟随重定向。

## 公共类型

### `request`

```cpp
struct request {
    http_method method;
    std::string path;                       // 例如 "/index.html?q=1"
    std::string http_version;               // 例如 "HTTP/1.1"
    std::map<std::string, std::string> headers;
    std::string body;

    std::string serialize() const;
    static request deserialize(const std::string& str);
    static bool has_complete_headers(const std::string& str);
    size_t get_content_length() const;
};
```

### `response`

```cpp
struct response {
    http_status status;                     // 具名状态码，未知时为 UNKNOWN
    int status_code;                        // 收到的原始码；本机构造时为 0
    std::string http_version;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string serialize() const;
    static response deserialize(const std::string& str);
    static bool has_complete_headers(const std::string& str);
    static response make_text(http_status status, const std::string& text);
    static response make_json(http_status status, const std::string& json);
};
```

- `status_code` 是权威字段。`status` 只为库认识的码命名；其它码是 `http_status::UNKNOWN`，
  数字仍保存在 `status_code` 里。
- body 非空且没有该头时，`serialize()` 会写出 `Content-Length`；`status_code` 不为 0 时优先
  用它而不是 `status`。
- 消息格式错误、`Content-Length` 无效，或 `Content-Length` / `Transfer-Encoding` 重复时，
  `deserialize()` 抛 `network_error`。
- `has_complete_headers()` 回答 `\r\n\r\n` 是否已经到达。
- `make_text()` 设置 `Content-Type: text/plain; charset=utf-8`；`make_json()` 设置
  `application/json; charset=utf-8`。

`http_method` 包含 `GET`、`POST`、`PUT`、`DELETE`、`HEAD`、`OPTIONS`、`PATCH`、`TRACE`、
`CONNECT` 与 `CUSTOM_METHOD`。`method_to_string()` / `string_to_method()` 在枚举与协议文本
之间转换；`status_to_string()` 接受枚举或原始码。

## 客户端

```cpp
namespace network::http {

class client {
public:
    client();                                        // 自带 TCP transport
    explicit client(std::unique_ptr<scl2::transport_interface> transport);

    bool connect(const std::string& host, uint16_t port = 80);
    void disconnect();
    bool is_connected() const;

    response get(const std::string& path,
                 const std::map<std::string, std::string>& headers = {});
    response post(const std::string& path, const std::string& body,
                  const std::string& content_type = "application/x-www-form-urlencoded",
                  const std::map<std::string, std::string>& headers = {});
    response send_request(const request& req);

    void set_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds get_timeout() const;
    void set_header_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds header_timeout() const;
    void set_body_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds body_timeout() const;
    void set_max_header_size(std::size_t bytes);
    std::size_t max_header_size() const;
    void set_max_body_size(std::size_t bytes);
    std::size_t max_body_size() const;
    void set_connect_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds connect_timeout() const;

    std::string server_host() const;
    uint16_t server_port() const;
};

}
```

### 连接

`connect()` 通过 transport 解析主机名。默认 transport 是 `network::tcp::client`；传入 TLS
transport 后同一个客户端就能说 HTTPS——`scl2ext::https_client` 就是这样构造的。`connect()`
失败时返回 false；连接超时由 `set_connect_timeout()` 设置（默认 10 秒），只有实现了该超时
的 transport 才会遵守。

这个客户端发出的请求带 `Connection: close`；端口不是 80 或 443 时 `Host` 会带上端口。
**不跟随重定向**：3xx 响应原样返回。

### 超时与限制

| 设置 | 默认值 | 作用于 |
|---------|---------|---------|
| `header_timeout` | 30 秒 | 状态行与头部 |
| `body_timeout` | 30 秒 | 响应体 |
| `connect_timeout` | 10 秒 | 连接 |
| `max_header_size` | 64 KiB | 响应头块 |
| `max_body_size` | 64 MiB | 响应体 |

`set_timeout()` 同时设置头部与响应体超时，不动连接超时。头部与响应体限制都从请求发出那一刻
开始计算。

### 响应

响应体按分帧方式读取，顺序如下：

| 分帧 | 处理 |
|---------|---------|
| `Transfer-Encoding: chunked` | 逐块解码，包括末尾的 0 块与 trailer |
| `Content-Length` | 精确读取该长度 |
| 两者都没有 | 读到服务端关闭连接为止 |

按定义没有响应体的状态（`1xx`、`204`、`304`）直接返回空 `body`，不会等待。其它
`Transfer-Encoding` 抛 `network_error`。

客户端未连接、请求发不出去、分帧无效、超过限制或超时时，`get()` / `post()` /
`send_request()` 抛 `network_error`。HTTP 错误状态**不是**异常：调用方检查
`response.status_code`。

## 服务端

```cpp
namespace network::http {

using route_handler = std::function<std::string(const request&)>;

class server {
public:
    server();
    server(uint16_t port);
    server(network_address address, uint16_t port);

    void start();
    void start(uint16_t port);
    void stop();

    void route(http_method method, const std::string& path, route_handler handler);
    int tick();

    uint16_t port() const;
    network_address address() const;
};

}
```

- `route()` 为精确的 `(method, path)` 对注册处理函数。`path` 逐字比较，包含查询串；没有通配符
  或路径参数。
- 处理函数只返回响应体。服务端以 `200 OK` 和 `Content-Type: text/plain; charset=utf-8` 作答。
  处理函数抛异常时变成 `500 Internal Server Error`，异常文本放在响应体里。
- 没有匹配路由的请求得到 `404 Not Found`，响应体为 `Not Found: <path>`。
- **一个连接一个请求**：响应发出后连接即关闭，因此没有 keep-alive，也没有流水线。
- `tick()` 驱动服务端：接受新连接并处理已就绪的请求。需要反复调用。接受连接时返回 `1`，
  没有可接受连接时返回 `0`，服务端未运行时返回 `-1`。服务端没有自己的线程。
- 调用不带端口的 `start()` 时使用构造函数里的端口；端口 0 会被拒绝。

限制与拒绝：

| 情况 | 回答 |
|---------|---------|
| 请求头超过 64 KiB | `413 Payload Too Large`，关闭连接 |
| 请求体超过 8 MiB | `413 Payload Too Large`，关闭连接 |
| 请求带 `Transfer-Encoding` | `400 Bad Request`——不支持 chunked 请求体 |
| 请求行或头部无法解析 | `400 Bad Request` |

## 文件传输辅助函数

```cpp
namespace network::io {

struct url_info {
    std::string scheme;     // "http" 或 "https"
    std::string host;
    uint16_t port;          // 按 scheme 填好的默认值
    std::string path;       // 含查询串
};

url_info parse_url(const std::string& url);

scl2::bytearray download(const std::string& url);
void download_file(const std::string& url, const std::filesystem::path& local_path);

network::http::response upload(const std::string& url,
                               const scl2::bytearray& data,
                               const std::string& content_type = "application/octet-stream");
network::http::response upload_file(const std::string& url,
                                    const std::filesystem::path& local_path,
                                    const std::string& content_type = "application/octet-stream");

}
```

- `parse_url()` 接受绝对的 `http://` 与 `https://` URL，补齐默认端口；其它情况抛
  `std::invalid_argument`。URL 没有路径时 `path` 为 `/`。
- `download()` 发 GET，且只接受 `200 OK`；连接失败或其它状态抛 `network_error`。
  `download_file()` 用 `scl2::writeFile` 写出结果。
- `upload()` 发 POST，原样返回响应——状态由调用方检查。`upload_file()` 先读文件，读不到时
  抛 `std::runtime_error`。
- **这些辅助函数拒绝 HTTPS**：纯库没有 TLS。`download()` 与 `upload()` 遇到 `https://` URL
  都抛 `std::runtime_error`。请使用 SCL2Ext 的 OpenSSL 集成（给
  `network::http::client` 提供 TLS transport），或 `scl2ext::https_client`。

## 相关模块

- [network](network.md) — 地址、DNS、TCP 与 UDP
- [ws](ws.md) — WebSocket 客户端
- [bytearray](bytearray.md) — 辅助函数使用的缓冲类型
