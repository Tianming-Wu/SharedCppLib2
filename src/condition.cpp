#include "condition.hpp"

#include <algorithm>
#include <stdexcept>
#include <bitset>

#include "bits.hpp"

// temporary included for debugging purposes.
// Relavant functions might be kept, but will be moved to a better place.
#include "ansiio.hpp"

/*
    ///TODO: Support the dirty flag.
*/

namespace scl2 {

condition_node::condition_node() : type(Type::Null), dirty(true) {}

condition_node condition_node::deep_copy() const
{
    if (type == Type::Endpoint) {
        if (isBoolValue()) return create_bool(endpoint_value.value());
        return create_function(endpoint_func->func_id);
    } 
    
    if (type == Type::Logical) {
        // It is possible for unary operator to have empty right child.
        if (!logical->right) {
            return create_unary(logical->left->deep_copy(), logical->op);
        }
        return create_logical(logical->left->deep_copy(), logical->right->deep_copy(), logical->op);
    }

    return create_null();
}

bool condition_node::valid() const
{
    // This could get quite complex (

    // We might need to consider a better way to optimize the validation
    // process, since running it every single time we work on this expression
    // would be quite expensive.

    if(type == Type::Null) {
        return true; // no payload, always valid
    } else if(type == Type::Endpoint) {
        if(!endpoint.has_value()) {
            return false; // missing endpoint metadata
        }
        if(endpoint->endpoint_type == EndpointType::BoolValue) {
            return endpoint_value.has_value(); // must have a boolean value
        } else if(endpoint->endpoint_type == EndpointType::Function) {
            return endpoint_func.has_value(); // must have function data
        } else {
            return false; // unknown endpoint type
        }
    } else if(type == Type::Logical) {
        if(!logical || !logical->left) {
            return false; // missing logical operator data
        }

        if(__lo_is_not_only(logical->op)) {
            // only check left node
            return logical->left->valid();
        } else {
            // check both left and right node
            return logical->left->valid() && logical->right->valid();
        }
    } else {
        return false; // unknown node type
    }
}

bool condition_node::strong_valid() const
{
    if(type == Type::Null) {
        return false; // not valid for this case
    } else if(type == Type::Endpoint) {
        if(!endpoint.has_value()) {
            return false; // missing endpoint metadata
        }
        if(endpoint->endpoint_type == EndpointType::BoolValue) {
            return endpoint_value.has_value(); // must have a boolean value
        } else if(endpoint->endpoint_type == EndpointType::Function) {
            if (!endpoint_func.has_value()) {
                return false; // must have function data
            }

            // also check if the function is actually assigned.
            return endpoint_func->func != nullptr;

        } else {
            return false; // unknown endpoint type
        }
    } else if(type == Type::Logical) {
        if(!logical || !logical->left) {
            return false; // missing logical operator data
        }

        if(__lo_is_not_only(logical->op)) {
            // only check left node
            return logical->left->strong_valid();
        } else {
            // check both left and right node
            return logical->left->strong_valid() && logical->right->strong_valid();
        }
    } else {
        return false; // unknown node type
    }
}

bool condition_node::isNull() const
{
    return type == Type::Null;
}

bool condition_node::isEndpoint() const
{
    return type == Type::Endpoint && endpoint.has_value();
}

bool condition_node::isBoolValue() const {
    return isEndpoint() && endpoint->endpoint_type == EndpointType::BoolValue;
}

bool condition_node::isFunction() const {
    return isEndpoint() && endpoint->endpoint_type == EndpointType::Function;
}

bool condition_node::hasFunction() const
{
    if(isFunction()) {
        return endpoint_func.has_value() && endpoint_func->func != nullptr;
    } else {
        throw std::runtime_error("Performing hasFunction check on a non-function endpoint node");
    }
}

bool condition_node::isLogical() const
{
    return type == Type::Logical && logical && logical->left;
}

bool condition_node::evaluate() const
{
    if(type == Type::Endpoint) {
        if(endpoint->endpoint_type == EndpointType::BoolValue) {
            // return the value directly
            return endpoint_value.value();
        } else if(endpoint->endpoint_type == EndpointType::Function) {
            if(!endpoint_func->func) {
                throw std::runtime_error("Function endpoint not assigned with actual function");
            }
            return endpoint_func->func();
        } else {
            throw std::runtime_error("Unknown endpoint type in condition node");
        }
    } else if(type == Type::Logical) {
        bool r_left = logical->left->evaluate();

        if(__lo_is_not_only(logical->op)) {
            // only evaluate left node
            return lo_eval(logical->op, r_left);
        } else {
            // also need to evaluate right node
            bool r_right = logical->right->evaluate();
            return lo_eval(logical->op, r_left, r_right);
        }
    } else {
        // Yes, null nodes are considered valid, but they cannot be evaluated.
        throw std::runtime_error("Trying to evaluate an invalid condition node");
    }
}

condition_node condition_node::simplify() const
{
    // since managing the ownership of unique_ptr in a tree can be disgusting,
    // we will return a clean new tree instead.

    switch (type) {
        case Type::Null:
        case Type::Endpoint:
            return deep_copy();
        case Type::Logical:
            return simplify_logical();
        default:
            throw std::runtime_error("Unknown node type in condition node");
    };

    // Step 1 : Constant folding

    // Step 2 : Short-circuit evaluation

    // Step 3: Flattening logical nodes where possible.
    // For simplicity, this implementation only contains two children
    // each node, so it is only possible to flatten something like this:
    // ((A & B) & C) & D --> (A & B) & (C & D).
}

condition_node condition_node::simplify_logical() const
{
    if(!logical || !logical->left) {
        throw std::runtime_error("Trying to simplify an invalid logical node");
    }

    // simplify recursively
    condition_node simleft = logical->left->simplify();

    // handle unary operator cases
    if(__lo_is_not_only(logical->op)) {
        return simplify_not(std::move(simleft));
    }

    if(__lo_is_not(logical->op)) {
        // if it's a reversed operator, check if we can apply de morgan's laws.

        // (!A !& !B) -> A | B
        if (logical->op == logical_operator::Nand) {
            auto& left = *logical->left;
            auto& right = *logical->right;
            if (left.isLogical() && left.logical->op == logical_operator::Not &&
                right.isLogical() && right.logical->op == logical_operator::Not) {
                // extract A and B (the left child of !A and !B)
                return create_logical(
                    std::move(*left.logical->left),
                    std::move(*right.logical->left),
                    logical_operator::Or
                );
            }
        }

        // (!A !| !B) -> A & B
        if (logical->op == logical_operator::Nor) {
            auto& left = *logical->left;
            auto& right = *logical->right;
            if (left.isLogical() && left.logical->op == logical_operator::Not &&
                right.isLogical() && right.logical->op == logical_operator::Not) {
                // extract A and B (the left child of !A and !B)
                return create_logical(
                    std::move(*left.logical->left),
                    std::move(*right.logical->left),
                    logical_operator::And
                );
            }
        }
    }

    // handle binary operator cases
    condition_node simright = logical->right->simplify();

    // try to fold constants
    if(simleft.isBoolValue() && simright.isBoolValue()) {
        return simplify_constant(simleft, simright, logical->op);
    }

    // try to apply identities
    return apply_identities(std::move(simleft), std::move(simright), logical->op);
}

// condition_node condition_node::simplify_chain() const
// {
//     // Assume: isLogical(), operator is [ And, Or, Xor ], and both left and right child are valid.

//     std::vector<const condition_node*> raw_operands;
//     this->collect_operands(logical->op, raw_operands);

//     std::vector<condition_node> simplified_operands(raw_operands.size());
//     for(const auto operand : raw_operands) {
//         simplified_operands.push_back(operand->simplify());
//     }

//     bool changed = false;
//     for(size_t i = 0; i < raw_operands.size(); i++) {
//         for(size_t j = i + 1; j < raw_operands.size(); j++) {
//             //


//             changed = true;
//         }
//     }

//     if(changed) {
//         // if changed, reassemble the tree.


//     }
// }

condition_node condition_node::simplify_not(condition_node&& child) const
{
    // This function is responsible for listed cases:
    // ! ! -> ()
    // ! !& -> &, ! !| -> |, ! !^ -> ^, ! & -> !&, ! | -> !|, ! ^ -> !^

    // handle constants
    if (child.isBoolValue()) {
        return create_bool(!child.endpoint_value.value());
    }

    if (child.isLogical()) {
        logical_operator op = child.logical->op;
        
        // ! ! -> ()
        if (op == logical_operator::Not) {
            return std::move(*(child.logical->left));
        }

        // Apply De Morgan's laws:

        // !(!A & !B) -> A | B
        // Child is not, and child's left and right are also not.
        if (op == logical_operator::And) {
            auto& left = *child.logical->left;
            auto& right = *child.logical->right;
            if (left.isLogical() && left.logical->op == logical_operator::Not &&
                right.isLogical() && right.logical->op == logical_operator::Not) {
                // extract A and B (the left child of !A and !B)
                return create_logical(
                    std::move(*left.logical->left),
                    std::move(*right.logical->left),
                    logical_operator::Or
                );
            }
        }
        
        // !(!A | !B) -> A & B
        if (op == logical_operator::Or) {
            auto& left = *child.logical->left;
            auto& right = *child.logical->right;
            if (left.isLogical() && left.logical->op == logical_operator::Not &&
                right.isLogical() && right.logical->op == logical_operator::Not) {
                // extract A and B (the left child of !A and !B)
                return create_logical(
                    std::move(*left.logical->left),
                    std::move(*right.logical->left),
                    logical_operator::And
                );
            }
        }

        // ! !& -> &, ! !| -> |, ! !^ -> ^
        if (__lo_is_not(op)) {
            auto base_op = __lo_base_op(op);
            return create_logical(std::move(*(child.logical->left)), std::move(*(child.logical->right)), base_op);
        }

        // ! & -> !&, ! | -> !|, ! ^ -> !^
        // do not support imply operators here, since !(A -> B) is not equivalent to A -> !B or !A -> B.
        if (!__lo_is_imply(op)) {
            auto neg_op = __lo_add_not(op);
            return create_logical(std::move(*(child.logical->left)), std::move(*(child.logical->right)), neg_op);
        }
    }
    
    // cannot simplify
    return create_unary(std::move(child), logical_operator::Not);
}

condition_node condition_node::simplify_constant(const condition_node& l, const condition_node& r, logical_operator op) const
{
    return create_bool(lo_eval(op, l.endpoint_value.value(), r.endpoint_value.value()));
}

condition_node condition_node::apply_identities(condition_node&& l, condition_node&& r, logical_operator op) const
{
    /*
        1. AND:
            - If either side is false, the whole expression is false.
            - If one side is true, the expression simplifies to the other side.
        2. OR:
            - If either side is true, the whole expression is true.
            - If one side is false, the expression simplifies to the other side.
        3. NOT:
            - If the child is true, it simplifies to false.
            - If the child is false, it simplifies to true.
        4. XOR:
            - If one side is false, the expression simplifies to the other side.
            - If one side is true, the expression simplifies to the negation of the other side.
        5. IMPLY:
            - A implies B is equivalent to !A OR B. So if A is true, it simplifies to B; if A is false, it simplifies to true.
    */

    if (op == logical_operator::And) {
        // false & A -> false (short)
        if (l.isBoolValue() && !l.endpoint_value.value()|| (r.isBoolValue() && !r.endpoint_value.value())) {
            return create_bool(false);
        }

        // true & A -> A (constant)
        if (l.isBoolValue() && l.endpoint_value.value()) return std::move(r);
        if (r.isBoolValue() && r.endpoint_value.value()) return std::move(l);

        // A & !A -> false
        if (is_negation_of(l, r)) return create_bool(false);

        // A & A -> A
        if (is_same_tree(l, r)) return std::move(l);
    } else

    if (op == logical_operator::Or) {
        // true | A -> true (short)
        if (l.isBoolValue() && l.endpoint_value.value() || (r.isBoolValue() && r.endpoint_value.value())) {
            return create_bool(true);
        }

        // false | A -> A (constant)
        if (l.isBoolValue() && !l.endpoint_value.value()) return std::move(r);
        if (r.isBoolValue() && !r.endpoint_value.value()) return std::move(l);

        // A | !A -> true
        if (is_negation_of(l, r)) return create_bool(true);

        // A | A -> A
        if (is_same_tree(l, r)) return std::move(l);

        // (A & !B) | (A & B) -> A (combining terms)
        if (l.isLogical() && l.logical->op == logical_operator::And &&
            r.isLogical() && r.logical->op == logical_operator::And) {
            if (is_same_tree(*l.logical->left, *r.logical->left)) {
                // A & !B | A & B -> A
                if (is_negation_of(*l.logical->right, *r.logical->right)) {
                    return std::move(*l.logical->left);
                }
                // A & B | A & B -> A & B
                if (is_same_tree(*l.logical->right, *r.logical->right)) {
                    return std::move(l);
                }
            } else
            if (is_same_tree(*l.logical->left, *r.logical->right)) {
                // A & !B | B & A -> A
                if (is_negation_of(*l.logical->right, *r.logical->left)) {
                    return std::move(*l.logical->left);
                }
                // A & B | B & A -> A & B
                if (is_same_tree(*l.logical->right, *r.logical->left)) {
                    return std::move(l);
                }
            }
        }

        // A & B | A & !B -> A (absorption)
        if (l.isEndpoint() && r.isLogical() && r.logical->op == logical_operator::And) {
            if (is_negation_of(l, *r.logical->left)) return create_logical(std::move(l), std::move(*r.logical->right), logical_operator::Or);
            if (is_negation_of(l, *r.logical->right)) return create_logical(std::move(l), std::move(*r.logical->left), logical_operator::Or);
        }

        // A | (A & B) -> A
        if (l.isEndpoint() && r.isLogical() && r.logical->op == logical_operator::And) {
            if (is_same_tree(l, *r.logical->left) || is_same_tree(l, *r.logical->right)) {
                return std::move(l);
            }
        }

        // (A & B) | A -> A
        if (r.isEndpoint() && l.isLogical() && l.logical->op == logical_operator::And) {
            if (is_same_tree(r, *l.logical->left) || is_same_tree(r, *l.logical->right)) {
                return std::move(r);
            }
        }

    } else

    if (op == logical_operator::Xor) {
        // false ^ A -> A
        if (l.isBoolValue() && !l.endpoint_value.value()) return std::move(r);
        if (r.isBoolValue() && !r.endpoint_value.value()) return std::move(l);

        // true ^ A -> !A (equivalant to not A)
        if (l.isBoolValue() && l.endpoint_value.value()) return create_unary(std::move(r), logical_operator::Not);
        if (r.isBoolValue() && r.endpoint_value.value()) return create_unary(std::move(l), logical_operator::Not);
    } else

    if (op == logical_operator::Imply_Left) {
        // A -> A is always true.
        if (is_same_function(l, r)) return create_bool(true);

        // A -> B is equivalent to !A | B. So if A is true, it simplifies to B; if A is false, it simplifies to true.
        if (l.isBoolValue() && l.endpoint_value.value()) return std::move(r);
        if (l.isBoolValue() && !l.endpoint_value.value()) return create_bool(true);
    } else

    if (op == logical_operator::Imply_Right) {
        // A <- A is always true.
        if (is_same_function(l, r)) return create_bool(true);

        // A <- B is equivalent to A | !B. So if B is true, it simplifies to true; if B is false, it simplifies to A.
        if (r.isBoolValue() && r.endpoint_value.value()) return create_bool(true);
        if (r.isBoolValue() && !r.endpoint_value.value()) return std::move(l);
    }

    // no rules matched, return a new logical node with simplified children.
    return create_logical(std::move(l), std::move(r), op);
}

bool condition_node::is_same_function(const condition_node &l, const condition_node &r) const
{
    return (l.isFunction() && r.isFunction() && l.endpoint_func->func_id == r.endpoint_func->func_id);
}

bool condition_node::is_same_tree(const condition_node &l, const condition_node &r) const
{
    if (l.type != r.type) return false; // type should be the same

    if (l.isEndpoint()) {
        if (l.endpoint->endpoint_type != r.endpoint->endpoint_type) return false;
        if (l.isBoolValue()) return l.endpoint_value.value() == r.endpoint_value.value();
        if (l.isFunction())  return l.endpoint_func->func_id == r.endpoint_func->func_id;
        throw std::runtime_error("Unknown endpoint type in condition node");
    } else if (l.isLogical()) {
        if (l.logical->op != r.logical->op) return false; // operator should be the same
        // check left child
        if(!l.logical->left && !r.logical->left) throw std::runtime_error("Invalid logical node with missing left child");
        if(!l.logical->left || !r.logical->left) return false; // one

        // unary operator only check left child.
        if (__lo_is_not_only(l.logical->op)) {
            return is_same_tree(*l.logical->left, *r.logical->left);

        } else {
            if(!l.logical->right && !r.logical->right) throw std::runtime_error("Invalid logical node with missing right child");
            if(!l.logical->right || !r.logical->right) return false; // one is missing right child while the other is not, consider different
            
            if (__lo_is_imply(l.logical->op)) {
                // for imply operators, A -> B is not the same as B -> A, so we do not consider them the same even if they are swapped.
                return is_same_tree(*l.logical->left, *r.logical->left) && is_same_tree(*l.logical->right, *r.logical->right);
            }

            return (
                (is_same_tree(*l.logical->left, *r.logical->left ) && is_same_tree(*l.logical->right, *r.logical->right)) ||
                (is_same_tree(*l.logical->left, *r.logical->right) && is_same_tree(*l.logical->right, *r.logical->left ))
            );
        }
    } else { // null nodes are considered the same.
        return true;
    }
}

bool condition_node::is_negation_of(const condition_node &a, const condition_node &b) const
{
    // a == !b
    if (a.isLogical() && a.logical->op == logical_operator::Not) {
        return is_same_tree(*a.logical->left, b);
    }
    // b == !a
    if (b.isLogical() && b.logical->op == logical_operator::Not) {
        return is_same_tree(a, *b.logical->left);
    }
    return false;
}

// void condition_node::collect_operands(logical_operator op, std::vector<const condition_node*> &operands) const
// {
//     if (type == Type::Logical && logical->op == op) {
//         logical->left->collect_operands(op, operands);
//         if (logical->right) {
//             logical->right->collect_operands(op, operands);
//         }
//     } else {
//         operands.push_back(this); // 遇到不同算子或 Endpoint，停止展平
//     }
// }

int condition_node::setFunction(const std::string &func_id, condition_function_t func)
{
    if(isFunction()) {
        if(endpoint_func->func_id == func_id) {
            endpoint_func->func = func;
            return 1; // function set successfully
        } else {
            return 0; // Not a function endpoint.
        }
    }
    if (type == Type::Logical) {
        // try to set function on child nodes if possible.
        if(!logical || !logical->left) {
            throw std::runtime_error("Trying to set function on an invalid logical node");
        }
        int left_set = logical->left->setFunction(func_id, func);
        int right_set = 0;
        if(logical->right) {
            right_set = logical->right->setFunction(func_id, func);
        }
        return left_set + right_set; // tell the parent node how many functions have been set in the subtree.
    }
    return 0; // Not even a valid node.
}

std::vector<std::string> condition_node::getFunctions() const
{
    auto result = getFunctions_internal();
    // remove duplicates.
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<std::string> condition_node::getFunctions_internal() const
{
    if(isFunction()) {
        return { endpoint_func->func_id };
    } else if (isLogical()) {
        // should contains functions in the subtree of this node.
        std::vector<std::string> subtree_funcs;
        if(logical->left) {
            auto left_funcs = logical->left->getFunctions_internal();
            subtree_funcs.insert(subtree_funcs.end(), left_funcs.begin(), left_funcs.end());
        }
        if(logical->right) {
            auto right_funcs = logical->right->getFunctions_internal();
            subtree_funcs.insert(subtree_funcs.end(), right_funcs.begin(), right_funcs.end());
        }
        return subtree_funcs;
    } else {
        return {}; // Null nodes and endpoint nodes without function type do not contain any functions.
    }
}

condition_node condition_node::load(const std::bytearray_view &data)
{
    condition_node node;

    if(data.available(sizeof(Type))) {
        node.type = data.read<Type>();

        if(node.type == Type::Null) {
            // no payload, nothing to read
        } else if(node.type == Type::Endpoint) {
            if(!data.available(sizeof(EndpointType))) {
                throw std::runtime_error("Not enough data to load endpoint node");
            }
            EndpointType endpoint_type = data.read<EndpointType>();
            node.endpoint = { endpoint_type };

            if(endpoint_type == EndpointType::BoolValue) {
                if(!data.available(sizeof(bool))) {
                    throw std::runtime_error("Not enough data to load bool value endpoint");
                }
                node.endpoint_value = data.read<bool>();
            } else if(endpoint_type == EndpointType::Function) {
                // read the function id string.
                // keep func empty for now. The value should be set later.
                node.endpoint_func = { nullptr, data.readString() };
            } else {
                throw std::runtime_error("Unknown endpoint type in condition node");
            }

        } else if(node.type == Type::Logical) {
            if(!data.available(sizeof(logical_operator))) {
                throw std::runtime_error("Not enough data to load logical node operator");
            }
            logical_operator op = data.read<logical_operator>();

            node.logical = std::make_unique<_logical>();
            node.logical->op = op;

            // Load left child node recursively
            node.logical->left = std::make_unique<condition_node>(load(data));

            // If this is not a NOT-only operator, we also need to load the right child node.
            if(!__lo_is_not_only(op)) {
                node.logical->right = std::make_unique<condition_node>(load(data));
            }
        } else {
            throw std::runtime_error("Unknown node type in condition node");
        }
    } else {
        throw std::runtime_error("Not enough data to load condition node");
    }

    return node;
}

condition_node condition_node::create_null()
{
    return condition_node();
}

condition_node condition_node::create_bool(bool value)
{
    condition_node node;
    node.type = Type::Endpoint;
    node.endpoint = { EndpointType::BoolValue };
    node.endpoint_value = value;
    return node;
}

condition_node condition_node::create_function(const std::string& func_id)
{
    condition_node node;
    node.type = Type::Endpoint;
    node.endpoint = { EndpointType::Function };
    node.endpoint_func = { nullptr, func_id };
    return node;
}

condition_node condition_node::create_logical(condition_node&& left, condition_node&& right, logical_operator op)
{
    condition_node node;
    node.type = Type::Logical;
    node.logical = std::make_unique<_logical>();
    node.logical->op = op;
    node.logical->left = std::make_unique<condition_node>(std::move(left));
    node.logical->right = std::make_unique<condition_node>(std::move(right));
    return node;
}

condition_node condition_node::create_unary(condition_node&& child, logical_operator op)
{
    condition_node node;
    node.type = Type::Logical;
    node.logical = std::make_unique<_logical>();
    node.logical->op = op;
    node.logical->left = std::make_unique<condition_node>(std::move(child));
    node.logical->right = nullptr; // unary operator, no right child
    return node;

}


std::bytearray condition_node::dump(const condition_node &node)
{
    std::bytearray data;

    if(!node.valid()) {
        throw std::runtime_error("Trying to dump an invalid condition node");
    }

    data.append(node.type);

    if(node.type == Type::Null) {
        // no payload at all
    } else if(node.type == Type::Endpoint) {
        if(!node.endpoint.has_value()) {
            throw std::runtime_error("Endpoint node missing endpoint data");
        }
        data.append(node.endpoint->endpoint_type);

        if(node.endpoint->endpoint_type == EndpointType::BoolValue) {
            if(!node.endpoint_value.has_value()) {
                throw std::runtime_error("BoolValue endpoint missing value");
            }
            data.append(node.endpoint_value.value());
        } else if(node.endpoint->endpoint_type == EndpointType::Function) {
            // for function types, we keep their id.
            if(!node.endpoint_func.has_value()) {
                throw std::runtime_error("Function endpoint missing function data");
            }
            data.addString(node.endpoint_func->func_id);
        } else {
            throw std::runtime_error("Unknown endpoint type");
        }
    } else if(node.type == Type::Logical) {
        if(!node.logical) {
            throw std::runtime_error("Logical node missing operator data");
        }
        data.append(node.logical->op);

        // dump the left and right node recursively.
        data.append(dump(*node.logical->left));
        if(!__lo_is_not_only(node.logical->op)) {
            data.append(dump(*node.logical->right));
        }

    } else {
        throw std::runtime_error("Unknown node type");
    }

    return data;
}

// Tokenizer implementation for condition_expression parsing logic.
// This is not a separate module since it is too specific, and will
// be to costly to generalize.
class Tokenizer {
public:
    enum class TokenType {
        EndOfInput,
        LeftParen, RightParen, // ( )
        OperatorNot, // !
        OperatorAnd, OperatorOr, OperatorXor, // & | ^
        OperatorNand, OperatorNor, OperatorXnor, // !& !| !^
        OperatorImplyLeft, OperatorImplyRight, // -> <-
        Identifier, // [A-Za-z_0-9]+
        BoolLiteral, // {true} {false}
        Error
    };

    struct Token {
        TokenType type;
        std::string value;
    };

    explicit Tokenizer(const std::string& input) : source(input), pos(0) {}

    Token nextToken();

    inline std::generator<Token> tokenize() {
        Token token;
        while ((token = nextToken()).type != TokenType::EndOfInput) {
            co_yield token;
        }
    }

    static const std::map<TokenType, int> priority;

private:
    void skipWhitespace();
    bool isAlphaNum(char c) const;

private:
    std::string source;
    size_t pos;
};


// The second layer of the parser.
class ConditionParser {
public:
    using TokenType = Tokenizer::TokenType;
    using Token = Tokenizer::Token;

    explicit ConditionParser(const std::string& input);

    condition_node parse();

private:
    // Atomic / Parenthesized expressions -> NOT -> AND/NAND -> /OR/XOR/NOR/NXOR -> IMPLY
    condition_node parsePrimary(); // parse basic units: identifiers, literals, parenthesized expressions
    // Not is also parsed in parsePrimary.
    condition_node parseAnd(); // parse AND/NAND operators
    condition_node parseOrXor(); // parse OR, XOR, NOR, NXOR operators
    condition_node parseImply(); // parse implication operators, which have the lowest precedence

    // helper functions
    logical_operator maptoLogicalOp(TokenType type) const;

    inline bool isOrXor(TokenType type) const {
        return type == TokenType::OperatorOr || type == TokenType::OperatorXor ||
               type == TokenType::OperatorNor || type == TokenType::OperatorXnor;
    }

    void advance();

private:
    Tokenizer tokenizer;
    Token currentToken;
};

condition_node condition_node::parse(const std::string &str)
{
    ConditionParser cp (str);
    return cp.parse();
}

std::string condition_node::to_string(const condition_node &node)
{
    if(node.isNull()) return "{null}";

    if(node.isEndpoint()) {
        if(node.isBoolValue()) {
            return node.endpoint_value.value() ? "{true}" : "{false}";
        } else if(node.isFunction()) {
            return node.endpoint_func->func_id;
        } else {
            throw std::runtime_error("Unknown endpoint type in condition node");
        }
    }

    if(node.isLogical()) {
        if (__lo_is_not_only(node.logical->op)) {
            // 格式示例: !(A)
            return "!(" + to_string(*node.logical->left) + ")";
        }

        // 处理二元运算符
        std::string op_str;
        switch (node.logical->op) {
            case logical_operator::And:         op_str = " & ";  break;
            case logical_operator::Or:          op_str = " | ";  break;
            case logical_operator::Xor:         op_str = " ^ ";  break;
            case logical_operator::Nand:        op_str = " !& "; break;
            case logical_operator::Nor:         op_str = " !| "; break;
            case logical_operator::Xnor:        op_str = " !^ "; break;
            case logical_operator::Imply_Left:  op_str = " <- "; break;
            case logical_operator::Imply_Right: op_str = " -> "; break;
            default: op_str = " ? "; break;
        }

        // 为了绝对安全，给二元运算两侧加上括号
        // 这样可以避免优先级引起的歧义，例如 (A & B) | C
        return "(" + to_string(*node.logical->left) + op_str + to_string(*node.logical->right) + ")";
    }

    return "{error}";
}

Tokenizer::Token Tokenizer::nextToken()
{
    skipWhitespace();

    if(pos >= source.size()) {
        return { TokenType::EndOfInput, {} };
    }

    char current = source[pos];

    // handle parentheses
    if(current == '(') { pos++; return { TokenType::LeftParen,  "(" }; }
    if(current == ')') { pos++; return { TokenType::RightParen, ")" }; }

    // handle constants
    if(current == '{') {
        size_t startp = ++pos;
        while (pos < source.size() && source[pos] != '}') pos++;
        std::string literal = source.substr(startp, pos - startp);
        if (pos < source.size() && source[pos] == '}') {
            pos++; // skip "}"
            if (literal == "true" || literal == "false" || literal == "1" || literal == "0") {
                return { TokenType::BoolLiteral, literal };
            } else {
                return { TokenType::Error, source.substr(startp - 1, pos - startp + 1) }; // include the braces in error
            }
        }
    }

    // handle not or not-like operators
    if (current == '!') {
        pos++;
        if(pos < source.size()) {
            if(source[pos] == '&') { pos++; return { TokenType::OperatorNand, "!&" }; }
            if(source[pos] == '|') { pos++; return { TokenType::OperatorNor,  "!|" }; }
            if(source[pos] == '^') { pos++; return { TokenType::OperatorXnor, "!^" }; }
        }
        return { TokenType::OperatorNot, "!" }; // otherwise, just a single not operator
    }

    // handle normal operators
    if (current == '&') { pos++; return { TokenType::OperatorAnd, "&" }; }
    if (current == '|') { pos++; return { TokenType::OperatorOr,  "|" }; }
    if (current == '^') { pos++; return { TokenType::OperatorXor, "^" }; }

    // handle imply operators
    if (current == '-') {
        pos++;
        if(pos < source.size() && source[pos] == '>') {
            pos++;
            return { TokenType::OperatorImplyRight, "->" };
        }
        return { TokenType::Error, "-" };
    }

    if (current == '<') {
        pos++;
        if(pos < source.size() && source[pos] == '-') {
            pos++;
            return { TokenType::OperatorImplyLeft, "<-" };
        }
        return { TokenType::Error, "<" };
    }

    // Handle identifiers
    if(isAlphaNum(current)) {
        size_t startp = pos;
        while (pos < source.size() && isAlphaNum(source[pos])) pos++;
        return { TokenType::Identifier, source.substr(startp, pos - startp) };
    }

    return { TokenType::Error, source.substr(pos) };
}

void Tokenizer::skipWhitespace()
{
    while(pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos]))) pos++;
}

