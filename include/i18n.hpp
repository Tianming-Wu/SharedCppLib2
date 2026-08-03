/*
    i18n Module for SharedCppLib2.

    * Migration: migrated from AutoClicker project's independent module "translator".

    This module can be used to load translation files in JSON format,
    and translate keys to their corresponding values.

    If no translation is found for a key, the key itself will be returned.
    So it is possible to use it as default English, or use a "value_key".

    It is recommended to organize translation entries using "type.module.key" format.
    e.g. "menu.file.open", "menu.file.save", "menu.edit.copy", "menu.edit.paste", etc.

    Warning:
        c_postfix function does not work properly under certain conditions, since
        it passes a temporary string to the caller that may be destroyed before used.

        In this case, capture the string using the non-c version, then use .c_str() of the
        string object.


    namespace: none
    class: i18n

*/

#pragma once

#include <map>
#include <vector>

#include "xstring.hpp"

namespace scl2::i18n::detail {

struct tr_entry
{
    std::variant<
        scl2::xstring,
        std::vector<scl2::xstring>
    > value;
};

struct trx_arg
{
    scl2::xstring value;

    trx_arg(std::nullptr_t) {} // empty argument that can be used as placeholder
    trx_arg(scl2::xstring s) : value(s) {}
    trx_arg(const char* s) : value(s) {} // added explicitly since double implicit does not work.
    trx_arg(const wchar_t* s) : value(s) {}
    trx_arg(int i) : value(scl2::to_xstring(i)) {}
    trx_arg(long i) : value(scl2::to_xstring(i)) {}
    trx_arg(long long i) : value(scl2::to_xstring(i)) {}
    trx_arg(unsigned i) : value(scl2::to_xstring(i)) {}
    trx_arg(unsigned long long i) : value(scl2::to_xstring(i)) {}
    trx_arg(double d) : value(scl2::to_xstring(d)) {}
};

} // namespace scl2::i18n::detail

using scl2::i18n::detail::tr_entry;
using scl2::i18n::detail::trx_arg;

class i18n
{
public:
    i18n();
    ~i18n();
    
    /** @brief Get system locale in "lang-REGION" format, e.g. "en-US", "zh-CN", etc.
     * @note This function is only available on Windows.
     * @return Empty string if the system locale cannot be determined.
     */
    static std::wstring system_locale();

    // Automatically load translation file based on system locale.
    // No language override available. If you need that feature, use load(lang_code).
    // You can also use system_locale() to get the system locale, and work with the config.
    void autoLoad();

    // Load translation file based on language code, e.g. "en-US", "zh-CN", etc.
    // This does not need to be a valid system language code, as long as the corresponding translation file exists.
    /// @return true if the file was loaded successfully, false if not found or parse error.
    bool load(const std::wstring& lang_code);

    /// @brief Translate a key to its corresponding value.
    /// @param key The translation key. Return type matches the input type.
    /// @return The translated value if found, otherwise the key itself.
    static scl2::xstring tr(const scl2::xstring& key);

    /// @brief Translate a key to its corresponding value with an index for array values.
    /// @param key The translation key. Return type matches the input type.
    /// @param index The index of the value in the array.
    /// @return The translated value if found, otherwise the key itself.
    static scl2::xstring trl(const scl2::xstring& key, size_t index);

    static std::wstring _apply(std::wstring temp, std::initializer_list<trx_arg> args);

    /// @brief Translate a key template to its corresponding value with arguments.
    /// @param key_temp The key template with placeholders. Return type matches the input type.
    /// @param args The arguments to replace the placeholders.
    /// @return The translated value if found, otherwise the key template itself.
    static scl2::xstring trx(const scl2::xstring& key_temp, std::initializer_list<trx_arg> args);

    /// @brief Translate a key template to its corresponding value with an index and arguments.
    /// @param key_temp The key template with placeholders. Return type matches the input type.
    /// @param index The index of the value in the array.
    /// @param args The arguments to replace the placeholders.
    /// @return The translated value if found, otherwise the key template itself.
    static scl2::xstring trxl(const scl2::xstring& key_temp, size_t index, std::initializer_list<trx_arg> args);

    /// @brief Check if the translation instance is valid.
    /// This means that a translation file has been successfully loaded and is ready for use.
    bool valid() const;

private:
    static i18n* trInstance;
    bool m_valid = false;
    std::map<std::wstring, tr_entry> entries;

};