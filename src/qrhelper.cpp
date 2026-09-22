#include "qrhelper.hpp"

namespace scl2::qrcode {

scl2::wstring escapeUrl(const scl2::wstring &url)
{
    // RFC 3986 percent-encoding for a URL.
    //  - Keeps unreserved chars (ALPHA / DIGIT / - . _ ~) and URL structure
    //    chars ( / : ? @ [ ] ) so a full URL stays intact.
    //  - Encodes spaces, quotes, a literal '%', and other reserved chars
    //    ( # & + = ; , ' ( ) ! $ * etc. ).
    //  - Encodes any non-ASCII character as UTF-8 percent bytes.
    scl2::wstring result;
    result.reserve(url.length() * 3);

    auto encode_byte = [&](uint8_t b) {
        static const wchar_t hex[] = L"0123456789ABCDEF";
        result += L'%';
        result += hex[b >> 4];
        result += hex[b & 0xF];
    };
    auto encode_utf8 = [&](uint32_t cp) {
        if (cp < 0x80) {
            encode_byte(static_cast<uint8_t>(cp));
        } else if (cp < 0x800) {
            encode_byte(static_cast<uint8_t>(0xC0 | (cp >> 6)));
            encode_byte(static_cast<uint8_t>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            encode_byte(static_cast<uint8_t>(0xE0 | (cp >> 12)));
            encode_byte(static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F)));
            encode_byte(static_cast<uint8_t>(0x80 | (cp & 0x3F)));
        } else {
            encode_byte(static_cast<uint8_t>(0xF0 | (cp >> 18)));
            encode_byte(static_cast<uint8_t>(0x80 | ((cp >> 12) & 0x3F)));
            encode_byte(static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F)));
            encode_byte(static_cast<uint8_t>(0x80 | (cp & 0x3F)));
        }
    };

    for (size_t i = 0; i < url.length(); ++i) {
        uint32_t cp = static_cast<uint32_t>(url[i]);
        // UTF-16 surrogate pair (Windows wchar_t) -> full code point.
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < url.length()) {
            const uint32_t lo = static_cast<uint32_t>(url[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }

        if (cp < 0x80) {
            const char ac = static_cast<char>(cp);
            const bool unreserved =
                (ac >= 'a' && ac <= 'z') || (ac >= 'A' && ac <= 'Z') ||
                (ac >= '0' && ac <= '9') || ac == '-' || ac == '.' ||
                ac == '_' || ac == '~';
            const bool structure =
                ac == '/' || ac == ':' || ac == '?' || ac == '@' ||
                ac == '[' || ac == ']';
            if (unreserved || structure) result += static_cast<wchar_t>(ac);
            else encode_utf8(cp);
        } else {
            encode_utf8(cp);
        }
    }
    return result;
}

scl2::wstring escapeString(const scl2::wstring &str, EscapeRule rule)
{
    if(str.empty()) return L"";

    scl2::wstring result;
    result.reserve(str.length() * 1.2);

    for (const wchar_t& c : str) {
        switch(c) {
        case L'\\': result += L"\\\\"; break;
        case L';':  result += L"\\;";  break;
        case L':':  result += L"\\:";  break;
        case L',':  result += L"\\,";  break;
        case L'\n': result += L"\\n";  break;
        case L'\r': result += L"\\r";  break;
        default:   result += c;
        }
    }

    switch(rule) {
    case WifiRule:
        result.find_and_replace(L" ", L"\\20");
        break;
    case vCardRule:
        result.find_and_replace(L"\"", L"\\\"");
        break;
    }

    return result;
}

scl2::wstring wifi::generate(Secure s, const scl2::wstring& ssid, const scl2::wstring& password, bool hiddenSSID) {
    return std::format(L"WIFI:T:{};S:{};P:{};H:{};;",
        [s]{
            if(s == Secure::WPA) return L"WPA";
            else if(s == Secure::WEP) return L"WEP";
            else if(s == Secure::nopass) return L"nopass";
            else throw std::runtime_error("Unknown Secure type");
        }(),
        ssid, password, hiddenSSID ? L"true" : L"false"
    );
}

} // namespace scl2::qrcode