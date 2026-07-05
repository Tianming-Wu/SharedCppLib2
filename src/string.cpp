#include "string.hpp"

#include "stringlist_regex.hpp"

namespace scl2 {

template class basic_string<char>;
template class basic_string<wchar_t>;

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::split(CharT delim)
{ return scl2::basic_stringlist<CharT>::split(*this, delim); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::split(const string_type &delim)
{ return scl2::basic_stringlist<CharT>::split(*this, delim); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::split(const scl2::basic_stringlist<CharT> &delims)
{ return scl2::basic_stringlist<CharT>::split(*this, delims); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::xsplit(const string_type &delim, const string_type &begin_bind, string_type end_bind, bool remove_binding)
{ return scl2::basic_stringlist<CharT>::xsplit(*this, delim, begin_bind, end_bind, remove_binding); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::exsplit(const string_type &delim, const string_type &begin_bind, string_type end_bind, bool remove_binding, bool strict)
{ return scl2::basic_stringlist<CharT>::exsplit(*this, delim, begin_bind, end_bind, remove_binding, strict); }

// Regex-based methods commented out: regex_chop/extract are char-only,
// but templates must compile for both char and wchar_t.
// template <typename CharT>
// scl2::basic_stringlist<CharT> basic_string<CharT>::split(const std::regex &regex_delim)
// { return regex_chop(*this, regex_delim); }
// template <typename CharT>
// scl2::basic_stringlist<CharT> basic_string<CharT>::extract(const std::regex &regex_pattern)
// { return regex_extract(*this, regex_pattern); }

} // namespace scl2