bool Tokenizer::isAlphaNum(char c) const
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

ConditionParser::ConditionParser(const std::string &input)
    : tokenizer(input)
{
    advance(); // pre-read the first token
}

condition_node ConditionParser::parse()
{
    return parseImply(); // start from the lowest precedence operator
}

condition_node ConditionParser::parsePrimary()
{
    if(currentToken.type == TokenType::OperatorNot) {
        advance();
        auto child = parsePrimary(); // recursively handle !(!A)
        return condition_node::create_unary(std::move(child), logical_operator::Not); // treat parentheses as a NOT operator with only one child, which will be simplified later.
    }
    if(currentToken.type == TokenType::LeftParen) {
        advance();
        auto node = parseImply(); // parse the expression inside the parentheses
        if(currentToken.type != TokenType::RightParen) {
            throw std::runtime_error("Expected ')' after expression in parentheses");
        }
        advance(); // consume the right parenthesis
        return node;
    }
    if(currentToken.type == TokenType::Identifier) {
        std::string func_id(currentToken.value);
        advance();
        return condition_node::create_function(func_id);
    }
    if(currentToken.type == TokenType::BoolLiteral) {
        bool value = (currentToken.value == "true" || currentToken.value == "1");
        advance();
        return condition_node::create_bool(value);
    } else {
        throw std::runtime_error("Unexpected token: " + std::string(currentToken.value));
    }
}

