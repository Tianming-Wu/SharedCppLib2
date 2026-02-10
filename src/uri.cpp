#include "uri.hpp"
#include <regex>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <map>

uri::uri(const std::string &url)
{
    parse(url);
}

bool uri::parse(const std::string& uri_string)
{
    if(uri_string.empty()) {
        return false;
    }

    // prepare regex for validation
    // scheme ":" hier-part [ "?" query ] [ "#" fragment ]
    // hier-part = "//" authority path-abempty / path-absolute / path-rootless / path-empty
    std::regex uri_regex(R"(^([a-zA-Z][a-zA-Z0-9+.-]*):\/\/(([^:@\/?#]+)(:([^:@\/?#]*))?@)?([^:\/?#]+)(:([0-9]+))?(\/[^?#]*)?(\?[^#]*)?(#.*)?$)");

    std::smatch match;
    if (!std::regex_match(uri_string, match, uri_regex)) {
        return false;
    }

    m_scheme   = match[1];
    m_userinfo = match[3].matched ? match[3].str() : opt_string();
    m_host     = match[6];
    if (match[8].matched) {
        try {
            uint16_t port_val = static_cast<uint16_t>(std::stoi(match[8]));
            m_port = port_val;
        } catch (...) {
            return false;
        }
    }
    m_path     = match[9].matched ? match[9].str() : opt_string();
    m_query    = match[10].matched ? match[10].str() : opt_string();
    m_fragment = match[11].matched ? match[11].str() : opt_string();

    return true;
}

uri uri::fromString(const std::string &str)
{
    uri u;
    u.parse(str);
    return u;
}

std::string uri::toString() const
{
    std::string result;

    // scheme
    if (!m_scheme.empty()) result += m_scheme + "://";

    // userinfo
    if (m_userinfo.has_value() && !m_userinfo->empty()) result += m_userinfo.value() + "@";
    // host
    result += m_host;
    // port
    if (m_port.has_value()) result += ":" + std::to_string(m_port.value());
    // path
    if (m_path.has_value()) result += m_path.value();
    // query
    if (m_query.has_value()) result += "?" + m_query.value();
    // fragment
    if (m_fragment.has_value()) result += "#" + m_fragment.value();

    return result;
}

std::string uri::string() const
{
    return toString();
}

// getters
std::string uri::getScheme() const
{
    return m_scheme;
}

opt_string uri::getUserInfo() const
{
    return m_userinfo;
}

std::string uri::getHost() const
{
    return m_host;
}

std::optional<uint16_t> uri::getPort() const
{
    return m_port;
}

opt_string uri::getPath() const
{
    return m_path;
}

opt_string uri::getQuery() const
{
    return m_query;
}

opt_string uri::getFragment() const
{
    return m_fragment;
}

// setters
void uri::setScheme(const std::string& scheme)
{
    if(!isValidScheme(scheme)) {
        throw std::invalid_argument("Invalid scheme: " + scheme);
    }
    m_scheme = scheme;
}

void uri::setUserInfo(const std::optional<std::string>& userinfo)
{
    m_userinfo = userinfo;
}

void uri::setHost(const std::string& host)
{
    if(!isValidHost(host)) {
        throw std::invalid_argument("Invalid host: " + host);
    }
    m_host = host;
}

void uri::setPort(std::optional<uint16_t> port)
{
    m_port = port;
}

void uri::setPath(const std::optional<std::string>& path)
{
    m_path = path;
}

void uri::setQuery(const std::optional<std::string>& query)
{
    m_query = query;
}

void uri::setFragment(const std::optional<std::string>& fragment)
{
    m_fragment = fragment;
}

// validation
bool uri::isValid() const
{
    // basic check: must have scheme
    if(m_scheme.empty() || !isValidScheme(m_scheme)) {
        return false;
    }
    
    // host should be valid (may be empty for some schemes like file://)
    if(!isValidHost(m_host)) {
        return false;
    }

    return true;
}

bool uri::isAbsolute() const
{
    return !m_scheme.empty();
}

bool uri::operator==(const uri &other) const
{
    return m_scheme == other.m_scheme &&
           m_userinfo == other.m_userinfo &&
           m_host == other.m_host &&
           m_port == other.m_port &&
           m_path == other.m_path &&
           m_query == other.m_query &&
           m_fragment == other.m_fragment;
}

