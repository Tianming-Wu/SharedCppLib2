#include "arguments.hpp"

// This is used for some invalid/ignored flags,
// and will be later used to dock with log
#define ARGUMENTS_IGNORE_FLAG_WARNING(flag, TEXT)

namespace std {

template class basic_arguments<char>;
template class basic_arguments<wchar_t>;

// // Default parsing policy: allows space-separated values and equals syntax
// template<typename CharT>
// constexpr typename basic_arguments<CharT>::parse_policy basic_arguments<CharT>::default_policy = 
//     basic_arguments<CharT>::AllowEqualSign;


template<typename CharT>
basic_arguments<CharT>::basic_arguments(int argc, CharT** argv)
    : std::basic_stringlist<CharT>(argc, argv), m_policy(default_policy), m_style(Style_GNU)
{
    parse();
}

template <typename CharT>
basic_arguments<CharT>::basic_arguments(int argc, CharT **argv, parse_policy policy)
    : std::basic_stringlist<CharT>(argc, argv), m_policy(policy), m_style(Style_GNU)
{
    parse();
}

template <typename CharT>
basic_arguments<CharT>::basic_arguments(int argc, CharT **argv, parse_policy policy, argument_style style)
    : std::basic_stringlist<CharT>(argc, argv), m_policy(policy), m_style(style)
{
    parse();
}

template <typename CharT>
bool basic_arguments<CharT>::addParameter(const string_type &name, string_type &value, const string_type &default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        if (!it->second.value.empty()) {
            value = it->second.value;
            return true;
        } else {
            if(testPolicy(FailIfEmptyValue)) {
                throw parameter_error("Argument '" + toNarrow(name) + "' requires a value.");
            }
            value = default_value;
            return false;
        }
    } else {
        value = default_value;
        return false;
    }
}

template <typename CharT>
std::basic_string<CharT> basic_arguments<CharT>::name() const {
    return this->size() > 0 ? this->at(0) : string_type();
}

template <typename CharT>
bool basic_arguments<CharT>::empty() const {
    return this->size() <= 1;
}

template <typename CharT>
bool basic_arguments<CharT>::addParameter(const string_type &name, bool &value, bool default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        if(!it->second.value.empty()) {
            string_type val = it->second.value;
            if (val == S("1") || val == S("true") || val == S("yes") || val == S("on")) {
                value = true;
                return true;
            } else if (val == S("0") || val == S("false") || val == S("no") || val == S("off")) {
                value = false;
                return true;
            } else {
                throw parameter_error("Argument '" + toNarrow(name) + "' requires a boolean value.");
            }
        } else {
            if(testPolicy(FailIfEmptyValue)) {
                throw parameter_error("Argument '" + toNarrow(name) + "' requires a boolean value.");
            }
            value = true; // Presence of flag implies true. in this case, addFlag works the same.
            return true;
        }
    } else {
        value = default_value;
        return false;
    }
}

template <typename CharT>
bool basic_arguments<CharT>::addFlag(const string_type &name, bool &value, bool default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        value = true;
        return true;
    } else {
        value = default_value;
        return false;
    }
}

template <typename CharT>
bool basic_arguments<CharT>::addFlag(const string_type &name)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        return true;
    } else {
        return false;
    }
}

template <typename CharT>
bool basic_arguments<CharT>::addEnum(const string_type &name, int &value, const std::map<string_type, int> &options, int default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        if (!it->second.value.empty()) {
            auto opt_it = options.find(it->second.value);
            if (opt_it != options.end()) {
                value = opt_it->second;
                return true;
            } else {
                throw parameter_error("Argument '" + toNarrow(name) + "' has invalid enum value '" + toNarrow(it->second.value) + "'.");
            }
        } else {
            if(testPolicy(FailIfEmptyValue)) {
                throw parameter_error("Argument '" + toNarrow(name) + "' requires a value.");
            }
            value = default_value;
            return false;
        }
    } else {
        value = default_value;
        return false;
    }
}

template <typename CharT>
bool basic_arguments<CharT>::addHelp(std::function<void()> helpFunction)
{
    if(m_parameters.find(S("help")) != m_parameters.end()) {
        helpFunction();
        if (testPolicy(HelpAboutBlocking)) {
            exit(0);
        }
        return true;
    }
    return false;
}

