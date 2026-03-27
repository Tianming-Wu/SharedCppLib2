#include "condition.hpp"

/*
    ///TODO: Support the dirty flag.
*/

namespace scl2 {

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
        if(!logical.has_value()) {
            return false; // missing logical operator data
        }

        if(__lo_is_not_only(logical->op)) {
            // only check left node
            return logical->left.valid();
        } else {
            // check both left and right node
            return logical->left.valid() && logical->right.valid();
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
        if(!logical.has_value()) {
            return false; // missing logical operator data
        }

        if(__lo_is_not_only(logical->op)) {
            // only check left node
            return logical->left.strong_valid();
        } else {
            // check both left and right node
            return logical->left.strong_valid() && logical->right.strong_valid();
        }
    } else {
        return false; // unknown node type
    }
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
    return type == Type::Logical && logical.has_value();
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
        bool r_left = logical->left.evaluate();

        if(__lo_is_not_only(logical->op)) {
            // only evaluate left node
            return lo_eval(logical->op, r_left, false);
        } else {
            // also need to evaluate right node
            bool r_right = logical->right.evaluate();
            return lo_eval(logical->op, r_left, r_right);
        }
    } else {
        // Yes, null nodes are considered valid, but they cannot be evaluated.

        throw std::runtime_error("Trying to evaluate an invalid condition node");
    }
}

condition_node condition_node::simplify() const
{
    condition_node simplified_node = *this; // start with a copy of the current node
}

bool condition_node::setFunction(const std::string &func_id, condition_function_t func)
{
    if(!valid()) {
        throw std::runtime_error("Trying to set function on an invalid condition node");
    }

    if(type == Type::Endpoint) {
        if(endpoint->endpoint_type == EndpointType::Function) {
            endpoint_func = { func, func_id };
            return true;
        } else {
            throw std::runtime_error("Trying to set function on a non-function endpoint");
        }
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

            node.logical = { condition_node(), condition_node(), op };

            // Load left child node recursively
            node.logical->left = load(data);

            // If this is not a NOT-only operator, we also need to load the right child node.
            if(!__lo_is_not_only(op)) {
                node.logical->right = load(data);
            }
        } else {
            throw std::runtime_error("Unknown node type in condition node");
        }
    } else {
        throw std::runtime_error("Not enough data to load condition node");
    }

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
        if(!node.logical.has_value()) {
            throw std::runtime_error("Logical node missing operator data");
        }
        data.append(node.logical->op);

        // dump the left and right node recursively.
        data.append(dump(node.logical->left));
        if(!__lo_is_not_only(node.logical->op)) {
            data.append(dump(node.logical->right));
        }

    } else {
        throw std::runtime_error("Unknown node type");
    }

    return data;
}

} // namespace scl2