#include "stringlist.hpp"

#include <stack>

namespace std {

template class basic_stringlist<char>;
template class basic_stringlist<wchar_t>;

template<typename CharT>
basic_stringlist<CharT>::string_type basic_stringlist<CharT>::join(const string_type &i) const {
    string_type re{};
    for(const string_type &s : *this) {
        re += (re.empty()?string_type():i) + s;
    }
    return re;
}

template<typename CharT>
basic_stringlist<CharT>::string_type basic_stringlist<CharT>::join(size_t begin, size_t size, const string_type &i) const {
    string_type re{}; size_t j = begin;
    if(size == -1) size = this->size();
    for(; j < size; j++) {
        const basic_stringlist<CharT>::string_type &s = this->at(j);
        re += (re.empty()?string_type():i) + s;
    }
    return re;
}

template<typename CharT>
basic_stringlist<CharT>::string_type basic_stringlist<CharT>::xjoin(const basic_stringlist<CharT>::string_type &i, const CharT binding) const {
    string_type re{};
    for(const string_type &s : *this) {
        basic_stringlist<CharT>::string_type add = s;
        if(s.contains(i)) add = binding + s + binding;
        re += (re.empty()?string_type():i) + add;
    }
    return re;
}

template<typename CharT>
basic_stringlist<CharT>::string_type basic_stringlist<CharT>::dbgjoin(basic_stringlist<CharT>::string_type i) const {
    string_type re{};
    for(const string_type &s : *this) {
        re += i + s;
    }
    re += i;
    return re;
}

template<typename CharT>
void basic_stringlist<CharT>::append(const basic_stringlist<CharT> &l) {
    for(const basic_stringlist<CharT>::string_type &s : l) {
        push_back(s);
    }
}

template<typename CharT>
void basic_stringlist<CharT>::append(const basic_stringlist<CharT>::string_type &s) {
    push_back(s);
}

template<typename CharT>
std::basic_stringlist<CharT> basic_stringlist<CharT>::subarr(size_t pos, size_t len) const {
    std::basic_stringlist<CharT> result;
    if(len == 0) len = max_size();
    size_t end = std::min(size(), pos+len);
    for(size_t p = pos; p < end; p++) {
        result.push_back((*this)[p]);
    }
    return result;
}

template<typename CharT>
void basic_stringlist<CharT>::from(int size, CharT** content, int begin, int end) {
    clear();
    if(end == -1 || end >= size) end = size;
    for (int t = begin; t < end; t++)
        push_back(content[t]);
}

template<typename CharT>
basic_stringlist<CharT>::string_type basic_stringlist<CharT>::vat(size_t p, const basic_stringlist<CharT>::string_type &v) const {
    return (p<size())?at(p):v;
}

template<typename CharT>
void basic_stringlist<CharT>::remove_empty() {
    basic_stringlist<CharT>::iterator it = begin();
    while(it != end()) {
        if((*it).empty()) {
            it = erase(it);
        } else it++;
    }
}

template<typename CharT>
size_t basic_stringlist<CharT>::find(const string_type &s, size_t start) const {
    size_t i = start;
    for(const string_type& ss : *this) {
        if(ss == s) return i;
        i++;
    }
    return npos;
}

template<typename CharT>
size_t basic_stringlist<CharT>::find_last(const basic_stringlist<CharT>::string_type &s) const {
    size_t i = size();
    for(const string_type& ss : *this) {
        if(ss == s) return i;
        i--;
    }
    return npos;
}

template<typename CharT>
basic_stringlist<CharT> basic_stringlist<CharT>::unique() {
    std::unordered_set<string_type> seen;
    iterator it = begin();
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

template<typename CharT>
basic_stringlist<CharT>::point basic_stringlist<CharT>::find_inside(const basic_stringlist<CharT>::string_type &s, size_t start, size_t start_inside) const {
    size_t i = start, j;
    for(const string_type& ss : *this) {
        if((j = ss.find(s, start_inside)) != string_type::npos) return std::make_pair(1, j);
        i++;
    }
    return npoint;
}

template<typename CharT>
bool basic_stringlist<CharT>::contains(const basic_stringlist<CharT>::string_type &s) const {
    return (find(s) != npos);
}

template<typename CharT>
void basic_stringlist<CharT>::exec_foreach(function<void(size_t,string_type&)> f) {
    size_t i = 0;
    for(string_type& s : *this) {
        f(i++, s);
    }
}

template<typename CharT>
basic_stringlist<CharT> basic_stringlist<CharT>::from_initializer(initializer_list<string_type> build) {
    basic_stringlist<CharT> l;
    for (const string_type& s : build)
        l.push_back(s);
    return l;
}

template<typename CharT>
basic_stringlist<CharT> basic_stringlist<CharT>::split(const basic_stringlist<CharT>::string_type &s, const basic_stringlist<CharT>::string_type &delim) {
    if(s.length() < 3) return basic_stringlist<CharT>(s);
    basic_stringlist<CharT> l;
    size_t pos, lpos = 0, dlen = delim.length();
    while((pos = s.find(delim, lpos)) != string_type::npos) {
        l.push_back(s.substr(lpos, pos - lpos));
        lpos = pos + dlen;
    }
    // if(lpos-=dlen < s.length()-1)
    //     l.push_back(s.substr(lpos+1));
    if(lpos < s.length())
        l.push_back(s.substr(lpos));
    return l;
}

template<typename CharT>
basic_stringlist<CharT> basic_stringlist<CharT>::split(const basic_stringlist<CharT>::string_type &s, const basic_stringlist<CharT> &delims) {
    if(s.length() < 3) return basic_stringlist<CharT>(s);
    basic_stringlist<CharT> l;
    size_t pos, lpos = 0, mpos = s.length(), dlen = 0;
    for(;;) {
        for(const string_type& delim : delims) {
            pos = s.find(delim, lpos);
            if(pos != string_type::npos) {
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

template<typename CharT>
basic_stringlist<CharT> basic_stringlist<CharT>::xsplit(const basic_stringlist<CharT>::string_type &s, const basic_stringlist<CharT>::string_type &delim, const basic_stringlist<CharT>::string_type &begin_bind, basic_stringlist<CharT>::string_type end_bind, bool remove_binding) {
    if(s.length() < 3) return basic_stringlist<CharT>(s);
    if(end_bind.empty()) end_bind = begin_bind;
    else if(begin_bind.length() != end_bind.length()) throw std::runtime_error("xsplit: binding charset length mismatch");

    basic_stringlist<CharT> result = split(s, delim);
    size_t bind_id, tpbind_id;
    bool inside = false;
    auto previous = result.begin();
    
    for(auto it = result.begin(); it != result.end();) {
        if(!inside) {
            if((tpbind_id = begin_bind.find((*it)[0])) != string_type::npos) {
                bind_id = tpbind_id;
                inside = true;
                previous = it;
            }
            ++it;
        } else {
            (*previous) += delim + (*it);
            if((*it)[(*it).length()-1] == end_bind[bind_id]) {
                inside = false;
                if(remove_binding) (*previous) = (*previous).substr(1, (*previous).length()-2); // remove the binding character
            }
            it = result.erase(it);
        }
    }
    return result;
}

template<typename CharT>
basic_stringlist<CharT> basic_stringlist<CharT>::exsplit(const basic_stringlist<CharT>::string_type &s, const basic_stringlist<CharT>::string_type &delim, const basic_stringlist<CharT>::string_type &begin_bind, basic_stringlist<CharT>::string_type end_bind, bool remove_binding, bool strict) {
    if(s.length() < 3) return basic_stringlist<CharT>(s);
    if(end_bind.empty()) end_bind = begin_bind;
    else if(begin_bind.length() != end_bind.length()) throw std::runtime_error("exsplit: binding charset length mismatch");

    basic_stringlist<CharT> result;
    /*stack<size_t> bind_id;*/ size_t tpbind_id;
    vector<size_t> bind_id;

    int depth = 0;
    basic_stringlist<CharT>::string_type buffer;
    size_t idx = 0;
    for(; idx < s.length(); ) {
        if((tpbind_id = begin_bind.find(s[idx])) != string_type::npos) {
            // bind_id.push(tpbind_id);
            bind_id.push_back(tpbind_id);
            depth++;
            if(!remove_binding) buffer += s[idx++];
        // } else if(depth != 0 && s[idx] == end_bind[bind_id.top()]) {
        } else if(depth != 0 && s[idx] == end_bind[bind_id.back()]) {
            // bind_id.pop();
            bind_id.pop_back();
            depth--;
            if(!remove_binding) buffer += s[idx++];
        } else if(depth == 0 && s.substr(idx, delim.length()) == delim) {
            result.push_back(buffer);
            buffer.clear();
            idx += delim.length();
        } else {
            buffer += s[idx++];
        }
    }
    if(!buffer.empty()) result.push_back(buffer);
    return result;
}

template<typename CharT>
basic_stringlist<CharT>::string_type basic_stringlist<CharT>::pack() const {
    return xjoin();
}

template<typename CharT>
basic_stringlist<CharT> basic_stringlist<CharT>::unpack(const basic_stringlist<CharT>::string_type &s) {
    return xsplit(s, string_type(1,' '), string_type(1,'\"'));
}

template<typename CharT>
basic_stringlist<CharT>& basic_stringlist<CharT>::operator=(const basic_stringlist<CharT>& l) {
    vector<string_type>::operator=(l);
    return *this;
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist() : vector<string_type>()
{}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(int size, CharT** content, int begin, int end)
    : vector<string_type>()
{
    from(size, content, begin, end);
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(initializer_list<string_type> build) {
    for (const string_type& s : build)
        push_back(s);
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(const basic_stringlist<CharT>& l) : vector<string_type>(l)
{}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(const string_type& s)
    : vector<string_type>()
{
    push_back(s);
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(CharT s)
    : vector<string_type>()
{
    basic_stringlist<CharT>::string_type str;
    str[0] = s;
    push_back(str);
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(const CharT* s)
    : vector<string_type>()
{
    *this = split(string_type(s), string_type(1, ' '));
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(const basic_stringlist<CharT>::string_type &s, const basic_stringlist<CharT>::string_type &delim)
    : vector<string_type>()
{
    *this = split(s, delim);
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(const basic_stringlist<CharT>::string_type &s, const basic_stringlist<CharT> &delim)
    : vector<string_type>()
{
    *this = split(s, delim);
}

template<typename CharT>
basic_stringlist<CharT>::basic_stringlist(const vector<basic_stringlist<CharT>::string_type>& v)
    : vector<string_type>(v)
{}

template<typename CharT>
size_t basic_stringlist<CharT>::all_size() {
    size_t result = 0;
    for(const basic_stringlist<CharT>::string_type &s : *this) {
        result += s.size();
    }
    return result;
}

} // namespace std