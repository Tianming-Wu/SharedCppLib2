#pragma once

#include "stringlist.hpp"
#include "basics.hpp"

#include <map>
#include <stdexcept>
#include <charconv>

namespace std {

class parameter_error : public std::runtime_error {
public:
    explicit parameter_error(const std::string& message)
        : std::runtime_error(message) {}
};

template<typename CharT>
class basic_arguments : protected std::basic_stringlist<CharT>
{
public:
    typedef std::basic_string<CharT> string_type;

    // allow unscoped usage to simply, since user will need to combine multiple flags.
    enum parse_policy { 
        Null = 0,
        AllowCombinedOptions = 1 << 0,
        FailIfEmptyValue = 1 << 1,
        AllowEqualSign = 1 << 2,  // Parse --option=value syntax
    };
    Define_Enum_BitOperators_Inclass(parse_policy)

    enum argument_style {
        Style_GNU,      // --long-option, -s, -abc
        Style_POSIX,    // --long-option, -s, -a -b -c
        Style_Windows,  // /long-option, /s, /abc   /* This one is stupid */
    };

    basic_arguments(int argc, CharT** argv);
    basic_arguments(int argc, CharT** argv, parse_policy policy);
    basic_arguments(int argc, CharT** argv, parse_policy policy, argument_style style);
    ~basic_arguments() = default;

    using std::basic_stringlist<CharT>::size;
    using std::basic_stringlist<CharT>::empty;
    using std::basic_stringlist<CharT>::at;
    using std::basic_stringlist<CharT>::operator[];

    // Key feature: everything was actually parsed afterwards, and the behavior is actually in parse_policy.
    void addParameter(const string_type &name, string_type& value, const string_type& default_value = string_type());
    void addParameter(const string_type &name, bool& value, bool default_value = false);

    template<typename _TN>
    requires(is_integral_v<_TN> && !is_same_v<_TN, bool>)
    void addParameter(const string_type &name, _TN& value, _TN default_value = 0, int base = 10) {
        auto it = m_parameters.find(name);
        if (it == m_parameters.end()) {
            value = default_value;
            return;
        }
        if (it->second.value.empty()) {
            if (testPolicy(FailIfEmptyValue))
                throw parameter_error("Argument '" + name + "' requires an integral value.");
            value = default_value;
            return;
        }
        if constexpr (is_same_v<CharT, char>) {
            const auto& str = it->second.value;
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value, base);
            if (ec != std::errc{} || ptr != str.data() + str.size())
                throw parameter_error("Argument '" + name + "' has invalid integral value '" + str + "'.");
        } else {
            std::string narrow(it->second.value.begin(), it->second.value.end());
            auto [ptr, ec] = std::from_chars(narrow.data(), narrow.data() + narrow.size(), value, base);
            if (ec != std::errc{} || ptr != narrow.data() + narrow.size())
                throw parameter_error("Argument '" + name + "' has invalid integral value.");
        }
    }

    void addFlag(const string_type &name, bool& value, bool default_value = false);
    void addEnum(const string_type &name, int& value, const std::map<string_type, int>& options, int default_value = 0);
    
    // For types with deserialize() or deserialise() method (custom serializable types)
    template<typename T>
    requires requires(T& t, const string_type& s) {
        requires std::is_class_v<T>;
        requires requires { t.deserialize(s); } || requires { t.deserialise(s); };
    }
    void addParameter(const string_type &name, T& value, std::optional<T> default_value = std::nullopt) {
        auto it = m_parameters.find(name);
        if (it == m_parameters.end()) {
            if (default_value.has_value())
                value = default_value.value();
            return;
        }
        if (it->second.value.empty()) {
            if (testPolicy(FailIfEmptyValue))
                throw parameter_error("Argument '" + name + "' requires a value.");
            if (default_value.has_value())
                value = default_value.value();
            return;
        }
        try {
            if constexpr (requires { value.deserialize(it->second.value); }) {
                value.deserialize(it->second.value);
            } else {
                value.deserialise(it->second.value);
            }
        } catch (const std::exception& e) {
            throw parameter_error("Argument '" + name + "' failed to deserialize: " + std::string(e.what()));
        }
    }

    // For floating-point types
    template<typename T>
    requires(std::is_floating_point_v<T>)
    void addParameter(const string_type &name, T& value, std::optional<T> default_value = std::nullopt) {
        auto it = m_parameters.find(name);
        if (it == m_parameters.end()) {
            if (default_value.has_value())
                value = default_value.value();
            return;
        }
        if (it->second.value.empty()) {
            if (testPolicy(FailIfEmptyValue))
                throw parameter_error("Argument '" + name + "' requires a numeric value.");
            if (default_value.has_value())
                value = default_value.value();
            return;
        }
        if constexpr (std::is_same_v<CharT, char>) {
            const auto& str = it->second.value;
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
            if (ec != std::errc{} || ptr != str.data() + str.size())
                throw parameter_error("Argument '" + name + "' has invalid floating-point value '" + str + "'.");
        } else {
            // For wchar_t, convert to narrow string first
            std::string narrow(it->second.value.begin(), it->second.value.end());
            auto [ptr, ec] = std::from_chars(narrow.data(), narrow.data() + narrow.size(), value);
            if (ec != std::errc{} || ptr != narrow.data() + narrow.size())
                throw parameter_error("Argument '" + name + "' has invalid floating-point value.");
        }
    }

    // should this be protected?
    bool testPolicy(parse_policy p);

    // trigger re-parse with current policy and style, doesn't do anything yet
    // because setting policy/style after construction is not supported yet.
    // void reparse();

protected:
    // internal parsing methods
    void parse();

    void parse_GNU();
    void parse_POSIX();
    void parse_Windows();

private:
    parse_policy m_policy;
    argument_style m_style;

    struct param_info {
        size_t position;
        string_type value;
    };
    
    std::map<string_type, param_info> m_parameters;
};



extern template class basic_arguments<char>;
extern template class basic_arguments<wchar_t>;

typedef basic_arguments<char> arguments;
typedef basic_arguments<wchar_t> warguments;

} // namespace std