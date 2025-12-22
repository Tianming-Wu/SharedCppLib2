#pragma once

#include <string>
#include <functional>

// inline bool throw_or_false(bool active, const std::string& msg) {
//     if(active) {
//         throw std::runtime_error(msg);
//     } else {
//         return false;
//     }
// }

namespace condition {

enum condition_item_type {
    TYPE_NODE,
    TYPE_BOOLVALUE, TYPE_FUNCTION, TYPE_TIMER,
    TYPE_PAIR
};

class condition_node;
class condition_item;
class condition_pair;

enum condition_pair_logic {
    LOGIC_AND, LOGIC_OR, LOGIC_XOR,
    LOGIC_NAND, LOGIC_NOR, LOGIC_XNOR,
    IMPLY_LEFT, IMPLY_RIGHT
};

class condition_node {
public:
    virtual bool evaluate() const = 0;

    condition_item& asItem();
    condition_pair& asPair();

    condition_item_type type() const;

protected:
    condition_node(condition_item_type type);
    virtual ~condition_node() = default;
    
    condition_item_type m_type;
};

class condition_item : public condition_node
{
public:
    condition_item(const std::string& name, condition_item_type type);
    ~condition_item();

    const std::string& name() const;
    bool evaluate() const override;

    void setInvert(bool invert);
    void setFunction(const std::function<bool()>& func);
    void setValue(bool value);

protected:
    std::string m_name;
    bool m_invert = false;

    std::function<bool()> m_func;
    bool m_value = false;
};


class condition_pair : public condition_node
{
public:
    condition_pair(condition_node* left, condition_node* right, const std::string& op);
    ~condition_pair();

    bool evaluate() const override;
    void simplify();

protected:
    condition_pair_logic m_logic;

    condition_node *left = nullptr, *right = nullptr;
};

using condition_expression = condition_pair;

extern const condition_item condition_false, condition_true;

std::istream& operator >> (std::istream& is, condition_expression& expression);
std::ostream& operator << (std::ostream& os, condition_expression& expression);

} // namespace condition