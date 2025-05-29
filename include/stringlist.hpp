/* stringlist version 3.22.5

*/

#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <unordered_set>

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

    string join(string i = " ") const {
        string re = "";
        for(const string &s : *this) {
            re += (re.empty()?"":i) + s;
        }
        return re;
    }

    string join(int begin, int size = -1, string i = " ") const {
        string re = ""; int j = begin;
        if(size == -1) size = this->size();
        for(; j < size; j++) {
            const string &s = this->at(j);
            re += (re.empty()?"":i) + s;
        }
        return re;
    }

    string dbgjoin(string i = "'") const {
        string re = "";
        for(const string &s : *this) {
            re += i + s;
        }
        re += i;
        return re;
    }

    void append(const stringlist &l) {
        for(const string &s : l) {
            push_back(s);
        }
    }

    void append(const string &s) {
        push_back(s);
    }
    
    void from(int size, char** content, int begin = 0, int end = -1) {
        clear();
        if(end == -1 || end >= size) end = size;
        for (int t = begin; t < end; t++)
            push_back(content[t]);
    }

    /// @brief Return the string at position p, or v if p doesn't exist.
    string vat(size_t p, const string &v = "") const {
        return (p<size())?at(p):v;
    }

    void remove_empty() {
        stringlist::iterator it = begin();
        while(it != end()) {
            if((*it).empty()) {
                it = erase(it);
            } else it++;
        }
    }

    size_t find(const std::string &s, size_t start = 0) const {
        size_t i = start;
        for(const string& ss : *this) {
            if(ss == s) return i;
            i++;
        }
        return npos;
    }

    size_t find_last(const std::string &s) const {
        size_t i = size();
        for(const string& ss : *this) {
            if(ss == s) return i;
            i--;
        }
        return npos;
    }

    stringlist unique() {
        std::unordered_set<std::string> seen;
        stringlist::iterator it = begin();
        while (it != end()) {
            if (seen.find(*it) != seen.end()) {
                it = erase(it);
            } else {
                seen.insert(*it);
                ++it;
            }
        }
        return *this;
    }

    point find_inside(const std::string &s, size_t start = 0, size_t start_inside = 0) {
        size_t i = start, j;
        for(const string& ss : *this) {
            if((j = ss.find(s, start_inside)) != std::string::npos) return std::make_pair(1, j);
            i++;
        }
        return npoint;
    }

    bool contains(const std::string &s) const {
        return (find(s) != npos);
    }

    /** @brief Execute f() for each of the element.
     * 
     * void f(size_t id, string &content)
     * 
     * define normally or use lambda.
     */
    void exec_foreach(function<void(size_t,string&)> f) {
        size_t i = 0;
        for(string& s : *this) {
            f(i++, s);
        }
    }

    /// @brief Build a stringlist from a dynamic list.
    /// @param build param list, use like {"1", "2"}.
    /// @return the built stringlist
    static stringlist from_initializer(initializer_list<string> build) {
        stringlist l;
        for (const string& s : build)
            l.push_back(s);
        return l;
    }

    static stringlist split(const string &s, const string &delim) {
        if(s.length() < 3) return stringlist(s);
        stringlist l;
        size_t pos, lpos = 0, dlen = delim.length();
        while((pos = s.find(delim, lpos)) != string::npos) {
            l.push_back(s.substr(lpos, pos - lpos));
            lpos = pos + dlen;
        }
        // if(lpos-=dlen < s.length()-1)
        //     l.push_back(s.substr(lpos+1));
        if(lpos < s.length())
            l.push_back(s.substr(lpos));
        return l;
    }

    static stringlist split(const string &s, const stringlist &delims) {
        if(s.length() < 3) return stringlist(s);
        stringlist l;
        size_t pos, lpos = 0, mpos = s.length(), dlen = 0;
        for(;;) {
            for(const string& delim : delims) {
                pos = s.find(delim, lpos);
                if(pos != string::npos) {
                    if(pos < mpos) {
                        mpos = pos;
                        dlen = delim.length();
                    }
                }
            }
            if(mpos == s.length()) break;
            l.push_back(s.substr(lpos, mpos - lpos));
            lpos = mpos + dlen;
            mpos = s.length();
        }
        if(lpos < s.length())
            l.push_back(s.substr(lpos));
        return l;
    }

    stringlist& operator=(const stringlist& l) {
        vector<string>::operator=(l);
        return *this;
    }


public:
    explicit stringlist() : vector<string>()
    {}

    explicit stringlist(int size, char** content, int begin = 0, int end = -1)
        : vector<string>()
    {
        from(size, content, begin, end);
    }

    explicit stringlist(initializer_list<string> build) {
        for (const string& s : build)
            push_back(s);
    }

    stringlist(const stringlist& l) : vector<string>(l)
    {}

    explicit stringlist(const string& s)
        : vector<string>()
    {
        push_back(s);
    }

    explicit stringlist(char s)
        : vector<string>()
    {
        std::string str;
        str[0] = s;
        push_back(str);
    }

    explicit stringlist(const char* s)
        : vector<string>()
    {
        push_back(s);
    }

    explicit stringlist(const string &s, const string &delim)
        : vector<string>()
    {
        *this = split(s, delim);
    }

    explicit stringlist(const string &s, const stringlist &delim)
        : vector<string>()
    {
        *this = split(s, delim);
    }

    explicit stringlist(const vector<string>& v) : vector<string>(v)
    {}
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