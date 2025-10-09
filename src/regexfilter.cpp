#include "regexfilter.hpp"

namespace rf {

base_list::base_list(const std::stringlist &patterns)
{
    for(const std::string &p : patterns) {
        try {
            m_patterns.push_back(std::regex(p));
        } catch(...) {
            // ignore invalid regex
        }
    }
}

bool base_list::filtered(const std::string &s) const
{
    for (const std::regex &pattern : m_patterns) {
        if (std::regex_match(s, pattern)) {
            return true;
        }
    }
    return false;
}

int base_list::apply(std::stringlist &list, bool reverse) const
{
    int count = 0;
    for (auto it = list.begin(); it != list.end(); ) {
        if (filtered(*it) == reverse) {
            it = list.erase(it);
            ++count;
        } else {
            ++it;
        }
    }
    return count;
}

} // namespace rf