condition_node ConditionParser::parseAnd()
{
    auto left = parsePrimary();
    while(currentToken.type == TokenType::OperatorAnd || currentToken.type == TokenType::OperatorNand) {
        auto op = currentToken;
        advance();
        auto right = parsePrimary();
        left = condition_node::create_logical(std::move(left), std::move(right), maptoLogicalOp(op.type));
    }
    return left;
}

condition_node ConditionParser::parseOrXor()
{
    auto left = parseAnd();
    while(isOrXor(currentToken.type)) {
        auto op = currentToken;
        advance();
        auto right = parseAnd();
        left = condition_node::create_logical(std::move(left), std::move(right), maptoLogicalOp(op.type));
    }
    return left;
}

condition_node ConditionParser::parseImply()
{
    auto left = parseOrXor();
    while(currentToken.type == Tokenizer::TokenType::OperatorImplyLeft || currentToken.type == Tokenizer::TokenType::OperatorImplyRight) {
        auto op = currentToken;
        advance();
        auto right = parseOrXor();
        left = condition_node::create_logical(std::move(left), std::move(right), maptoLogicalOp(op.type));
    }

    return left;
}

logical_operator ConditionParser::maptoLogicalOp(TokenType type) const
{
    switch(type) {
        case TokenType::OperatorNot: return logical_operator::Not;
        case TokenType::OperatorAnd: return logical_operator::And;
        case TokenType::OperatorOr: return logical_operator::Or;
        case TokenType::OperatorXor: return logical_operator::Xor;
        case TokenType::OperatorNand: return logical_operator::Nand;
        case TokenType::OperatorNor: return logical_operator::Nor;
        case TokenType::OperatorXnor: return logical_operator::Xnor;
        case TokenType::OperatorImplyLeft: return logical_operator::Imply_Left;
        case TokenType::OperatorImplyRight: return logical_operator::Imply_Right;
        default:
            throw std::runtime_error("Invalid token type for logical operator");
    }
}

