#include "variant.hpp"
#include <cstring>
#include <sstream>
#include <algorithm>

variant::variant() : m_type(Type::Invalid) {}

variant::variant(bool value) : m_type(Type::Bool) {
    new (&boolValue) bool(value);
}

variant::variant(int value) : m_type(Type::Int) {
    new (&intValue) int(value);
}

variant::variant(const std::string& value) : m_type(Type::String) {
    new (&stringValue) std::string(value);
}

variant::variant(const char* value) : variant(std::string(value)) {}

variant::variant(const std::stringlist& value) : m_type(Type::StringList) {
    new (&stringListValue) std::stringlist(value);
}

variant::variant(const ByteArray& value) : m_type(Type::ByteArray) {
    new (&byteArrayValue) ByteArray(value);
}

variant::variant(const variant& other) : m_type(other.m_type) {
    switch (m_type) {
        case Type::Bool: new (&boolValue) bool(other.boolValue); break;
        case Type::Int: new (&intValue) int(other.intValue); break;
        case Type::String: new (&stringValue) std::string(other.stringValue); break;
        case Type::StringList: new (&stringListValue) std::stringlist(other.stringListValue); break;
        case Type::ByteArray: new (&byteArrayValue) ByteArray(other.byteArrayValue); break;
        default: break;
    }
}

variant::variant(variant&& other) noexcept : m_type(other.m_type) {
    switch (m_type) {
        case Type::Bool: new (&boolValue) bool(other.boolValue); break;
        case Type::Int: new (&intValue) int(other.intValue); break;
        case Type::String: new (&stringValue) std::string(std::move(other.stringValue)); break;
        case Type::StringList: new (&stringListValue) std::stringlist(std::move(other.stringListValue)); break;
        case Type::ByteArray: new (&byteArrayValue) ByteArray(std::move(other.byteArrayValue)); break;
        default: break;
    }
    other.m_type = Type::Invalid; // 置空原对象
}

variant::~variant() {
    cleanup();
}

void variant::cleanup() {
    switch (m_type) {
        case Type::String:
            stringValue.~basic_string(); // 显式调用析构
            break;
        case Type::StringList:
            stringListValue.~stringlist();
            break;
        case Type::ByteArray:
            byteArrayValue.~ByteArray();
            break;
        default:
            break;
    }
    m_type = Type::Invalid; // 重置类型
}

bool variant::toBool() const {
    if (m_type == Type::Bool) return boolValue;
    if (m_type == Type::String) {
        if (stringValue == "true") return true;
        if (stringValue == "false") return false;
    }
    throw std::runtime_error("Invalid conversion to bool");
}

int variant::toInt() const {
    if (m_type == Type::Int) return intValue;
    if (m_type == Type::String) return std::stoi(stringValue);
    throw std::runtime_error("Invalid conversion to int");
}

std::string variant::toString() const {
    switch (m_type) {
        case Type::Bool: return boolValue ? "true" : "false";
        case Type::Int: return std::to_string(intValue);
        case Type::String: return stringValue;
        case Type::StringList: return stringListValue.join(",");
        case Type::ByteArray: return byteArrayValue.tohex();
        default: return "";
    }
}

std::stringlist variant::toStringList() const {
    if (m_type != Type::StringList) throw std::runtime_error("Not a stringlist");
    return stringListValue;
}

ByteArray variant::toByteArray() const {
    if (m_type != Type::ByteArray) throw std::runtime_error("Not a ByteArray");
    return byteArrayValue;
}

// 转义特殊字符（逗号、反斜杠）
std::string escapeString(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        if (c == '\\' || c == ',') oss << '\\';
        oss << c;
    }
    return oss.str();
}

// 反转义
std::string unescapeString(const std::string& s) {
    std::ostringstream oss;
    bool escape = false;
    for (char c : s) {
        if (escape) {
            oss << c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else {
            oss << c;
        }
    }
    return oss.str();
}

std::string variant::serialize() const {
    switch (m_type) {
        case Type::Bool: return boolValue ? "true" : "false";
        case Type::Int: return std::to_string(intValue);
        case Type::String: return escapeString(stringValue);
        case Type::StringList: {
            std::stringlist escapedList;
            for (const auto& s : stringListValue) {
                escapedList.append(escapeString(s));
            }
            return escapedList.join(",");
        }
        case Type::ByteArray: return byteArrayValue.tohex();
        default: return "";
    }
}

variant variant::deserialize(const std::string& data) {
    // 尝试解析为Bool
    if (data == "true") return variant(true);
    if (data == "false") return variant(false);

    // 尝试解析为Int
    try {
        return variant(std::stoi(data));
    } catch (...) {}

    // 尝试解析为ByteArray（偶数长度且全为十六进制字符）
    if (data.size() % 2 == 0 && std::all_of(data.begin(), data.end(), ::isxdigit)) {
        return variant(ByteArray::fromHex(data));
    }

    // 尝试解析为StringList
    std::stringlist list;
    std::string current;
    bool escape = false;
    for (char c : data) {
        if (escape) {
            current += c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == ',') {
            list.push_back(unescapeString(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        list.push_back(unescapeString(current));
    }
    if (!list.empty()) {
        return variant(list);
    }

    // 默认为String
    return variant(unescapeString(data));
}

variant& variant::operator=(const variant& other) {
    if (this == &other) return *this; // 自赋值检查

    cleanup();

    m_type = other.m_type;
    switch (m_type) {
        case Type::Bool:
            new (&boolValue) bool(other.boolValue);
            break;
        case Type::Int:
            new (&intValue) int(other.intValue);
            break;
        case Type::String:
            new (&stringValue) std::string(other.stringValue);
            break;
        case Type::StringList:
            new (&stringListValue) std::stringlist(other.stringListValue);
            break;
        case Type::ByteArray:
            new (&byteArrayValue) ByteArray(other.byteArrayValue);
            break;
        default:
            break;
    }

    return *this;
}

variant& variant::operator=(variant&& other) noexcept {
    if (this == &other) return *this;
    cleanup();
    m_type = other.m_type;
    switch (m_type) {
        case Type::Bool: boolValue = other.boolValue; break;
        case Type::Int: intValue = other.intValue; break;
        case Type::String: stringValue = std::move(other.stringValue); break;
        case Type::StringList: stringListValue = std::move(other.stringListValue); break;
        case Type::ByteArray: byteArrayValue = std::move(other.byteArrayValue); break;
        default: break;
    }
    other.m_type = Type::Invalid;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const variant& var) {
    os << var.serialize();
    return os;
}

inline std::istream& operator>>(std::istream& is, variant& var) {
    std::string data;
    is >> data;
    variant temp = variant::deserialize(data); // 构造临时对象
    var.cleanup();                            // 清理原对象
    var = std::move(temp);                    // 使用移动赋值（需定义移动语义）
    return is;
}