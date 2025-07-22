#include "stringlist.hpp"

namespace std {

string stringlist::join(const string &i) const {
    string re = "";
    for(const string &s : *this) {
        re += (re.empty()?"":i) + s;
    }
    return re;
}

string stringlist::join(size_t begin, size_t size, const string &i) const {
    string re = ""; size_t j = begin;
    if(size == -1) size = this->size();
    for(; j < size; j++) {
        const string &s = this->at(j);
        re += (re.empty()?"":i) + s;
    }
    return re;
}

string stringlist::xjoin(const string &i, const char binding) const {
    string re = "";
    for(const string &s : *this) {
        std::string add = s;
        if(s.contains(i)) add = binding + s + binding;
        re += (re.empty()?"":i) + add;
    }
    return re;
}

string stringlist::dbgjoin(string i) const {
    string re = "";
    for(const string &s : *this) {
        re += i + s;
    }
    re += i;
    return re;
}

void stringlist::append(const stringlist &l) {
    for(const string &s : l) {
        push_back(s);
    }
}

void stringlist::append(const string &s) {
    push_back(s);
}

std::stringlist stringlist::subarr(size_t pos, size_t len) const {
    std::stringlist result;
    if(len == 0) len = max_size();
    size_t end = std::min(size(), pos+len);
    for(size_t p = pos; p < end; p++) {
        result.push_back((*this)[p]);
    }
    return result;
}

void stringlist::from(int size, char** content, int begin, int end) {
    clear();
    if(end == -1 || end >= size) end = size;
    for (int t = begin; t < end; t++)
        push_back(content[t]);
}

string stringlist::vat(size_t p, const string &v) const {
    return (p<size())?at(p):v;
}

void stringlist::remove_empty() {
    stringlist::iterator it = begin();
    while(it != end()) {
        if((*it).empty()) {
            it = erase(it);
        } else it++;
    }
}

size_t stringlist::find(const std::string &s, size_t start) const {
    size_t i = start;
    for(const string& ss : *this) {
        if(ss == s) return i;
        i++;
    }
    return npos;
}

size_t stringlist::find_last(const std::string &s) const {
    size_t i = size();
    for(const string& ss : *this) {
        if(ss == s) return i;
        i--;
    }
    return npos;
}

stringlist stringlist::unique() {
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

stringlist::point stringlist::find_inside(const std::string &s, size_t start, size_t start_inside) const {
    size_t i = start, j;
    for(const string& ss : *this) {
        if((j = ss.find(s, start_inside)) != std::string::npos) return std::make_pair(1, j);
        i++;
    }
    return npoint;
}

bool stringlist::contains(const std::string &s) const {
    return (find(s) != npos);
}

void stringlist::exec_foreach(function<void(size_t,string&)> f) {
    size_t i = 0;
    for(string& s : *this) {
        f(i++, s);
    }
}

stringlist stringlist::from_initializer(initializer_list<string> build) {
    stringlist l;
    for (const string& s : build)
        l.push_back(s);
    return l;
}

stringlist stringlist::split(const string &s, const string &delim) {
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

stringlist stringlist::split(const string &s, const stringlist &delims) {
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

stringlist stringlist::xsplit(const string &s, const string &delim, const string &bindings) {
    if(s.length() < 3) return stringlist(s);
    stringlist result = split(s, delim);
    bool inside = false;
    auto previous = result.begin();
    for(auto it = result.begin(); it != result.end();) {
        if(!inside) {
            if(charmatch((*it)[0], bindings)) {
                inside = true;
                previous = it;
            }
            ++it;
        } else {
            (*previous) += delim + (*it);
            if(charmatch((*it)[(*it).length()-1], bindings)) {
                inside = false;
                (*previous) = (*previous).substr(1, (*previous).length()-2); // remove the binding character
            }
            it = result.erase(it);
        }
    }
    return result;
}

stringlist stringlist::exsplit(const string &s, const string &delim, const string &begin_bind, string end_bind)
{
    if(s.length() < 3) return stringlist(s);
    if(end_bind.empty()) end_bind = begin_bind;

    stringlist result = split(s, delim);
    size_t bind_id, tpbind_id;
    bool inside = false;
    auto previous = result.begin();
    
    for(auto it = result.begin(); it != result.end();) {
        if(!inside) {
            if((tpbind_id = begin_bind.find((*it)[0])) != std::string::npos) {
                bind_id = tpbind_id;
                inside = true;
                previous = it;
            }
            ++it;
        } else {
            (*previous) += delim + (*it);
            if((*it).find(end_bind[bind_id]) != std::string::npos) {
                inside = false;
                (*previous) = (*previous).substr(1, (*previous).length()-2); // remove the binding character
            }
            it = result.erase(it);
        }
    }
    return result;
}

string stringlist::pack() const {
    return xjoin();
}

stringlist stringlist::unpack(const std::string &s) {
    return xsplit(s, " ", "\"");
}

stringlist& stringlist::operator=(const stringlist& l) {
    vector<string>::operator=(l);
    return *this;
}

stringlist::stringlist() : vector<string>()
{}

stringlist::stringlist(int size, char** content, int begin, int end)
    : vector<string>()
{
    from(size, content, begin, end);
}

stringlist::stringlist(initializer_list<string> build) {
    for (const string& s : build)
        push_back(s);
}

stringlist::stringlist(const stringlist& l) : vector<string>(l)
{}

stringlist::stringlist(const string& s)
    : vector<string>()
{
    push_back(s);
}

stringlist::stringlist(char s)
    : vector<string>()
{
    std::string str;
    str[0] = s;
    push_back(str);
}

stringlist::stringlist(const char* s)
    : vector<string>()
{
    *this = split(string(s), string(" "));
}

stringlist::stringlist(const string &s, const string &delim)
    : vector<string>()
{
    *this = split(s, delim);
}

stringlist::stringlist(const string &s, const stringlist &delim)
    : vector<string>()
{
    *this = split(s, delim);
}

stringlist::stringlist(const vector<string>& v) : vector<string>(v)
{}

} // namespace std