bool uri::operator!=(const uri &other) const
{
    return !(*this == other);
}

bool uri::operator<(const uri &other) const
{
    if(m_scheme != other.m_scheme) return m_scheme < other.m_scheme;
    if(m_userinfo != other.m_userinfo) return m_userinfo < other.m_userinfo;
    if(m_host != other.m_host) return m_host < other.m_host;
    if(m_port != other.m_port) return m_port < other.m_port;
    if(m_path != other.m_path) return m_path < other.m_path;
    if(m_query != other.m_query) return m_query < other.m_query;
    return m_fragment < other.m_fragment;
}

// helper functions for parsing
bool uri::isValidScheme(const std::string& scheme)
{
    if(scheme.empty()) return false;
    
    // scheme must start with a letter
    if(!std::isalpha(scheme[0])) return false;
    
    // followed by letter, digit, '+', '-', or '.'
    for(size_t i = 1; i < scheme.length(); ++i) {
        char c = scheme[i];
        if(!std::isalnum(c) && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }
    
    return true;
}

bool uri::isValidHost(const std::string& host)
{
    // host can be empty, domain name, IPv4, or IPv6
    // simplified validation: just check it's not overly long
    // TODO: add proper IPv4/IPv6/domain validation
    return host.length() <= 255;
}

bool uri::isValidPort(uint16_t port)
{
    // all uint16_t values are valid ports (0-65535)
    return true;
}

// URL class implementation
url::url(const std::string &url_string)
{
    parse(url_string);
}

bool url::isValid() const
{
    // URL must have a valid scheme
    if(!uri::isValid()) {
        return false;
    }
    
    // URL must be hierarchical (have ://)
    if(m_scheme.empty()) {
        return false;
    }
    
    // Common URL schemes validation
    std::string lower_scheme = m_scheme;
    std::transform(lower_scheme.begin(), lower_scheme.end(), lower_scheme.begin(), ::tolower);
    
    if(lower_scheme == "http" || lower_scheme == "https") {
        // HTTP(S) URLs must have a host
        return !m_host.empty();
    } else if(lower_scheme == "file") {
        // file:// can have empty host (local file)
        return true;
    } else if(lower_scheme == "ftp" || lower_scheme == "ftps") {
        // FTP URLs should have a host
        return !m_host.empty();
    }
    
    // For other schemes, just do basic validation
    return true;
}

std::string url::normalize() const
{
    std::string normalized = toString();
    // Simple normalization: lowercase scheme and host
    size_t scheme_end = normalized.find("://");
    if(scheme_end != std::string::npos) {
        std::transform(normalized.begin(), normalized.begin() + scheme_end, normalized.begin(), ::tolower);
        
        size_t host_start = scheme_end + 3;
        size_t host_end = normalized.find_first_of("/?:#", host_start);
        if(host_end == std::string::npos) {
            host_end = normalized.length();
        }
        std::transform(normalized.begin() + host_start, normalized.begin() + host_end, normalized.begin() + host_start, ::tolower);
    }
    return normalized;
}

uint16_t url::getEffectivePort() const
{
    if(m_port.has_value()) {
        return m_port.value();
    } else {
        auto port = getDefaultPort(m_scheme);
        if(port.has_value()) {
            return port.value();
        } else {
            throw std::runtime_error("No effective port for scheme: " + m_scheme);
        }
    }
}

uri url::resolve(const uri &relative) const
{
    return uri();
}

std::optional<uint16_t> url::getDefaultPort(const std::string& scheme)
{
    std::string lower_scheme = scheme;
    std::transform(lower_scheme.begin(), lower_scheme.end(), lower_scheme.begin(), ::tolower);

    static const std::map<std::string, std::optional<uint16_t>> default_ports = {
        {"http", 80},
        {"https", 443},
        {"ftp", 21},
        {"ftps", 990},
        {"ssh", 22},
        {"telnet", 23},
        {"smtp", 25},
        {"pop3", 110},
        {"imap", 143},
        {"ldap", 389},
        {"file", std::nullopt}
    };
    
    auto it = default_ports.find(lower_scheme);
    if(it != default_ports.end()) {
        return it->second;
    }
    return std::nullopt;
}