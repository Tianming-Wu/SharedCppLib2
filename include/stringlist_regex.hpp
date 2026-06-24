/*
    Regex support for stringlist.
*/

#include <regex>

#include "stringlist.hpp"


namespace scl2 {

using std::stringlist;
using std::wstringlist;

// split by regex_match as delimiter. matched values will be removed.
stringlist regex_chop(const std::string& s, const std::regex& pattern);

// split by regex_match as content. Strings not matched will be removed.
stringlist regex_extract(const std::string& s, const std::regex& pattern);


} // namespace scl2