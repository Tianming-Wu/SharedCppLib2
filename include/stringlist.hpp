/* stringlist version 3.22.5

*/

#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <unordered_set>

#include "basics.hpp"
#include "stream.h"
#include "bytearray.h"

namespace std {

class stringlist : public vector<string>
{
public:

    class _iterator : public vector<string>::iterator
    {};

    class _const_iterator : public vector<string>::const_iterator
    {};

    typedef std::pair<size_t,size_t> point;

    static const size_t npos = -1;
    static constexpr point npoint = std::make_pair(npos, npos);

    string join(string i = " ") const;
    string join(size_t begin, size_t size = -1, string i = " ") const;

    string dbgjoin(string i = "'") const;

    void append(const stringlist &l);
    void append(const string &s);
    
    std::stringlist subarr(size_t pos, size_t len = 0);
    void from(int size, char** content, int begin = 0, int end = -1);

    /// @brief Return the string at position p, or v if p doesn't exist.
    string vat(size_t p, const string &v = "") const;

    void remove_empty();
    stringlist unique();

    size_t find(const std::string &s, size_t start = 0) const;
    size_t find_last(const std::string &s) const;
    point find_inside(const std::string &s, size_t start = 0, size_t start_inside = 0) const;
    bool contains(const std::string &s) const;

    /** @brief Execute f() for each of the element.
     * 
     * void f(size_t id, string &content)
     * 
     * define normally or use lambda.
     */
    void exec_foreach(function<void(size_t,string&)> f);

    /// @brief Build a stringlist from a dynamic list.
    /// @param build param list, use like {"1", "2"}.
    /// @return the built stringlist
    static stringlist from_initializer(initializer_list<string> build);

    static stringlist split(const string &s, const string &delim);
    static stringlist split(const string &s, const stringlist &delims);
    static stringlist xsplit(const string &s, const string &delim, const string bindings);

    stringlist& operator=(const stringlist& l);

public:
    explicit stringlist();
    explicit stringlist(int size, char** content, int begin = 0, int end = -1);
    explicit stringlist(initializer_list<string> build);
    stringlist(const stringlist& l);
    explicit stringlist(const string& s);
    explicit stringlist(char s);
    stringlist(const char* s);
    explicit stringlist(const string &s, const string &delim);
    explicit stringlist(const string &s, const stringlist &delim);
    explicit stringlist(const vector<string>& v);
};

inline ostream& operator<<(ostream& os, const ByteArray& ba) {
    if (os.flags() & ios_base::hex) {
        ios_base::fmtflags original_flags = os.flags();
        os << hex << setfill('0');
        for (const auto& b : ba) {
            os << setw(2) << static_cast<int>(b);
        }
        os.flags(original_flags);
    } else {
        os << ba.tostdstring();
    }
    return os;
}

class stringist_split{
public:
    string delim;
    stringlist &sl;

    stringist_split(const string& delim, stringlist& sl)
    : delim(delim), sl(sl) {}
};

inline std::istream& operator>>(std::istream& is, stringist_split& sw) {
    std::string input;
    is >> input;
    sw.sl = stringlist::split(input, sw.delim);
    return is;
}

}