void ConditionParser::advance()
{
    currentToken = tokenizer.nextToken();
}

void generate_truth_table(condition_node &root, bool colored)
{
    // collect function endpoints
    auto func_ids = root.getFunctions();
    size_t num_funcs = func_ids.size();

    // generate all combinations of function values
    std::vector<bool> values(num_funcs, false);

    // helper function: advance to the next combination of function values.
    auto advance = [&values]() {
        for(size_t i = 0; i < values.size(); i++) {
            values[i] = !values[i];
            if(values[i]) break; // if this bit is now true, we can stop flipping
        }
    };

    // inject functions.
    for(size_t i = 0; i < num_funcs; i++) {
        root.setFunction(func_ids[i], [i, &values]() {
            return values[i];
        });
    }

    // print header
    // format:
    // A | B | C | R
    for(const auto& func_id : func_ids) {
        std::cout << func_id << " | ";
    }
    std::cout << "R" << std::endl;
    std::cout << std::string(func_ids.size() * 4 + 2, '-') << std::endl; // separator line

    // generate truth table (using 1 or 0 to represent true or false)
    size_t num_combinations = 1ULL << num_funcs; // 2^n combinations
    for(size_t i = 0; i < num_combinations; i++) {
        // print current combination of function values
        for(bool value : values) {
            std::cout << std::coloredtext(value ? "1" : "0", value ? std::tGreen : std::tRed) << " | ";;
        }
        // print the result of evaluating the condition with the current function values
        bool eval = root.evaluate();
        std::cout << std::coloredtext(eval ? "1" : "0", eval ? std::tGreen : std::tRed) << std::endl;

        advance(); // move to the next combination
    }
}

