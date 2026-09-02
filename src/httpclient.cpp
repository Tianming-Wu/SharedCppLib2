#include "httpclient.hpp"

#include <cctype>
#include <chrono>
#include <thread>

namespace network::http {

namespace {

bool iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i]))
            != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

// 不区分大小写地查找响应头（HTTP 头名大小写不敏感）。
const std::string* find_header(const std::map<std::string, std::string>& headers,
                               const std::string& name)
{
    for (const auto& [key, value] : headers) {
        if (iequals(key, name)) return &value;
    }
    return nullptr;
}

bool is_chunked(const std::map<std::string, std::string>& headers)
{
    const std::string* te = find_header(headers, "Transfer-Encoding");
    return te && te->find("chunked") != std::string::npos;
}

// 增量 chunked 解码器（状态机）。网络数据可能被切成任意小块到达，因此必须
// 区分「等大小行 / 等块数据 / 等块尾 CRLF / 等 trailer」各阶段——数据不足时
// 停在当前阶段返回 false，绝不把块数据误当大小行重新解析。
struct chunked_decoder {
    enum class st { SizeLine, Data, DataCRLF, Trailer };

    std::string out;                       // 已解码的 body
    size_t pos = 0;                        // raw 中已安全消费的位置
    bool done = false;
    st state = st::SizeLine;
    unsigned long long data_left = 0;      // 当前块剩余待收字节

    // 用 raw 中自 pos 起的新数据推进解析；完整消费（0 块 + trailer 空行）时
    // 返回 true，数据不足时返回 false（可追加数据后再次调用）。
    bool feed(const std::string& raw)
    {
        while (!done) {
            switch (state) {
            case st::SizeLine: {
                const size_t le = raw.find("\r\n", pos);
                if (le == std::string::npos) return false;   // 等完整的大小行
                std::string line = raw.substr(pos, le - pos);
                pos = le + 2;
                const size_t semi = line.find(';');          // 去掉 chunk extensions
                if (semi != std::string::npos) line.resize(semi);
                const size_t fs = line.find_first_not_of(" \t");
                if (fs == std::string::npos)
                    throw network_error("Invalid chunked encoding");
                line = line.substr(fs);
                unsigned long long sz = 0;
                std::string::size_type p = 0;
                try {
                    sz = std::stoull(line, &p, 16);
                } catch (...) {
                    throw network_error("Invalid chunk size");
                }
                if (p == 0) throw network_error("Invalid chunk size");
                if (sz == 0) { state = st::Trailer; break; }
                data_left = sz;
                state = st::Data;
                break;
            }
            case st::Data: {
                if (data_left == 0) { state = st::DataCRLF; break; }
                const size_t avail = raw.size() - pos;
                if (avail == 0) return false;                // 等块数据
                const size_t take = avail < data_left
                                    ? avail : static_cast<size_t>(data_left);
                out.append(raw, pos, take);
                pos += take;
                data_left -= take;
                if (data_left > 0) return false;             // 块数据还没到齐
                state = st::DataCRLF;
                break;
            }
            case st::DataCRLF: {
                if (raw.size() - pos < 2) return false;      // 等块尾 CRLF
                if (raw[pos] != '\r' || raw[pos + 1] != '\n')
                    throw network_error("Invalid chunked encoding");
                pos += 2;
                state = st::SizeLine;
                break;
            }
            case st::Trailer: {
                // 0 长度块之后：逐行消费 trailer，直到空行
                const size_t te = raw.find("\r\n", pos);
                if (te == std::string::npos) return false;   // 等 trailer
                if (te == pos) { pos += 2; done = true; return true; }
                pos = te + 2;                                // 已消费完整一行 trailer
                break;
            }
            }
        }
        return done;
    }
};

} // namespace

client::client()
    : m_transport(std::make_unique<tcp::client>())
{
}

client::client(std::unique_ptr<scl2::transport_interface> transport)
    : m_transport(std::move(transport))
{
}

client::~client()
{
    disconnect();
}

bool client::connect(const std::string& host, uint16_t port)
{
    m_host = host;
    m_port = port;
    return m_transport->connect(host, port);
}

void client::disconnect()
{
    m_transport->disconnect();
}

bool client::is_connected() const
{
    return m_transport->is_connected();
}

response client::get(const std::string& path, 
                    const std::map<std::string, std::string>& headers)
{
    request req = build_request(http_method::GET, path, "", headers);
    return send_request(req);
}

