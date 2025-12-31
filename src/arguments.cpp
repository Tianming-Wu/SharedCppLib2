#include "arguments.hpp"

// This is used for some invalid/ignored flags,
// and will be later used to dock with log
#define ARGUMENTS_IGNORE_FLAG_WARNING(flag, TEXT)

namespace std {

template class basic_stringlist<char>;
template class basic_stringlist<wchar_t>;


template<typename CharT>
basic_arguments<CharT>::basic_arguments(int argc, CharT** argv)
    : std::basic_stringlist<CharT>(argc, argv), m_policy(Null), m_style(Style_GNU)
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
void basic_arguments<CharT>::addParameter(const string_type &name, string_type &value, const string_type &default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        if (!it->second.value.empty()) {
            value = it->second.value;
        } else {
            if(testPolicy(FailIfEmptyValue)) {
                throw parameter_error("Argument '" + name + "' requires a value.");
            }
            value = default_value;
        }
    } else {
        value = default_value;
    }
}

template <typename CharT>
void basic_arguments<CharT>::addParameter(const string_type &name, bool &value, bool default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        if(!it->second.value.empty()) {
            string_type val = it->second.value;
            if (val == "1" || val == "true" || val == "yes" || val == "on") {
                value = true;
            } else if (val == "0" || val == "false" || val == "no" || val == "off") {
                value = false;
            } else {
                throw parameter_error("Argument '" + name + "' requires a boolean value.");
            }
        } else {
            if(testPolicy(FailIfEmptyValue)) {
                throw parameter_error("Argument '" + name + "' requires a boolean value.");
            }
            value = true; // Presence of flag implies true. in this case, addFlag works the same.
        }
    } else {
        value = default_value;
    }
}

template <typename CharT>
void basic_arguments<CharT>::addFlag(const string_type &name, bool &value, bool default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        value = true;
    } else {
        value = default_value;
    }
}

template <typename CharT>
void basic_arguments<CharT>::addEnum(const string_type &name, int &value, const std::map<string_type, int> &options, int default_value)
{
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        if (!it->second.value.empty()) {
            auto opt_it = options.find(it->second.value);
            if (opt_it != options.end()) {
                value = opt_it->second;
            } else {
                throw parameter_error("Argument '" + name + "' has invalid enum value '" + it->second.value + "'.");
            }
        } else {
            if(testPolicy(FailIfEmptyValue)) {
                throw parameter_error("Argument '" + name + "' requires a value.");
            }
            value = default_value;
        }
    } else {
        value = default_value;
    }
}

template <typename CharT>
bool basic_arguments<CharT>::testPolicy(parse_policy p) {
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
    for (size_t i = 0; i < this->size(); i++) {
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
                    m_parameters[name] = { i, "" };
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
    for (size_t i = 0; i < this->size(); i++) {
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