condition_expression condition_simplifier::simplify(const condition_expression &cexpr){

    auto expr = cexpr.deep_copy();

    // Step 1: Group terms by the number of true inputs (number of 1s in the value).
    auto [groups, minterms] = generate_table(expr);

    // Step 2: Iteratively combine terms until no more combinations are possible.
    // Generated terms with used=false are the prime implicants, which cannot be combined further.
    // compare terms of adjacant groups (groups with n and n+1 true inputs)
    // to find pairs of terms that differ by only one bit, and combine them
    // into a new term with a "don't care" bit.

    // Step 3: find essential prime implicants and construct the simplified expression.
    // Essential prime implicants are those that cover an output of 1 that no other prime
    // implicant covers. We can find them by looking at the original terms (those with
    // used=false and no don't care bits) and seeing which ones are covered by only one prime implicant.

    std::vector<term> prime_implicants;

    {
        bool changed = true;
        auto table = make_plain(groups); // flatten the groups into a single table for easier processing.
        
        while(!table.empty()) {
            changed = false;
            std::vector<term> nextgen;
            for(size_t i = 0; i < table.size(); i++) {
                for(size_t j = i + 1; j < table.size(); j++) {
                    auto& term1 = table[i];
                    auto& term2 = table[j];

                    // check if they differ by only one bit
                    if (term1.mask == term2.mask && bits::is_adjacent(term1.value, term2.value)) { // check if diff is a power of 2, meaning only one bit is different
                        // combine the two terms into a new term with a "don't care" bit
                        term new_term;
                        new_term.mask = term1.mask & ~(term1.value ^ term2.value); // the differing bit becomes a don't care (mask bit set to 0)
                        new_term.value = term1.value & new_term.mask; // the value of the new term is the same as term1 and term2 on the non-don't-care bits
                        new_term.used = false;

                        // mark the original terms as used
                        term1.used = true;
                        term2.used = true;

                        changed = true; // mark that we have made a change in this iteration

                        // add the new term to the new table
                        if(std::none_of(nextgen.begin(), nextgen.end(), [&](const term& t) { return t == new_term; })) {
                            // only add if it's not already in the new table
                            nextgen.push_back(new_term);
                        }
                    }
                }
            }

            for (const auto& t : table) {
                if (!t.used) {
                    prime_implicants.push_back(t);
                }
            }

            table = std::move(nextgen); // move to the next generation of terms
            if(!changed) break; // if no new combinations were made, we are done
        }
    }

    std::map<term::mtvar_t, std::vector<size_t>> cover_chart;

    for (auto mt_val : minterms) {
        for (size_t i = 0; i < prime_implicants.size(); ++i) {
            if ((mt_val & prime_implicants[i].mask) == prime_implicants[i].value) {
                cover_chart[mt_val].push_back(i);
            }
        }
    }

    // Select minimum cover of prime implicants using a greedy approach.
    std::set<size_t> chosen_pi_indices;
    std::set<term::mtvar_t> remaining_minterms(minterms.begin(), minterms.end());

    for (auto const& [mt, pi_indices] : cover_chart) {
        if (pi_indices.size() == 1) {
            size_t idx = pi_indices[0];
            chosen_pi_indices.insert(idx);
            // 标记已覆盖的最小项
            for(auto m : minterms) {
                if((m & prime_implicants[idx].mask) == prime_implicants[idx].value) {
                    remaining_minterms.erase(m);
                }
            }
        }
    }

    while(!remaining_minterms.empty()) {
        // 找到能覆盖最多剩余项的 PI
        size_t best_pi = 0;
        size_t max_cover = 0;
        for(size_t i = 0; i < prime_implicants.size(); ++i) {
            size_t count = 0;
            for(auto m : remaining_minterms) {
                if((m & prime_implicants[i].mask) == prime_implicants[i].value) count++;
            }
            if(count > max_cover) {
                max_cover = count;
                best_pi = i;
            }
        }
        if(max_cover == 0) {
            // just in case
            throw std::runtime_error("No prime implicant can cover remaining minterms, something went wrong.");
        }
        chosen_pi_indices.insert(best_pi);
        // 移除新覆盖的项
        for(auto it = remaining_minterms.begin(); it != remaining_minterms.end(); ) {
            if((*it & prime_implicants[best_pi].mask) == prime_implicants[best_pi].value) {
                it = remaining_minterms.erase(it);
            } else {
                ++it;
            }
        }
    }

    return reconstruct_tree(chosen_pi_indices, prime_implicants, expr.getFunctions());
}

