/*
    Uniform Resource Identifier (URI) handling

    URI handling designed for SharedCppLib2.

*/

#pragma once

#include <string>
#include <optional>

typedef std::optional<std::string> opt_string;

class uri
{
public:
    uri() = default;
    uri(const std::string& url);
    virtual ~uri() = default;

    // parsing
    static uri fromString(const std::string& str);
    bool parse(const std::string& uri_string);

    // serialization
    std::string toString() const;
    std::string string() const;  // alias for toString()

    // getters
    std::string getScheme() const;
    opt_string getUserInfo() const;
    std::string getHost() const;
    std::optional<uint16_t> getPort() const;
    opt_string getPath() const;
    opt_string getQuery() const;
    opt_string getFragment() const;

    // setters
    void setScheme(const std::string& scheme);
    void setUserInfo(const std::optional<std::string>& userinfo);
    void setHost(const std::string& host);
    void setPort(std::optional<uint16_t> port);
    void setPath(const std::optional<std::string>& path);
    void setQuery(const std::optional<std::string>& query);
    void setFragment(const std::optional<std::string>& fragment);

    // validation
    virtual bool isValid() const;
    bool isAbsolute() const;  // has scheme

    // compare operators
    bool operator==(const uri& other) const;
    bool operator!=(const uri& other) const;

    // note: this is totally arbitrary, just to have some ordering
    // for something like map or set. (not suggested)
    bool operator<(const uri& other) const;

protected:
    std::string m_scheme;
    opt_string m_userinfo;
    std::string m_host; // host must be present, though it can be empty
    std::optional<uint16_t> m_port;
    opt_string m_path;
    opt_string m_query;
    opt_string m_fragment;

    // helper functions for parsing
    static bool isValidScheme(const std::string& scheme);
    static bool isValidHost(const std::string& host);
    static bool isValidPort(uint16_t port);
};


/*
    URL (Uniform Resource Locator) handling
    URLs are hierarchical URIs with scheme, host, path, etc.
*/
class url : public uri
{
public:
    url() = default;
    url(const std::string& url_string);

    bool isValid() const override;

    std::string normalize() const;

    uint16_t getEffectivePort() const;

    uri resolve(const uri& relative) const;

    // get default port for common schemes
    static std::optional<uint16_t> getDefaultPort(const std::string& scheme);
};


/*
    URN (Uniform Resource Name) handling
    URNs are opaque URIs (not hierarchical)
    Format: urn:nid:nss
*/
class urn : public uri
{
    // not fully implemented yet
    // TODO: implement URN parsing and validation according to RFC 8141
};