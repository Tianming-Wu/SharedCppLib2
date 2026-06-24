#include "stringlist_regex.hpp"

namespace scl2 {

stringlist regex_chop(const std::string &s, const std::regex &pattern)
{
    stringlist result;
    std::sregex_token_iterator iter(s.begin(), s.end(), pattern, -1);
    std::sregex_token_iterator end;
    for (; iter != end; ++iter) {
        result.push_back(*iter);
    }
    return result;
}

stringlist regex_extract(const std::string &s, const std::regex &pattern)
{
    stringlist result;
    std::sregex_iterator iter(s.begin(), s.end(), pattern);
    std::sregex_iterator end;
    for (; iter != end; ++iter) {
        result.push_back(iter->str());
    }
    return result;
}

} // namespace scl2
