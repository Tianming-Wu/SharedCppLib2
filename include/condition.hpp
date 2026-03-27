/*
    New condition module for SharedCppLib2.
    Tianming <github.com/Tianming-Wu> 2026.03.27



*/

#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <stdexcept>
#include <functional>

#include "bytearray.hpp"
#include "api.hpp"

#include "logical.hpp"

namespace scl2 {

class condition_node;

typedef std::function<bool()> condition_function_t;

class condition_node {
public:
    enum class Type : uint8_t {
        Null = 0,
        Endpoint = 1,
        Logical = 2 // Basic node
    };

    enum class EndpointType : uint8_t {
        BoolValue = 0,
        Function = 1
        // Timer is no longer a thing, if you need it just use function.
    };

    // condition_node() : type(Type::Null) {}
    ~condition_node() = default;

    enable_copy_move(condition_node)

    // check the whole node tree to see if it is valid.
    bool valid() const;
    bool strong_valid() const; // This checks Null nodes and function validity.

    bool isEndpoint() const;
    bool isBoolValue() const;
    bool isFunction() const;
    bool hasFunction() const; // check if function assigned
    bool isLogical() const;

    bool evaluate() const;

    // simplify the condition node tree by evaluating and collapsing constant nodes, and flattening logical nodes where possible.
    condition_node simplify() const;

    // Look through the entire node tree to find the function endpoint with the given func_id.
    bool setFunction(const std::string& func_id, condition_function_t func);

private:
    condition_node();

private:
    struct _endpoint { EndpointType endpoint_type; };
    struct _func { condition_function_t func; std::string func_id; };

    struct _logical { condition_node left, right; logical_operator op; };

    Type type;

    std::optional<_endpoint> endpoint; // endpoint metadata

    std::optional<_func> endpoint_func; // for functional endpoint
    std::optional<bool> endpoint_value; // for boolean value endpoint

    std::optional<_logical> logical;

    // runtime flags that does not get stored into files
    bool dirty = true;

public: // api
    static condition_node load(const std::bytearray_view& data);
    static std::bytearray dump(const condition_node& node);
};

scl2_check_generic_dump_load(condition_node)

using condition_expression = condition_node; // for better readability

} // namespace scl2