response client::post(const std::string& path, 
                     const std::string& body,
                     const std::string& content_type,
                     const std::map<std::string, std::string>& headers)
{
    auto full_headers = headers;
    full_headers["Content-Type"] = content_type;
    
    request req = build_request(http_method::POST, path, body, full_headers);
    return send_request(req);
}

response client::send_request(const request& req)
{
    if (!is_connected()) {
        throw network_error("Not connected to server");
    }
    
    // Serialize and send request
    std::string req_str = req.serialize();
    scl2::bytearray req_data(req_str);
    
    size_t sent = m_transport->write(req_data);
    if (sent == 0) {
        throw network_error("Failed to send request");
    }
    
    // Receive response
    return receive_response();
}

void client::set_timeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

std::chrono::milliseconds client::get_timeout() const
{
    return m_timeout;
}

std::string client::server_host() const
{
    return m_host;
}

uint16_t client::server_port() const
{
    return m_port;
}

request client::build_request(http_method method, const std::string& path,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers)
{
    request req;
    req.method = method;
    req.path = path;
    req.http_version = "HTTP/1.1";
    req.body = body;
    
    // Set Host header (required for HTTP/1.1)
    req.headers["Host"] = m_host;
    if (m_port != 80 && m_port != 443) {
        req.headers["Host"] += ":" + std::to_string(m_port);
    }
    
    // Set Connection header
    req.headers["Connection"] = "close";
    
    // Copy additional headers
    for (const auto& [key, value] : headers) {
        req.headers[key] = value;
    }
    
    return req;
}

response client::receive_response()
{
    std::string buffer;
    auto start_time = std::chrono::steady_clock::now();
    
    // Read until we have complete headers
    while (!response::has_complete_headers(buffer)) {
        if (m_transport->readyRead()) {
            auto data = m_transport->readAll();
            if (!data.empty()) {
                buffer += data.toStdString();
            }
        }
        
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > m_timeout) {
            throw network_error("Response timeout");
        }
        
        // Small delay to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 只解析头部；body 按下面的分帧方式单独读取
    response resp;
    try {
        resp = response::deserialize(buffer);
    } catch (...) {
        throw network_error("Failed to parse response headers");
    }

    // 规范上无 body 的响应：1xx / 204 / 304（读了会等到连接关闭或超时）
    const int code = static_cast<int>(resp.status);
    if ((code >= 100 && code < 200) || code == 204 || code == 304) {
        resp.body.clear();
        return resp;
    }

    const size_t header_end = buffer.find("\r\n\r\n");
    std::string raw_body = buffer.substr(header_end + 4);

    // body 的分帧方式：
    //   - Transfer-Encoding: chunked —— 逐块解码（末尾 0 块与 trailer 一并消费）
    //   - Content-Length           —— 精确读取该长度字节
    //   - 其它（HTTP/1.0 或无 CL）   —— 读到连接关闭为止（请求已带 Connection: close）
    if (is_chunked(resp.headers)) {
        chunked_decoder dec;
        while (!dec.feed(raw_body)) {
            if (m_transport->readyRead()) {
                auto data = m_transport->readAll();
                if (!data.empty()) raw_body += data.toStdString();
            }
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > m_timeout) throw network_error("Response body timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        resp.body = std::move(dec.out);
        return resp;
    }

    if (const std::string* cl = find_header(resp.headers, "Content-Length")) {
        size_t content_length = 0;
        try {
            content_length = static_cast<size_t>(std::stoull(*cl));
        } catch (...) {
            content_length = 0;
        }
        while (raw_body.size() < content_length) {
            if (m_transport->readyRead()) {
                auto data = m_transport->readAll();
                if (!data.empty()) raw_body += data.toStdString();
            }
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > m_timeout) throw network_error("Response body timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        resp.body = raw_body.substr(0, content_length);
        return resp;
    }

    // 无分帧信息：读到 EOF（服务器按 Connection: close 关闭连接）
    while (true) {
        if (m_transport->readyRead()) {
            auto data = m_transport->readAll();
            if (data.empty()) break;   // 对端关闭
            raw_body += data.toStdString();
            continue;
        }
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > m_timeout) throw network_error("Response body timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    resp.body = std::move(raw_body);
    return resp;
}

} // namespace network::http
