/*
    Helper function set for generating qr codes.

    This module provides multiple pre-defined patterns of QRCode content.

*/

#pragma once

#include <format>

#include "string.hpp"

namespace scl2::qrcode {

enum EscapeRule {
    WifiRule, vCardRule
};

scl2::wstring escapeUrl(const scl2::wstring& url);
scl2::wstring escapeString(const scl2::wstring& str, EscapeRule rule);

scl2::wstring link(const scl2::wstring& title, const scl2::wstring& url);

namespace wifi {

enum class Secure {
    WPA, WEP, nopass
};

scl2::wstring generate(Secure s, const scl2::wstring& ssid, const scl2::wstring& password, bool hiddenSSID);

} // namespace wifi




} // namespace scl2::qrcode