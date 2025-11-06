/*
    Variant project inspired by QVariant.
    This project is currently deprecated, but may restart if good
    solutions are found.
*/

#pragma once
#include <string>
#include <stdexcept>
#include "stringlist.hpp"
#include "bytearray.hpp"

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

    // Constructors
    variant();
    variant(bool value);
    variant(int value);
    variant(const std::string& value);
    variant(const char* value);
    variant(const std::stringlist& value);
    variant(const std::bytearray& value);
    variant(const variant& other);
    variant(variant&& other) noexcept;
    
    ~variant();

    // query
    Type type() const { return m_type; }

    // conversion methods
    bool toBool() const;
    int toInt() const;
    std::string toString() const;
    std::stringlist toStringList() const;
    std::bytearray toByteArray() const;

    // serualization and deserialization
    std::string serialize() const;
    static variant deserialize(const std::string& data);

    variant& operator=(const variant& other);
    variant& operator=(variant&& other) noexcept;

    // stream operators
    friend std::ostream& operator<<(std::ostream& os, const variant& var);
    friend std::istream& operator>>(std::istream& is, variant& var);

private:
    Type m_type;
    union {
        bool boolValue;
        int intValue;
        std::string stringValue;
        std::stringlist stringListValue;
        std::bytearray byteArrayValue;
    };

    void cleanup(); // cleanup none-POD types
};