std::tuple<std::map<int, std::vector<condition_simplifier::term>>, std::vector<condition_simplifier::term::mtvar_t>> condition_simplifier::generate_table(condition_expression &expr)
{
    // std::vector<term> table;
    std::map<int, std::vector<term>> groups;
    std::vector<term::mtvar_t> minterm_values;

    auto func_ids = expr.getFunctions();
    size_t num_funcs = func_ids.size();

    if(num_funcs > sizeof(term::mtvar_t) * 8) {
        throw std::runtime_error("Too many functions in the expression to generate truth table");
    }

    std::bitset<sizeof(term::mtvar_t)*8> values;

    // inject functions
    for(size_t i = 0; i < num_funcs; i++) {
        expr.setFunction(func_ids[i], [i, &values]() -> bool {
            return values[i];
        });
    }

    // generate truth table (using 1 or 0 to represent true or false)
    size_t num_combinations = 1ULL << num_funcs; // 2^n combinations
    for(size_t i = 0; i < num_combinations; i++) {
        if (expr.evaluate()) {
            term t;
            t.mask = ~static_cast<term::mtvar_t>(0); // All bits are relevant at the beginning.
            t.value = static_cast<term::mtvar_t>(values.to_ulong()); // the value of the term is determined by the current combination of function values.
            minterm_values.push_back(t.value);
            t.used = false; // not used yet.

            groups[static_cast<int>(values.count())].push_back(t);
        }

        // move to next combination
        for(size_t j = 0; j < values.size(); j++) {
            values[j] = !values[j];
            if(values[j]) break; // if this bit is now true, we can stop flipping
        }
    }

    return std::make_tuple(std::move(groups), std::move(minterm_values));
}

