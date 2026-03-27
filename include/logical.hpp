/*
    Logical module for SharedCppLib2.
    Tianming <github.com/Tianming-Wu> 2026.03.27
*/

#pragma once

#include <cstdint>

#include "enum.hpp"

namespace scl2 {

// Well this is kind of a "bit" enum but not completely.

// the highest bit is used for not, and the rest of the bits are the actual operator.

enum class logical_operator : uint8_t 
{
    Null = 0,
    Not = 1 << 7,
    And = 1,
    Or = 2,
    Xor = 3,
    Nand = And | Not,
    Nor = Or | Not,
    Xnor = Xor | Not,
    Imply_Left = 4, // left implies right
    Imply_Right = 5, // right implies left
};

using LogicalOperator = logical_operator; // In case user wants this style of naming.

scl2_bitenum_op(logical_operator)

// detect if the operator is a Not operator (reversed operator)
inline constexpr bool __lo_is_not(logical_operator op) {
    return (op & logical_operator::Not) != logical_operator::Null;
}

inline constexpr bool __lo_is_not_only(logical_operator op) {
    return op == logical_operator::Not;
}

// extract the base operator without the Not bit.
inline constexpr logical_operator __lo_base_op(logical_operator op) {
    return op & static_cast<logical_operator>(~logical_operator::Not);
}

inline constexpr bool __lo_eval_base(logical_operator op, bool left, bool right) {
    switch(op) {
        case logical_operator::And:
            return left && right;
        case logical_operator::Or:
            return left || right;
        case logical_operator::Xor:
            return left != right;
        case logical_operator::Imply_Left:
            return !left || right; // left implies right is equivalent to !left || right
        case logical_operator::Imply_Right:
            return !right || left; // right implies left is equivalent to !right || left
        default:
            throw std::runtime_error("Invalid logical operator");
    }
}

inline constexpr bool lo_eval(logical_operator op, bool left, bool right) {
    if(__lo_is_not_only(op)) {
        return !left; // for Not operator, we only care about the left operand, and return its negation.
    } else {
        bool result = __lo_eval_base(__lo_base_op(op), left, right);
        if(__lo_is_not(op)) {
            return !result; // if it's a reversed operator, return the negation of the base operator result.
        } else {
            return result;
        }
    }
}


} // namespace scl2