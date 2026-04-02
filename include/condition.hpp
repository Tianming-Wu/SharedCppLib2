/*
    New condition module for SharedCppLib2.
    Tianming <github.com/Tianming-Wu> 2026.03.27

    For condition expression format, check document (condition.md).

*/

#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <stdexcept>
#include <functional>
#include <tuple>
#include <set>
#include <vector>
#include <map>

#include "bytearray.hpp"
#include "api.hpp"
#include "macros.hpp"

#include "logical.hpp"


namespace scl2 {

class condition_node;

typedef std::function<bool()> condition_function_t;

class condition_node {
    friend class condition_simplifier;
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

    enable_move_only(condition_node) // unique_ptr is not copyable, and we don't need it anyway

    // explicitly deep-copy the node. This should not be used in most
    // cases, unless you need a different tree based on the old one
    // while you need the old one as well.
    condition_node deep_copy() const; 

    // check the whole node tree to see if it is valid.
    bool valid() const;
    bool strong_valid() const; // This checks Null nodes and function validity.

    bool isNull() const;
    bool isEndpoint() const;
    bool isBoolValue() const;
    bool isFunction() const;
    bool hasFunction() const; // check if function assigned
    bool isLogical() const;

    bool evaluate() const;

    // simplify the condition node tree by evaluating and collapsing constant nodes, and flattening logical nodes where possible.
    condition_node simplify() const;

    // simplify by using 
    // condition_node super_simplify() const;

    // Look through the entire tree to find the function endpoint with the given func_id.
    // Return the number of endpoints found and set.
    int setFunction(const std::string& func_id, condition_function_t func);

    std::vector<std::string> getFunctions() const; // get a list of all function ids in the tree

private_constructor:
    condition_node();

private:
    struct _endpoint { EndpointType endpoint_type; };
    struct _func { condition_function_t func; std::string func_id; };

    struct _logical {
        std::unique_ptr<condition_node> left;
        std::unique_ptr<condition_node> right;
        logical_operator op;
    };

    Type type;

    std::optional<_endpoint> endpoint; // endpoint metadata

    std::optional<_func> endpoint_func; // for functional endpoint
    std::optional<bool> endpoint_value; // for boolean value endpoint

    std::unique_ptr<_logical> logical; // for logical nodes

    // runtime flags that does not get stored into files
    bool dirty = true;

private:
    // helper functions
    condition_node simplify_logical() const;
    // condition_node simplify_chain() const;
    condition_node simplify_not(condition_node&& child) const;
    condition_node simplify_constant(const condition_node& l, const condition_node& r, logical_operator op) const;
    // simplify_imply
    condition_node apply_identities(condition_node&& l, condition_node&& r, logical_operator op) const;
    bool is_same_function(const condition_node& l, const condition_node& r) const;
    bool is_same_tree(const condition_node& l, const condition_node& r) const;
    bool is_negation_of(const condition_node& a, const condition_node& b) const;

    // collect operands for associative operators
    // void collect_operands(logical_operator op, std::vector<const condition_node*> &operands) const;

    std::vector<std::string> getFunctions_internal() const; // helper for getFunctions

factories:
    // factory methods
    factory condition_node create_null();
    factory condition_node create_bool(bool value);
    factory condition_node create_function(const std::string& func_id);
    factory condition_node create_logical(condition_node&& left, condition_node&& right, logical_operator op);
    factory condition_node create_unary(condition_node&& child, logical_operator op);

interface:
    // api
    static condition_node load(const std::bytearray_view& data);
    static std::bytearray dump(const condition_node& node);

    // Build a logical expression tree from a string.
    // For the format of the string, check document (condition.md).
    static condition_node parse(const std::string& str);

    static std::string to_string(const condition_node& node);
};

scl2_check_generic_dump_load(condition_node)

using condition_expression = condition_node; // for better readability


// simplify the condition expression by using MCluskey algorithm.
// Reject inputs with more than 8 variables.
class condition_simplifier {
public:
    static condition_expression simplify(const condition_expression& cexpr);

    struct term {
        typedef uint8_t mtvar_t;

        uint8_t mask;
        uint8_t value;
        bool used = false;

        inline bool operator==(const term& other) const {
            return mask == other.mask && value == other.value;
        }
    };

private:
    static std::tuple<std::map<int, std::vector<term>>, std::vector<term::mtvar_t>> generate_table(condition_expression& expr);
    static std::vector<term> make_plain(const std::map<int, std::vector<term>>& groups);
    static condition_expression reconstruct_tree(const std::set<size_t>& indices, const std::vector<term>& pis, const std::vector<std::string>& func_ids);
};

// Generate and print the truth table for the given condition expression.
// **Will change** the function bindings of the condition expression.
// You need to make sure that all function id are only 1 char long,
// to generate a better looking truth table.
void generate_truth_table(condition_node& root, bool colored = false);

} // namespace scl2
