#include "ini.hpp"

#include <fstream>
#include <stringlist.hpp>
#include <bytearray.hpp>

bool ini::loadFromFile(const std::filesystem::path &filename)
{
    std::ifstream ifs(filename);

    if(!ifs.good() || !ifs.is_open()) {
        return false;
    }

    this->operator>>(ifs);

    ifs.close();
    return true;
}

bool ini::saveToFile(const std::filesystem::path &filename) const
{
    if(data.empty()) return true; // Nothing to save
    std::ofstream ofs(filename);

    if(!ofs.good() || !ofs.is_open()) {
        return false;
    }

    this->operator<<(ofs);

    ofs.close();
    return true;
}

std::istream &ini::operator>>(std::istream &is)
{
    data.clear(); // Clear existing data

    std::string line, currentSection;

    while(std::getline(is, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if(line.empty() || line[0] == ';' || line[0] == '#') {
            continue; // Skip comments and empty lines
        }

        if(line.front() == '[' && line.back() == ']') {
            // Section header
            currentSection = line.substr(1, line.size() - 2);
        } else {
            // Key-value pair
            size_t equalPos = line.find('=');
            if(equalPos != std::string::npos) {
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);

                // Trim whitespace from key and value
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                data[currentSection][key] = value;
            }
        }
    }

    return is;
}

std::ostream &ini::operator<<(std::ostream &os) const
{
    for(const auto & [section, keys] : data) {
        os << "[" << section << "]\n";
        for(const auto & [key, value] : keys) {
            os << key << "=" << value << "\n";
        }
        os << "\n"; // Separate sections by a blank line
    }

    return os;
}

std::map<std::string, std::string> &ini::operator[](const std::string &section)
{
    return data[section]; // This will create the section if it doesn't exist
}

const std::map<std::string, std::string> &ini::operator[](const std::string &section) const
{
    return data.at(section); // Will throw if section doesn't exist
}

std::string ini::getValue(const std::string &section, const std::string &key, const std::string &default_value) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        return data.at(section).at(key);
    }
    return default_value;
}

void ini::setValue(const std::string &section, const std::string &key, const std::string &value)
{
    data[section][key] = value; // This will create the section if it doesn't exist
}

bool ini::getValueAsBool(const std::string& section, const std::string& key, bool default_value) const {
    if(data.contains(section) && data.at(section).contains(key)) {
        const std::string& valueStr = data.at(section).at(key);
        if(valueStr == "1" || valueStr == "true" || valueStr == "yes" || valueStr == "on") {
            return true;
        } else if(valueStr == "0" || valueStr == "false" || valueStr == "no" || valueStr == "off") {
            return false;
        }
    }
    return default_value;
}

void ini::setValue(const std::string &section, const std::string &key, bool value)
{
    data[section][key] = value ? "true" : "false";
}

std::stringlist ini::getValueAsStringList(const std::string &section, const std::string &key) const
{
    return std::stringlist::unpack(getValue(section, key, ""));
}

std::stringlist ini::getValueAsStringList(const std::string &section, const std::string &key, const std::stringlist &default_value) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        const std::string& valueStr = data.at(section).at(key);
        return std::stringlist::unpack(valueStr);
    }
    return default_value;
}

void ini::setValue(const std::string &section, const std::string &key, const std::stringlist &value)
{
    data[section][key] = value.pack();
}

std::bytearray ini::getValueAsByteArray(const std::string &section, const std::string &key) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        const std::string& valueStr = data.at(section).at(key);
        return std::bytearray::fromHex(valueStr);
    }
    return std::bytearray();
}

std::bytearray ini::getValueAsByteArray(const std::string &section, const std::string &key, const std::bytearray &default_value) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        const std::string& valueStr = data.at(section).at(key);
        return std::bytearray::fromHex(valueStr);
    }
    return default_value;
}

void ini::setValue(const std::string &section, const std::string &key, const std::bytearray &value)
{
    data[section][key] = value.toHex();
}

std::optional<std::string> ini::getValueOptional(const std::string& section, const std::string& key) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        return std::optional<std::string>(data.at(section).at(key));
    }
    return std::nullopt;
}

std::optional<bool> ini::getValueAsBoolOptional(const std::string& section, const std::string& key) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        const std::string& valueStr = data.at(section).at(key);
        if(valueStr == "1" || valueStr == "true" || valueStr == "yes" || valueStr == "on") {
            return std::optional<bool>(true);
        } else if(valueStr == "0" || valueStr == "false" || valueStr == "no" || valueStr == "off") {
            return std::optional<bool>(false);
        }
    }
    return std::nullopt;
}

std::optional<std::stringlist> ini::getValueAsStringListOptional(const std::string& section, const std::string& key) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        const std::string& valueStr = data.at(section).at(key);
        return std::optional<std::stringlist>(std::stringlist::unpack(valueStr));
    }
    return std::nullopt;
}

std::optional<std::bytearray> ini::getValueAsByteArrayOptional(const std::string& section, const std::string& key) const
{
    if(data.contains(section) && data.at(section).contains(key)) {
        const std::string& valueStr = data.at(section).at(key);
        return std::optional<std::bytearray>(std::bytearray::fromHex(valueStr));
    }
    return std::nullopt;
}

bool ini::hasSection(const std::string &section) const
{
    return data.contains(section);
}

bool ini::hasKey(const std::string &section, const std::string &key) const
{
    return data.contains(section) && data.at(section).contains(key);
}

void ini::removeKey(const std::string &section, const std::string &key)
{
    if(data.contains(section) && data[section].contains(key)) {
        data[section].erase(key);
    }
}

void ini::removeSection(const std::string &section)
{
    if(data.contains(section)) {
        data.erase(section);
    }
}

auto ini::__getRawData() const -> const std::map<std::string, std::map<std::string, std::string>> &
{
    return data;
}
