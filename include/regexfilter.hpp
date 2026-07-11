#pragma once
#include "stringlist.hpp"
#include <regex>

namespace rf {

class base_list {
public:
    base_list(const scl2::stringlist& patterns = scl2::stringlist());

    bool filtered(const std::string& s) const;
    int apply(scl2::stringlist& list, bool reverse) const;

    void addPattern(const std::string& pattern);
    void addPatterns(const scl2::stringlist& patterns);
    void clearPatterns();
    void setPatterns(const scl2::stringlist& patterns);

private:
    std::vector<std::regex> m_patterns;
};

class blacklist : public base_list {
public:
    using base_list::base_list;

    inline bool filtered(const std::string& s) const {
        return base_list::filtered(s);
    }
    inline bool apply(scl2::stringlist& list) const {
        return base_list::apply(list, false);
    }
};

class whitelist : public base_list {
public:
    using base_list::base_list;

    inline bool filtered(const std::string& s) const {
        return !base_list::filtered(s);
    }
    inline bool apply(scl2::stringlist& list) const {
        return base_list::apply(list, true);
    }
};


} // namespace rf