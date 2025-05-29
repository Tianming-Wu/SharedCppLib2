#pragma once
#include <string>
#include <stdexcept>
#include "stringlist.hpp"
#include "bytearray.h"

class variant {
public:
    enum class Type {
        Invalid,
        Bool,
        Int,
        String,
        StringList,
        ByteArray
    };

    // 构造函数
    variant();
    variant(bool value);
    variant(int value);
    variant(const std::string& value);
    variant(const char* value); // 委托给string构造函数
    variant(const std::stringlist& value);
    variant(const ByteArray& value);
    variant(const variant& other);
    variant(variant&& other) noexcept;
    ~variant();

    // 类型查询
    Type type() const { return m_type; }

    // 转换方法
    bool toBool() const;
    int toInt() const;
    std::string toString() const;
    std::stringlist toStringList() const;
    ByteArray toByteArray() const;

    // 序列化与反序列化
    std::string serialize() const;
    static variant deserialize(const std::string& data);

    variant& operator=(const variant& other);
    variant& operator=(variant&& other) noexcept;

    // 流操作符
    friend std::ostream& operator<<(std::ostream& os, const variant& var);
    friend std::istream& operator>>(std::istream& is, variant& var);

private:
    Type m_type;
    union {
        bool boolValue;
        int intValue;
        std::string stringValue;
        std::stringlist stringListValue;
        ByteArray byteArrayValue;
    };

    void cleanup(); // 清理union中的非POD类型
};