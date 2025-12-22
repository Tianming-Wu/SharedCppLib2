#include "condition.hpp"

#include <stdexcept>
#include <strstream>

namespace condition {

condition_item build_const_value_item(const std::string& name, bool val) {
    condition_item item(name, TYPE_BOOLVALUE);
    item.setValue(val);
    return item;
}

const condition_item condition_false = build_const_value_item("false", false),
                     condition_true = build_const_value_item("true", true);


condition_node::condition_node(condition_item_type type)
    : m_type(type)
{}

condition_item &condition_node::asItem() {
    return *dynamic_cast<condition_item*>(this);
}

condition_pair &condition_node::asPair() {
    return *dynamic_cast<condition_pair*>(this);
}

condition_item::condition_item(const std::string &name, condition_item_type type)
    : condition_node(type), m_name(name)
{
    switch(type) {
    case TYPE_BOOLVALUE: case TYPE_FUNCTION: case TYPE_TIMER:
        break;
    default:
        throw std::runtime_error("Initializing condition item with invalid type");
    }
}

condition_item::~condition_item()
{}

const std::string &condition_item::name() const { return m_name; }
void condition_item::setInvert(bool invert) { m_invert = invert; }

void condition_item::setFunction(const std::function<bool()> &func)
{
    if(m_type != TYPE_FUNCTION) {
        throw std::runtime_error("Cannot set function on non-function condition item");
    }
    m_func = func;
}

void condition_item::setValue(bool value)
{
    if(m_type != TYPE_BOOLVALUE) {
        throw std::runtime_error("Cannot assign value to non-value condition item");
    }
    m_value = value;
}

#define _eval_with_invert(target) (m_invert)? !target : target

bool condition_item::evaluate() const {
    try {
        switch(m_type) {
        case TYPE_BOOLVALUE: return _eval_with_invert(m_value);
        case TYPE_FUNCTION:  return _eval_with_invert(m_func());
        }

        throw std::runtime_error("Trying to evaluate value of an invalid condition item");
    } catch(const std::runtime_error& e) {
        throw e; // passthrough
    } catch(...) {
        return false;
    }
}

condition_pair::condition_pair(condition_node *left, condition_node *right, const std::string &op)
    : condition_node(TYPE_PAIR), left(left), right(right)
{
    if(left == nullptr || right == nullptr) {
        throw std::invalid_argument("condition_pair: left and right must be valid object");
    }
}

condition_pair::~condition_pair()
{
}

bool condition_pair::evaluate() const
{
    bool r_left = left->evaluate(), r_right = right->evaluate();

    switch(m_logic) {
    case LOGIC_AND:     return r_left && r_right;       // only true if both are true
    case LOGIC_OR:      return r_left || r_right;       // is true if any is true
    case LOGIC_XOR:     return r_left != r_right;       // is true if is different
    case LOGIC_NAND:    return !(r_left && r_right);    
    case LOGIC_NOR:     return !(r_left || r_right);
    case LOGIC_XNOR:    return r_left == r_right;       // is true if is the same
    case IMPLY_LEFT:    return false;                   // not supported
    case IMPLY_RIGHT:   return false;                   // not supported
    }

    throw std::runtime_error("Trying to evaluate value of an invalid condition pair type");
}

void condition_pair::simplify()
{
    if(left->type() == TYPE_PAIR) left->asPair().simplify();
    if(right->type() == TYPE_PAIR) right->asPair().simplify();

    ///TODO: complete simplify logic...
}

std::istream &operator>>(std::istream &is, condition_expression &expression)
{
    std::string buffer;
    if(std::getline(is, buffer)) {

    }
    return is;
}

std::ostream &operator<<(std::ostream &os, condition_expression &expression)
{
    
    return os;
}

} // namespace condition