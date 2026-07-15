#include "xstring.hpp"
#include "platform.hpp"

namespace {

// --- u8string <-> string (reinterpret, both UTF-8) ---

std::u8string str_to_u8str(const std::string& s)
{
    return std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size());
}

std::string u8str_to_str(const std::u8string& s)
{
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}

} // anonymous namespace

namespace scl2 {

// ============================================================
//  Type queries
// ============================================================

xstring::stype xstring::type() const
{
    return static_cast<stype>(value.index());
}

bool xstring::empty() const
{
    if (type() == stype::null) return true;
    return _query<bool>([](const auto& s) { return s.empty(); });
}

size_t xstring::size() const
{
    return _query<size_t>([](const auto& s) { return s.size(); });
}

// ============================================================
//  Typed access (copies)
//  Conversion chain: string <-> wstring (direct),
//  everything else routes through string as intermediate.
// ============================================================

scl2::string xstring::a() const
{
    // Direct hit
    if (auto* s = std::get_if<scl2::string>(&value))
        return *s;
    if (type() == stype::null)
        return scl2::string();

    // Convert from other types
    if (auto* s = std::get_if<scl2::wstring>(&value))
        return scl2::string(wstr_to_str(*s));
    if (auto* s = std::get_if<std::u8string>(&value))
        return scl2::string(u8str_to_str(*s));
    if (auto* s = std::get_if<std::u16string>(&value))
        return scl2::string(); // TODO: UTF-16 -> UTF-8
    if (auto* s = std::get_if<std::u32string>(&value))
        return scl2::string(); // TODO: UTF-32 -> UTF-8

    return scl2::string();
}

scl2::wstring xstring::w() const
{
    // Direct hit
    if (auto* s = std::get_if<scl2::wstring>(&value))
        return *s;
    if (type() == stype::null)
        return scl2::wstring();

    // Convert from other types — all go through a() as intermediate
    return str_to_wstr(a());
}

std::u8string xstring::u8() const
{
    // Direct hit
    if (auto* s = std::get_if<std::u8string>(&value))
        return *s;
    if (type() == stype::null)
        return std::u8string();

    // Convert from other types — go through a() (UTF-8 string) as intermediate
    return str_to_u8str(a());
}

std::u16string xstring::u16() const
{
    // Direct hit
    if (auto* s = std::get_if<std::u16string>(&value))
        return *s;
    if (type() == stype::null)
        return std::u16string();

    // TODO: UTF-8 -> UTF-16 conversion via a()
    return std::u16string();
}

std::u32string xstring::u32() const
{
    // Direct hit
    if (auto* s = std::get_if<std::u32string>(&value))
        return *s;
    if (type() == stype::null)
        return std::u32string();

    // TODO: UTF-8 -> UTF-32 conversion via a()
    return std::u32string();
}

// ============================================================
//  Typed access (views)
// ============================================================

std::string_view xstring::a_view() const
{
    if (auto* s = std::get_if<scl2::string>(&value))
        return *s;
    return {};
}

std::wstring_view xstring::w_view() const
{
    if (auto* s = std::get_if<scl2::wstring>(&value))
        return *s;
    return {};
}

std::u8string_view xstring::u8_view() const
{
    if (auto* s = std::get_if<std::u8string>(&value))
        return *s;
    return {};
}

std::u16string_view xstring::u16_view() const
{
    if (auto* s = std::get_if<std::u16string>(&value))
        return *s;
    return {};
}

std::u32string_view xstring::u32_view() const
{
    if (auto* s = std::get_if<std::u32string>(&value))
        return *s;
    return {};
}

// ============================================================
//  Conversions
// ============================================================

xstring::operator scl2::string() const { return a(); }
xstring::operator scl2::wstring() const { return w(); }
xstring::operator std::u8string() const { return u8(); }
xstring::operator std::u16string() const { return u16(); }
xstring::operator std::u32string() const { return u32(); }

xstring::operator std::string() const { return std::string(a()); }
xstring::operator std::wstring() const { return std::wstring(w()); }

// ============================================================
//  Assignment (keep type)
// ============================================================

xstring& xstring::assign(const xstring& s)
{
    if (type() == stype::null || s.type() == stype::null)
        return *this = s;

    return std::visit([&](const auto& src) -> xstring& {
        using SrcT = std::decay_t<decltype(src)>;
        if constexpr (std::is_same_v<SrcT, std::monostate>) {
            return *this;
        } else {
            return std::visit([&](auto& dst) -> xstring& {
                using DstT = std::decay_t<decltype(dst)>;
                if constexpr (std::is_same_v<DstT, std::monostate>) {
                    return *this;
                } else {
                    dst = _match<DstT>(s);
                    return *this;
                }
            }, value);
        }
    }, s.value);
}

// ============================================================
//  Type conversion
// ============================================================

xstring& xstring::convert(stype target)
{
    if (type() == target) return *this;

    switch (target) {
    case stype::null:     reset(); break;
    case stype::ansi:     *this = a(); break;
    case stype::wchar:    *this = w(); break;
    case stype::u8char:   *this = u8(); break;
    case stype::u16char:  *this = u16(); break;
    case stype::u32char:  *this = u32(); break;
    }
    return *this;
}

xstring xstring::converted(stype target) const
{
    xstring r = *this;
    r.convert(target);
    return r;
}

// ============================================================
//  Mutation
// ============================================================

void xstring::clear()
{
    std::visit([](auto& v) {
        if constexpr (requires { v.clear(); })
            v.clear();
    }, value);
}

void xstring::reset()
{
    value = std::monostate();
}

// ============================================================
//  Passthrough operations
// ============================================================

xstring xstring::substr(size_t pos, size_t n) const
{
    return _call([&](const auto& s) { return s.substr(pos, n); });
}

xstring xstring::substr(size_t pos) const
{
    return _call([&](const auto& s) { return s.substr(pos); });
}

size_t xstring::find(const xstring& str, size_t pos) const
{
    return _query<size_t>([&](const auto& s) {
        return s.find(_match<std::decay_t<decltype(s)>>(str), pos);
    });
}

size_t xstring::rfind(const xstring& str, size_t pos) const
{
    return _query<size_t>([&](const auto& s) {
        return s.rfind(_match<std::decay_t<decltype(s)>>(str), pos);
    });
}

bool xstring::contains(const xstring& str) const
{
    return find(str) != std::string::npos;
}

bool xstring::starts_with(const xstring& prefix) const
{
    return _query<bool>([&](const auto& s) {
        return s.starts_with(_match<std::decay_t<decltype(s)>>(prefix));
    });
}

bool xstring::ends_with(const xstring& suffix) const
{
    return _query<bool>([&](const auto& s) {
        return s.ends_with(_match<std::decay_t<decltype(s)>>(suffix));
    });
}

// ============================================================
//  Comparison
// ============================================================

bool xstring::operator==(const xstring& other) const
{
    if (type() == stype::null && other.type() == stype::null) return true;
    if (type() == stype::null || other.type() == stype::null) return false;

    // Convert right side to left side's type, then compare
    return std::visit([&](const auto& a) -> bool {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, std::monostate>)
            return false;
        else
            return a == _match<T>(other);
    }, value);
}

} // namespace scl2