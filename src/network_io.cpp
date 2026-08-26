#include "network_io.hpp"

#include "basics.hpp"      // scl2::lower
#include "httpclient.hpp"  // network::http::client
#include "uri.hpp"         // ::uri (global-namespace URI parser)
#include "fileio.hpp"      // scl2::writeFile
#include "network.hpp"     // network::network_error

#include <fstream>
#include <stdexcept>

namespace network::io {

url_info parse_url(const std::string& url)
{
    ::uri u(url);
    if (!u.isAbsolute())
        throw std::invalid_argument("network::io: not an absolute URL: " + url);

    url_info info;
    info.scheme = scl2::lower(u.getScheme());
    if (info.scheme != "http" && info.scheme != "https")
        throw std::invalid_argument("network::io: unsupported URL scheme: " + info.scheme);

    info.host = u.getHost();
    if (info.host.empty())
        throw std::invalid_argument("network::io: URL has no host: " + url);

    info.port = u.getPort().value_or(info.scheme == "https" ? 443 : 80);

    info.path = u.getPath().value_or("/");
    if (auto q = u.getQuery(); q && !q->empty())
        info.path += "?" + *q;
    return info;
}

scl2::bytearray download(const std::string& url)
{
    const url_info info = parse_url(url);
    if (info.scheme == "https")
        throw std::runtime_error(
            "network::io: https:// download requires the SCL2Ext openssl "
            "integration (a scl2ext::tls_client transport)");

    network::http::client c;
    if (!c.connect(info.host, info.port))
        throw network::network_error("network::io: failed to connect: " + url);

    const network::http::response resp = c.get(info.path);
    if (resp.status != network::http::http_status::OK)
        throw network::network_error(
            "network::io: download failed with HTTP "
            + std::to_string(static_cast<int>(resp.status)) + ": " + url);
    return scl2::bytearray(resp.body);
}

void download_file(const std::string& url, const std::filesystem::path& local_path)
{
    scl2::writeFile(local_path, download(url));
}

network::http::response upload(const std::string& url,
                               const scl2::bytearray& data,
                               const std::string& content_type)
{
    const url_info info = parse_url(url);
    if (info.scheme == "https")
        throw std::runtime_error(
            "network::io: https:// upload requires the SCL2Ext openssl "
            "integration (a scl2ext::tls_client transport)");

    const std::string body(reinterpret_cast<const char*>(data.data()), data.size());
    network::http::client c;
    if (!c.connect(info.host, info.port))
        throw network::network_error("network::io: failed to connect: " + url);
    return c.post(info.path, body, content_type);
}

network::http::response upload_file(const std::string& url,
                                    const std::filesystem::path& local_path,
                                    const std::string& content_type)
{
    std::ifstream ifs(local_path, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("network::io: cannot open file: " + local_path.string());
    scl2::bytearray data;
    if (!data.readAllFromStream(ifs))
        throw std::runtime_error("network::io: failed to read file: " + local_path.string());
    return upload(url, data, content_type);
}

} // namespace network::io