std::vector<condition_simplifier::term> condition_simplifier::make_plain(const std::map<int, std::vector<term>> &groups)
{
    std::vector<term> table;
    for (auto& [count, term_list] : groups) {
        table.insert(table.end(), term_list.begin(), term_list.end());
    }
    return table;
}

condition_expression condition_simplifier::reconstruct_tree(const std::set<size_t> &indices, const std::vector<term> &pis, const std::vector<std::string> &func_ids)
{
    if(indices.empty()) return condition_node::create_bool(false);

    std::vector<condition_node> or_terms;

    for(size_t idx : indices) {
        const auto& t = pis[idx];
        std::vector<condition_node> and_terms;

        for(size_t i = 0; i < func_ids.size(); ++i) {
            // 如果 mask 的第 i 位为 1，说明该变量参与了运算
            if(t.mask & (static_cast<term::mtvar_t>(1) << i)) {
                auto func_node = condition_node::create_function(func_ids[i]);
                // 如果 value 的第 i 位为 0，说明是取反项 (!A)
                if(!(t.value & (static_cast<term::mtvar_t>(1) << i))) {
                    and_terms.push_back(condition_node::create_unary(std::move(func_node), logical_operator::Not));
                } else {
                    and_terms.push_back(std::move(func_node));
                }
            }
        }

        // 将 and_terms 用 AND 连接
        if(and_terms.empty()) {
            or_terms.push_back(condition_node::create_bool(true));
        } else {
            condition_node res = std::move(and_terms[0]);
            for(size_t k = 1; k < and_terms.size(); ++k) {
                res = condition_node::create_logical(std::move(res), std::move(and_terms[k]), logical_operator::And);
            }
            or_terms.push_back(std::move(res));
        }
    }

    // 将 or_terms 用 OR 连接
    condition_node final_root = std::move(or_terms[0]);
    for(size_t k = 1; k < or_terms.size(); ++k) {
        final_root = condition_node::create_logical(std::move(final_root), std::move(or_terms[k]), logical_operator::Or);
    }

    return final_root;
}

} // namespace scl2