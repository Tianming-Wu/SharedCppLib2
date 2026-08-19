#include "i18n.hpp"
#include "json.hpp"
#include "platform.hpp"

#include <stdexcept>
#include <filesystem>

i18n* i18n::trInstance = nullptr;

i18n::i18n()
{
    if (trInstance != nullptr) {
        throw std::runtime_error("i18n instance already exists");
    }

    trInstance = this;
}

i18n::~i18n()
{
    trInstance = nullptr;
}

std::wstring i18n::system_locale()
{
#ifdef _WIN32
    wchar_t langCode[LOCALE_NAME_MAX_LENGTH];
    return (GetUserDefaultLocaleName(langCode, LOCALE_NAME_MAX_LENGTH) > 0) ?
        std::wstring(langCode) : std::wstring();
#else
    return std::wstring(); // Not implemented for non-Windows platforms
#endif
}

void i18n::autoLoad()
{
    std::wstring lang_code = system_locale();
    if (lang_code.empty()) {
        lang_code = L"en-US"; // Default to English if system language code is not available
    }
    load(lang_code);
}

bool i18n::load(const std::wstring &lang_code)
{
    std::wstring filename = L"lang/" + lang_code + L".json";
    if (!std::filesystem::exists(filename)) return false;

    scl2::json j = scl2::json::fromFile(filename);
    if (!j.is_object()) return false;

    auto load_object = [this](const scl2::json_object& obj, const std::wstring& prefix, auto& self) -> void {
        for (const auto& [key, value] : obj) {
            std::wstring key_wstr = std::wstring(key.begin(), key.end());
            std::wstring full_key = prefix.empty() ? key_wstr : prefix + L":" + key_wstr;

            if (value.is_string()) {
                entries[full_key] = tr_entry{ scl2::xstring(value.as_wstring()) };
            } else if (value.is_array()) {
                std::vector<scl2::xstring> arr;
                for (const auto& item : value.as_array())
                    arr.push_back(scl2::xstring(item.as_wstring()));
                entries[full_key] = tr_entry{ arr };
            } else if (value.is_object()) {
                self(value.as_object(), full_key, self);
            }
        }
    };

    load_object(j.as_object(), L"", load_object);

    m_valid = true;
    return true;
}

scl2::xstring i18n::tr(const scl2::xstring &key)
{
    if (key.empty()) return scl2::xstring();
    if (!trInstance) return key;
    auto it = trInstance->entries.find(key.w());
    if (it != trInstance->entries.end()) {
        if (auto* s = std::get_if<scl2::xstring>(&it->second.value)) {
            scl2::xstring result = *s;
            if (result.type() != key.type())
                result.convert(key.type());
            return result;
        }
    }
    return key;
}

// const wchar_t* i18n::trc(const std::wstring &key)
// {
//     if (!trInstance) return key.c_str();
//     if (trInstance->entries.find(key) != trInstance->entries.end()) {
//         return std::get<scl2::xstring>(trInstance->entries[key].value).w().c_str();
//     }
//     return key.c_str();
// }

scl2::xstring i18n::trl(const scl2::xstring &key, size_t index)
{
    if (key.empty()) return scl2::xstring();
    if (!trInstance) return key;
    auto it = trInstance->entries.find(key.w());
    if (it != trInstance->entries.end()) {
        if (auto* arr = std::get_if<std::vector<scl2::xstring>>(&it->second.value)) {
            if (index < arr->size()) {
                scl2::xstring result = (*arr)[index];
                if (result.type() != key.type())
                    result.convert(key.type());
                return result;
            }
        }
    }
    return key;
}

// const wchar_t *i18n::trlc(const std::wstring &key, size_t index)
// {
//     return trl(key, index).c_str();
// }

std::wstring i18n::_apply(std::wstring temp, std::initializer_list<trx_arg> args)
{
    if (temp.empty()) return temp;
    if (temp.find(L"${") == std::wstring::npos) return temp; // no placeholders

    int arg_index = 0;
    for (const auto& arg : args) {
        std::wstring arg_index_placeholder = L"${" + std::to_wstring(arg_index) + L"}";
        size_t ap = 0;
        while ((ap = temp.find(arg_index_placeholder)) != std::wstring::npos)
            temp.replace(ap, arg_index_placeholder.length(), arg.value);
        arg_index++;
    }

    // ... means all arguments (not remaining). connected with `, `
    if (size_t pos = temp.find(L"${...}"); pos != std::wstring::npos) {
        std::wstring connected = [&]() {
            std::wstring result;
            for (const auto& arg : args) {
                if (!result.empty()) result += L", ";
                result += arg.value;
            }
            return result;
        }();

        do {
            temp.replace(pos, 5, connected);
        } while ((pos = temp.find(L"${...}")) != std::wstring::npos);
    }

    return temp;
}

scl2::xstring i18n::trx(const scl2::xstring &key_temp, std::initializer_list<trx_arg> args)
{
    scl2::xstring translated = tr(key_temp);
    scl2::xstring result(_apply(translated.w(), args));
    if (result.type() != key_temp.type())
        result.convert(key_temp.type());
    return result;
}

// const wchar_t *i18n::trxc(const std::wstring &key_temp, std::initializer_list<trx_arg> args)
// {
//     return trx(key_temp, args).c_str();
// }

scl2::xstring i18n::trxl(const scl2::xstring &key_temp, size_t index, std::initializer_list<trx_arg> args)
{
    scl2::xstring translated = trl(key_temp, index);
    scl2::xstring result(_apply(translated.w(), args));
    if (result.type() != key_temp.type())
        result.convert(key_temp.type());
    return result;
}

// const wchar_t *i18n::trxlc(const std::wstring &key_temp, size_t index, std::initializer_list<trx_arg> args)
// {
//     return trxl(key_temp, index, args).c_str();
// }

bool i18n::valid() const
{
    return m_valid;
}
