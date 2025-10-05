#pragma once
#include "stringlist.hpp"
#include <regex>

namespace rf {

class base_list {
public:
    base_list(const std::stringlist& patterns = std::stringlist());

    bool filtered(const std::string& s) const;
    int apply(std::stringlist& list, bool reverse) const;

private:
    std::vector<std::regex> m_patterns;
};

class blacklist : public base_list {
public:
    using base_list::base_list;

    inline bool filtered(const std::string& s) const {
        return base_list::filtered(s);
    }
    inline bool apply(std::stringlist& list) const {
        return base_list::apply(list, false);
    }
};

class whitelist : public base_list {
public:
    using base_list::base_list;

    inline bool filtered(const std::string& s) const {
        return !base_list::filtered(s);
    }
    inline bool apply(std::stringlist& list) const {
        return base_list::apply(list, true);
    }
};


} // namespace rf