# http - HTTP Client, Server and File Transfer

+ Name: http
+ Namespace: `network::http`; the file transfer helpers are `network::io`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `network_http` (client and server); the file transfer helpers are in `network_io` |
| Dependencies | `basic`, `network_tcp`; `network_io` also uses `uri` and `fileio` |

To include:

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::network_http)   # client / server
target_link_libraries(target SharedCppLib2::network_io)     # download / upload helpers
```

```cpp
#include <SharedCppLib2/http.hpp>         // request, response, method and status names
#include <SharedCppLib2/httpclient.hpp>   // network::http::client
#include <SharedCppLib2/httpserver.hpp>   // network::http::server
#include <SharedCppLib2/network_io.hpp>   // download, upload, parse_url
```

> [!NOTE]
> The API is synchronous: `get()` / `post()` / `send_request()` return when the whole response
> has been read, or throw when it cannot be. There are no callbacks.

## Description

`network::http` is the HTTP/1.x protocol layer: the request and response types, a client that
speaks them over a `scl2::transport_interface`, and a server that routes requests to handlers.

- `http.hpp` — shared types: `request`, `response`, `http_method`, `http_status`
- `httpclient.hpp` — `network::http::client`
- `httpserver.hpp` — `network::http::server`
- `network_io.hpp` — whole-file `download` / `upload` helpers

HTTP/1.0 and HTTP/1.1 are supported. There is no HTTP/2, no connection reuse and no redirect
following.

## Shared Types

### `request`

```cpp
struct request {
    http_method method;
    std::string path;                       // e.g. "/index.html?q=1"
    std::string http_version;               // e.g. "HTTP/1.1"
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
    http_status status;                     // named code, or UNKNOWN
    int status_code;                        // the code exactly as received; 0 when built locally
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

- `status_code` is authoritative. `status` only names the codes the library knows; anything else
  is `http_status::UNKNOWN` and still keeps its number in `status_code`.
- `serialize()` writes `Content-Length` for a non-empty body unless the header is already there,
  and prefers `status_code` over `status` when it is not 0.
- `deserialize()` throws `network_error` on a malformed message, an invalid `Content-Length`,
  or duplicate `Content-Length` / `Transfer-Encoding` headers.
- `has_complete_headers()` answers whether `\r\n\r\n` has arrived.
- `make_text()` sets `Content-Type: text/plain; charset=utf-8`; `make_json()` sets
  `application/json; charset=utf-8`.

`http_method` names `GET`, `POST`, `PUT`, `DELETE`, `HEAD`, `OPTIONS`, `PATCH`, `TRACE`,
`CONNECT` and `CUSTOM_METHOD`. `method_to_string()` / `string_to_method()` convert between the
enum and the protocol text; `status_to_string()` accepts either the enum or a raw code.

## Client

```cpp
namespace network::http {

class client {
public:
    client();                                        // owns a TCP transport
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

### Connecting

`connect()` resolves the host name through the transport. The default transport is
`network::tcp::client`; passing a TLS transport makes the same client speak HTTPS — that is how
`scl2ext::https_client` is built. `connect()` returns false on failure; the timeout for the
connection is `set_connect_timeout()` (default 10 seconds) and is honoured by transports that
implement it.

Requests sent by this client carry `Connection: close`, and `Host` includes the port when it is
not 80 or 443. **Redirects are not followed**: a 3xx response is returned as it is.

### Timeouts and limits

| Setting | Default | Applies to |
|---------|---------|---------|
| `header_timeout` | 30 s | the status line and the headers |
| `body_timeout` | 30 s | the body |
| `connect_timeout` | 10 s | the connection |
| `max_header_size` | 64 KiB | the response header block |
| `max_body_size` | 64 MiB | the response body |

`set_timeout()` sets the header and the body timeout together and leaves the connection timeout
alone. The header and body limits are measured from the moment the request has been sent.

### Responses

A response body is read by its framing, in this order:

| Framing | Handling |
|---------|---------|
| `Transfer-Encoding: chunked` | decoded, including the terminating chunk and trailer |
| `Content-Length` | read exactly that many bytes |
| neither | read until the server closes the connection |

Responses with no body by definition (`1xx`, `204`, `304`) return an empty `body` without
waiting. Any other `Transfer-Encoding` throws `network_error`.

`get()`, `post()` and `send_request()` throw `network_error` when the client is not connected,
the request cannot be sent, the framing is invalid, or a limit or timeout is exceeded. An HTTP
error status is **not** an exception: the caller checks `response.status_code`.

## Server

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

- `route()` registers a handler for an exact `(method, path)` pair. `path` is compared verbatim,
  query string included; there are no wildcards or path parameters.
- The handler returns only the body. The server answers `200 OK` with
  `Content-Type: text/plain; charset=utf-8`. An exception thrown by the handler becomes
  `500 Internal Server Error` with the exception text in the body.
- A request that matches no route gets `404 Not Found` with `Not Found: <path>`.
- **One request per connection**: the connection is closed after the response, so there is no
  keep-alive and no pipelining.
- `tick()` runs the server: it accepts new connections and answers the requests that are ready.
  Call it repeatedly. It returns `1` when a connection was accepted, `0` when there was nothing
  to accept, and `-1` when the server is not running. The server has no thread of its own.
- Calling `start()` without a port uses the port from the constructor; port 0 is rejected.

Limits and rejections:

| Condition | Answer |
|---------|---------|
| request headers over 64 KiB | `413 Payload Too Large`, connection closed |
| request body over 8 MiB | `413 Payload Too Large`, connection closed |
| `Transfer-Encoding` on the request | `400 Bad Request` — chunked request bodies are not supported |
| unparsable request line or headers | `400 Bad Request` |

## File Transfer Helpers

```cpp
namespace network::io {

struct url_info {
    std::string scheme;     // "http" or "https"
    std::string host;
    uint16_t port;          // defaulted from the scheme
    std::string path;       // includes the query string
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

- `parse_url()` accepts absolute `http://` and `https://` URLs, fills in the default port, and
  throws `std::invalid_argument` for anything else. `path` is `/` when the URL has none.
- `download()` performs a GET and requires exactly `200 OK`; a connection failure or any other
  status throws `network_error`. `download_file()` writes the result with `scl2::writeFile`.
- `upload()` performs a POST and returns the response unchanged — the caller checks its status.
  `upload_file()` reads the file first and throws `std::runtime_error` when it cannot.
- **HTTPS is rejected by these helpers**: the plain library has no TLS. Both `download()` and
  `upload()` throw `std::runtime_error` for an `https://` URL. Use the SCL2Ext OpenSSL
  integration, which supplies a TLS transport to `network::http::client`, or
  `scl2ext::https_client`.

## See Also

- [network](network.md) — addresses, DNS, TCP and UDP
- [bytearray](bytearray.md) — the buffer type the helpers use
