#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <unordered_set>

#include "basics.hpp"
#include "stream.h"
#include "bytearray.hpp"

namespace std {

template<typename CharT>
class basic_stringlist : public vector<std::basic_string<CharT>>
{
public:
    typedef std::basic_string<CharT> string_type;

    using vector<string_type>::size;
    using vector<string_type>::max_size;
    using vector<string_type>::empty;
    using vector<string_type>::clear;
    using vector<string_type>::begin;
    using vector<string_type>::end;
    using vector<string_type>::push_back;
    using vector<string_type>::erase;
    using vector<string_type>::at;
    using vector<string_type>::operator[];

    // class _iterator : public vector<string_type>::iterator {};
    // class _const_iterator : public vector<string_type>::const_iterator {};
    typedef typename vector<string_type>::iterator iterator;
    typedef typename vector<string_type>::const_iterator const_iterator;

    ~basic_stringlist() = default;

    typedef std::pair<size_t,size_t> point;

    static const size_t npos = -1;
    static constexpr point npoint = std::make_pair(npos, npos);

    string_type join(const string_type &i = string_type(1, ' ')) const;
    string_type join(size_t begin, size_t size = -1, const string_type &i = string_type(1, ' ')) const;
    string_type xjoin(const string_type &i = string_type(1, ' '), const CharT binding = '\"') const;

    string_type dbgjoin(string_type i = string_type(1, '\'')) const;

    void append(const basic_stringlist &l);
    void append(const string_type &s);
    
    basic_stringlist subarr(size_t pos, size_t len = 0) const;
    
    void from(int size, CharT** content, int begin = 0, int end = -1);

    /// @brief Return the string at position p, or v if p doesn't exist.
    string_type vat(size_t p, const string_type &v = string_type()) const;

    void remove_empty();
    basic_stringlist unique();

    size_t find(const string_type &s, size_t start = 0) const;
    size_t find_last(const string_type &s) const;
    point find_inside(const string_type &s, size_t start = 0, size_t start_inside = 0) const;
    bool contains(const string_type &s) const;

    /** @brief Execute f() for each of the element.
     * 
     * void f(size_t id, string_type &content)
     * 
     * define normally or use lambda.
     */
    void exec_foreach(function<void(size_t, string_type&)> f);

    /// @brief Build a stringlist from a dynamic list.
    /// @param build param list, use like {"1", "2"}.
    /// @return the built stringlist
    static basic_stringlist from_initializer(initializer_list<string_type> build);

    static basic_stringlist split(const string_type &s, const string_type &delim);
    static basic_stringlist split(const string_type &s, const basic_stringlist &delims);

    /// @brief 
    /// @param s string_type to split
    /// @param delim delimiter
    /// @param begin_bind a series of chars that is treated as combinitions
    /// @param end_bind paired one-by-one to the @c begin_bind , and will be the same as it if left empty
    static basic_stringlist xsplit(const string_type &s, const string_type &delim, const string_type &begin_bind, string_type end_bind = string_type(), bool remove_binding = true);

    /// @brief almost the same as xsplit, while it supports binding characters to be found inside the string
    static basic_stringlist exsplit(const string_type &s, const string_type &delim, const string_type &begin_bind, string_type end_bind = string_type(), bool remove_binding = false, bool strict = false);


    string_type pack() const;
    static basic_stringlist unpack(const string_type &s);

    basic_stringlist& operator=(const basic_stringlist& l);

    size_t all_size();

public:
    explicit basic_stringlist();
    explicit basic_stringlist(int size, CharT** content, int begin = 0, int end = -1);
    basic_stringlist(initializer_list<string_type> build);
    basic_stringlist(const basic_stringlist& l);
    explicit basic_stringlist(const string_type& s);
    explicit basic_stringlist(CharT s);
    explicit basic_stringlist(const CharT* s);
    explicit basic_stringlist(const string_type &s, const string_type &delim);
    explicit basic_stringlist(const string_type &s, const basic_stringlist &delim);
    explicit basic_stringlist(const vector<string_type>& v);
};

// template<typename CharT>
// class stringist_split{
// public:
//     string_type delim;
//     basic_stringlist<typename CharT> &sl;

//     stringist_split(const string& delim, basic_stringlist& sl)
//     : delim(delim), sl(sl) {}
// };

// template<typename CharT>
// inline std::istream& operator>>(std::istream& is, stringist_split<CharT>& sw) {
//     string_type input;
//     is >> input;
//     sw.sl = basic_stringlist<CharT>::split(input, sw.delim);
//     return is;
// }

extern template class basic_stringlist<char>;
extern template class basic_stringlist<wchar_t>;

typedef basic_stringlist<char> stringlist;
typedef basic_stringlist<wchar_t> wstringlist;

}