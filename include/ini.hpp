/*
    Ini format parser and writer.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.01.15

    You can use this file seperately without including the whole SharedCppLib2.
    However, that was really suggested.

    We abandoned the idea of using qt-like .as...() since that requires variant types.
    Instead, we use templated getValueAs...() and setValue() functions.

    The goal is to keep it simple and easy to use, but also lightweight and fast.
    So we did not really added that much error checking or complex features, like '=' in
    values or key, etc.

    Supported types:
        std::string (const char*), integral types, floating-point types, bool,
        std::stringlist, std::bytearray

    Note: Section and Key names are case-sensitive.
    
    Note: You should include api.hpp before this file to activate API type support,
    or they won't activate.

    Usage Example:
        ini config;
        config.loadFromFile("config.ini");
        int val = config.getValueAsInteger("Settings", "Width", 800);
        config.setValue("Settings", "Width", val);
        config.saveToFile("config.ini");
*/

#pragma once

#include <string>
#include <map>
#include <filesystem>


#include <optional>

// forward declaration
namespace std {
    template<typename CharT> class basic_stringlist;
    using stringlist = basic_stringlist<char>; using wstringlist = basic_stringlist<wchar_t>;
    class bytearray;
}

class ini {
public:
    ini() = default;
    ~ini() = default;

    bool loadFromFile(const std::filesystem::path& filename);
    bool saveToFile(const std::filesystem::path& filename) const;

    // You are actually allowed to save/load from any stream.
    std::istream& operator>>(std::istream& is);
    std::ostream& operator<<(std::ostream& os) const;

    std::map<std::string, std::string>& operator[] (const std::string& section);
    const std::map<std::string, std::string>& operator[] (const std::string& section) const;

    // string-based accessors
    std::string getValue(const std::string& section, const std::string& key, const std::string& default_value = "") const;
    void setValue(const std::string& section, const std::string& key, const std::string& value);

    // integer types accessors
    template<typename T>
    requires std::is_integral_v<T>
    T getValueAsInteger(const std::string& section, const std::string& key, const T& default_value = T()) const {
        if(data.contains(section) && data.at(section).contains(key)) {
            const std::string& valueStr = data.at(section).at(key);
            try {
                return static_cast<T>(std::stoll(valueStr));
            } catch(...) {
                return default_value;
            }
        }
        return default_value;
    }

    // floating-point types accessors
    template<typename T>
    requires std::is_floating_point_v<T>
    T getValueAsFloat(const std::string& section, const std::string& key, const T& default_value = T()) const {
        if(data.contains(section) && data.at(section).contains(key)) {
            const std::string& valueStr = data.at(section).at(key);
            try {
                return static_cast<T>(std::stold(valueStr));
            } catch(...) {
                return default_value;
            }
        }
        return default_value;
    }

    // Write function for any types that support to_string
    template<typename T>
    requires requires (const T& value) { std::to_string(value); }
    void setValue(const std::string& section, const std::string& key, const T& value) {
        data[section][key] = std::to_string(value);
    }

    // boolean type accessors
    bool getValueAsBool(const std::string& section, const std::string& key, bool default_value = false) const;
    void setValue(const std::string& section, const std::string& key, bool value);

    // special support for std::stringlist
    std::stringlist getValueAsStringList(const std::string& section, const std::string& key) const;
    std::stringlist getValueAsStringList(const std::string& section, const std::string& key, const std::stringlist& default_value) const;
    void setValue(const std::string& section, const std::string& key, const std::stringlist& value);

    // no, wstringlist is not supported

    // special support for std::bytearray
    std::bytearray getValueAsByteArray(const std::string& section, const std::string& key) const;
    std::bytearray getValueAsByteArray(const std::string& section, const std::string& key, const std::bytearray& default_value) const;
    void setValue(const std::string& section, const std::string& key, const std::bytearray& value);

    // template function for api-supported types. You need to inclulde api.hpp for this to work.
    #ifdef SHAREDCPPLIB2_API_SUPPORT
    template<typename T>
    requires scl2::hasSerializeSupport<T>
    T getValueAsApiType(const std::string& section, const std::string& key, const T& default_value = T()) const {
        if(data.contains(section) && data.at(section).contains(key)) {
            const std::string& valueStr = data.at(section).at(key);
            T value;
            typename T::string_type s = typename T::string_type(valueStr.begin(), valueStr.end());
            scl2::__deserialize_function<T>(value);
            return value;
        }
        return default_value;
    }
    
    template<typename T>
    requires scl2::hasSerializeSupport<T>
    void setValue(const std::string& section, const std::string& key, const T& value) {
        typename T::string_type s;
        scl2::__serialize_function<T>(value, s);
        data[section][key] = std::string(s.begin(), s.end());
    }
    #endif // SHAREDCPPLIB2_API_SUPPORT


    std::optional<std::string> getValueOptional(const std::string& section, const std::string& key) const;

    template<typename T>
    requires std::is_integral_v<T>
    std::optional<T> getValueAsIntegerOptional(const std::string& section, const std::string& key) const {
        if(data.contains(section) && data.at(section).contains(key)) {
            const std::string& valueStr = data.at(section).at(key);
            try {
                return std::optional<T>(static_cast<T>(std::stoll(valueStr)));
            } catch(...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    template<typename T>
    requires std::is_floating_point_v<T>
    std::optional<T> getValueAsFloatOptional(const std::string& section, const std::string& key) const {
        if(data.contains(section) && data.at(section).contains(key)) {
            const std::string& valueStr = data.at(section).at(key);
            try {
                return std::optional<T>(static_cast<T>(std::stold(valueStr)));
            } catch(...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<bool> getValueAsBoolOptional(const std::string& section, const std::string& key) const;
    std::optional<std::stringlist> getValueAsStringListOptional(const std::string& section, const std::string& key) const;
    std::optional<std::bytearray> getValueAsByteArrayOptional(const std::string& section, const std::string& key) const;

    bool hasSection(const std::string& section) const;
    bool hasKey(const std::string& section, const std::string& key) const;

    void removeKey(const std::string& section, const std::string& key);
    void removeSection(const std::string& section);

    // not really for the user, but you can use it if you want to.
    auto __getRawData() const -> const std::map<std::string, std::map<std::string, std::string>>&;

private:
    std::map<std::string, std::map<std::string, std::string>> data;
    // std::string currentSection; // for selection style usage like enterSection() and leaveSection()
};