/*
    A simple argument parser library. Supports many cool features.

    However, the current implementation can only support specific formats of
    arguments, so if you want some complex grammar, you should parse it yourself.

    This library supports the most commonly used formats of arguments, and is also
    adjustable to some extent. It should be enough for most use cases.

    Multiple plain arguments like the ones used by git is not fully supported.
    Currently, the library only supports one primary command.

    The full document can be found at the bottom of this file.

    classes:
        std::arguments, std::warguments
    link target:
        SharedCppLib2::arguments
*/
#pragma once

#include "stringlist.hpp"
#include "basics.hpp"
#include "platform.hpp" // unluckily needed for string conversion helpers

#include <map>
#include <stdexcept>
#include <charconv>
#include <type_traits>
#include <functional>

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
    typedef std::basic_stringlist<CharT> stringlist_type;

    // allow unscoped usage to simply, since user will need to combine multiple flags.
    enum parse_policy { 
        Null = 0,
        AllowCombinedOptions = 1 << 0,
        FailIfEmptyValue = 1 << 1,
        AllowEqualSign = 1 << 2,  // Parse --option=value syntax (on by default)
        HelpAboutBlocking = 1 << 3, // Stop parsing options if help/version detected
        EnablePrimaryCommand = 1 << 4, // Allow a primary command (first argument and non-option) (off by default)
    };
    Define_Enum_BitOperators_Inclass(parse_policy)

    static constexpr parse_policy default_policy = 
        basic_arguments<CharT>::AllowEqualSign;

    enum argument_style {
        Style_GNU,      // --long-option, -s, -abc
        Style_POSIX,    // --long-option, -s, -a -b -c
        Style_Windows,  // /long-option, /s, /abc   /* This one is stupid */
    };

    basic_arguments(int argc, CharT** argv);
    basic_arguments(int argc, CharT** argv, parse_policy policy);
    basic_arguments(int argc, CharT** argv, parse_policy policy, argument_style style);
    ~basic_arguments() = default;

    enable_move_only(basic_arguments)

    using std::basic_stringlist<CharT>::size;
    using std::basic_stringlist<CharT>::at;
    using std::basic_stringlist<CharT>::operator[];
    
    /// Get the program name (argv[0])
    /// @return Program name/path
    string_type name() const;
    
    /// Check if there are any arguments (excluding argv[0])
    /// @return true if there are arguments besides program name
    bool empty() const;

    /// Check if parse is successful.
    /// @return true if parsing failed
    bool fail() const;

    // Key feature: everything was actually parsed afterwards, and the behavior is actually in parse_policy.
    bool addParameter(const string_type &name, string_type& value, const string_type& default_value = string_type());
    bool addParameter(const string_type &name, bool& value, bool default_value = false);

    template<typename _TN>
    requires(is_integral_v<_TN> && !is_same_v<_TN, bool>)
    bool addParameter(const string_type &name, _TN& value, _TN default_value = 0, int base = 10) {
        auto it = m_parameters.find(name);
        if (it == m_parameters.end()) {
            value = default_value;
            return false;
        }
        if (it->second.value.empty()) {
            if (testPolicy(FailIfEmptyValue))
                throw parameter_error("Argument '" + name + "' requires an integral value.");
            value = default_value;
            return false;
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
        return true;
    }

    bool addFlag(const string_type &name, bool& value, bool default_value = false);
    bool addFlag(const string_type &name); // only work on returns
    bool addEnum(const string_type &name, int& value, const std::map<string_type, int>& options, int default_value = 0);
    
    // Template overload for enum types - accepts map<string, E> where E is an enum
    template<typename E>
    requires std::is_enum_v<E>
    bool addEnum(const string_type &name, E &value, const std::map<string_type, E> &options, E default_value = static_cast<E>(0))
    {
        std::map<string_type, int> int_opts;
        for (const auto &p : options) {
            int_opts.emplace(p.first, static_cast<int>(p.second));
        }

        int int_default = static_cast<int>(default_value);
        int int_value = static_cast<int>(value);

        // Call the existing int version
        bool result = addEnum(name, int_value, int_opts, int_default);

        // Write the parsed result back to the enum variable
        value = static_cast<E>(int_value);
        return result;
    }

    // This version accepts alias name with a stringlist as the secondary key.
    // This parameter does not look very common, but is easy to write.
    template <typename E>
    requires std::is_enum_v<E>
    bool addEnumAlias(const string_type &name, E &value, const std::map<stringlist_type, E> &options, E default_value = static_cast<E>(0))
    {
        std::map<string_type, E> str_to_enum;
        for (const auto &p : options) {
            for (const auto &str : p.first) {
                str_to_enum.emplace(str, p.second);
            }
        }

        return addEnum(name, value, str_to_enum, default_value);
    }
    
    // For types with deserialize() or deserialise() method (custom serializable types)
    template<typename T>
    requires requires(T& t, const string_type& s) {
        requires std::is_class_v<T>;
        requires requires { t.deserialize(s); } || requires { t.deserialise(s); };  
    }
    bool addParameter(const string_type &name, T& value, std::optional<T> default_value = std::nullopt) {
        auto it = m_parameters.find(name);
        if (it == m_parameters.end()) {
            if (default_value.has_value())
                value = default_value.value();
            return false;
        }
        if (it->second.value.empty()) {
            if (testPolicy(FailIfEmptyValue))
                throw parameter_error("Argument '" + name + "' requires a value.");
            if (default_value.has_value())
                value = default_value.value();
            return false;
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
        return true;
    }

    // For floating-point types
    template<typename T>
    requires(std::is_floating_point_v<T>)
    bool addParameter(const string_type &name, T& value, std::optional<T> default_value = std::nullopt) {
        auto it = m_parameters.find(name);
        if (it == m_parameters.end()) {
            if (default_value.has_value())
                value = default_value.value();
            return false;
        }
        if (it->second.value.empty()) {
            if (testPolicy(FailIfEmptyValue))
                throw parameter_error("Argument '" + name + "' requires a numeric value.");
            if (default_value.has_value())
                value = default_value.value();
            return false;
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
        return true;
    }

    /// @brief Returns whatever comes after the first occurrence of the specified name.
    /// @param name 
    /// @return the string after the name, or empty string if not found.
    string_type anyAfter(const string_type &name) const;

    string_type anyAfter(size_t index) const;

    /// @brief Returns whatever comes before the first occurrence of the specified name.
    /// @param name 
    /// @return the string before the name, or empty string if not found.
    /// @warning Does include the executable name.
    string_type anyBefore(const string_type &name) const;

    string_type anyBefore(size_t index) const;

    /**
     * @brief Returns whatever comes between the first occurrence of name1 and name2.
     * @param name1 The first name, must be in front of name2
     * @param name2 The second name, must be behind name1, otherwise it will not be recognized.
     * @return the string between name1 and name2, or empty string if not found.
     * 
     * This is useful for some cases like mytool `--multioption opt1 opt2 set --` where "--" 
     * means the end of dynamic options.
     * 
     * (Note: that is not actually supported, since the current parse logic does not recognize
     * `--`. This is just a function.)
    */
    string_type anyBetween(const string_type &name1, const string_type &name2) const;

    
    string_type anyBetween(size_t index1, size_t index2) const;

    bool addHelp(std::function<void()> helpFunction);
    bool addVersion(std::function<void()> versionFunction);

    // Primary command handling
    bool addPrimaryCommand(string_type& prim);
    string_type getPrimaryCommand();

    // should this be protected?
    bool testPolicy(parse_policy p);

    string_type joinArgs() const;
    string_type xjoinArgs() const;

    // trigger re-parse with current policy and style, doesn't do anything yet
    // because setting policy/style after construction is not supported yet.
    // void reparse();

protected:
    // internal parsing methods
    void parse();

    void parse_GNU();
    void parse_POSIX();
    void parse_Windows();

    // Helper function to create string literals with correct character type
    template<size_t N>
    static constexpr string_type S(const char (&str)[N]) {
        if constexpr (std::is_same_v<CharT, char>) {
            return string_type(str);
        } else {
            wchar_t wstr[N];
            for (size_t i = 0; i < N; ++i) {
                wstr[i] = static_cast<wchar_t>(str[i]);
            }
            return string_type(wstr, N - 1);
        }
    }

    // Helper function to create char literals with correct character type
    static constexpr CharT C(char c) {
        if constexpr (std::is_same_v<CharT, char>) {
            return c;
        } else {
            return static_cast<CharT>(c);
        }
    }

    // Helper function to convert string_type to std::string for exceptions
    static std::string toNarrow(const string_type& str) {
        if constexpr (std::is_same_v<CharT, char>) {
            return str;
        } else {
            return platform::wstringToString(str);
        }
    }

private:
    parse_policy m_policy;
    argument_style m_style;
    string_type m_primaryCommand;

    bool m_failed = false; // indicates if parsing failed due to invalid arguments or policy violations

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


/*
    Work document


*/