template <typename CharT>
bool basic_arguments<CharT>::addVersion(std::function<void()> versionFunction)
{
    if(m_parameters.find(S("version")) != m_parameters.end()) {
        versionFunction();
        if (testPolicy(HelpAboutBlocking)) {
            exit(0);
        }
        return true;
    }
    return false;
}

template <typename CharT>
bool basic_arguments<CharT>::testPolicy(parse_policy p)
{
    return (m_policy & p) != 0;
}

template <typename CharT>
void basic_arguments<CharT>::parse()
{
    switch(m_style) {
    case Style_GNU:
        parse_GNU(); break;
    case Style_POSIX:
        parse_POSIX(); break;
    case Style_Windows:
        parse_Windows(); break;
    default:
        throw parameter_error("Unknown argument style.");
    }
}

template <typename CharT>
void basic_arguments<CharT>::parse_GNU()
{
    m_parameters.clear();
    // Start from i=1 to skip argv[0] (program name)
    for (size_t i = 1; i < this->size(); i++) {
        const string_type &arg = this->at(i);
        if (arg.length() >= 2 && arg[0] == '-' ) {
            if (arg[1] == '-') {
                // Long option: --option
                string_type name = arg.substr(2);
                string_type value;
                
                // Check for --option=value syntax if policy allows
                if (testPolicy(AllowEqualSign)) {
                    size_t eq_pos = name.find('=');
                    if (eq_pos != string_type::npos) {
                        value = name.substr(eq_pos + 1);
                        name = name.substr(0, eq_pos);
                        m_parameters[name] = { i, value };
                        continue;
                    }
                }
                
                // Default: --option value (space-separated)
                if (i + 1 < this->size() && this->at(i + 1)[0] != '-') {
                    value = this->at(i + 1);
                    i++;  // Skip next argument
                }
                m_parameters[name] = { i, value };
            } else if (testPolicy(AllowCombinedOptions) && arg.length() > 2) {
                // Combined short options: -abc
                for (size_t j = 1; j < arg.length(); j++) {
                    string_type name(1, arg[j]);
                    m_parameters[name] = { i, S("") };
                }
            } else {
                // Short option: -o or -ovalue
                string_type name(1, arg[1]);
                string_type value;
                
                if (arg.length() > 2) {
                    // -ovalue (attached)
                    value = arg.substr(2);
                } else if (i + 1 < this->size() && this->at(i + 1)[0] != '-') {
                    // -o value (space-separated)
                    value = this->at(i + 1);
                    i++;  // Skip next argument
                }
                m_parameters[name] = { i, value };
            }
        }
    }
}

template <typename CharT>
void basic_arguments<CharT>::parse_POSIX()
{
    // Similar to GNU but does not support combined options

    // In this case, AllowCombinedOptions is ignored.
    ARGUMENTS_IGNORE_FLAG_WARNING(AllowCombinedOptions, "POSIX style does not support combined options.");

    m_parameters.clear();
    // Start from i=1 to skip argv[0] (program name)
    for (size_t i = 1; i < this->size(); i++) {
        const string_type &arg = this->at(i);
        if (arg.length() >= 2 && arg[0] == '-' ) {
            if (arg[1] == '-') {
                // Long option: --option
                string_type name = arg.substr(2);
                string_type value;
                
                // Check for --option=value syntax if policy allows
                if (testPolicy(AllowEqualSign)) {
                    size_t eq_pos = name.find('=');
                    if (eq_pos != string_type::npos) {
                        value = name.substr(eq_pos + 1);
                        name = name.substr(0, eq_pos);
                        m_parameters[name] = { i, value };
                        continue;
                    }
                }
                
                // Default: --option value (space-separated)
                if (i + 1 < this->size() && this->at(i + 1)[0] != '-') {
                    value = this->at(i + 1);
                    i++;  // Skip next argument
                }
                m_parameters[name] = { i, value };
            } else {
                // Short option: -o or -ovalue
                string_type name(1, arg[1]);
                string_type value;
                
                if (arg.length() > 2) {
                    // -ovalue (attached)
                    value = arg.substr(2);
                } else if (i + 1 < this->size() && this->at(i + 1)[0] != '-') {
                    // -o value (space-separated)
                    value = this->at(i + 1);
                    i++;  // Skip next argument
                }
                m_parameters[name] = { i, value };
            }
        }
    }
}

template <typename CharT>
void basic_arguments<CharT>::parse_Windows()
{
    // not supprted :(
}

} // namespace std