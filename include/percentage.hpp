/*
    Percentage class for representing progress as a percentage value.

*/


#pragma once

#include <concepts>

using _Percentage_Default_Value_Type = int;
using _Percentage_Default_Percentage_Type = double;

template <typename _Value_Type = _Percentage_Default_Value_Type, typename _Percentage_Type = _Percentage_Default_Percentage_Type>
requires std::is_arithmetic_v<_Value_Type> && std::is_arithmetic_v<_Percentage_Type>
class percentage {
public:
    percentage() : m_value(0), m_min_value(0), m_max_value(100), m_percentage(0) {}
    percentage(const _Value_Type& value, const _Value_Type& min_value = 0, const _Value_Type& max_value = 100)
        : m_value(value), m_min_value(min_value), m_max_value(max_value) {
        update_percentage();
    }

    void setValue(const _Value_Type& value) {
        m_value = value;
        update_percentage();
    }

    _Percentage_Type getPercentage() const {
        return m_percentage;
    }

    // default percentage value to use when division by zero occurs.
    // For example, when min is equal to max.
    constexpr _Percentage_Type default_value_if_zdevz = 0;


protected:
    void update_percentage() {
        if (m_max_value == m_min_value) {
            m_percentage = default_value_if_zdevz; // Avoid division by zero, treat as 0% if min and max are the same
        } else {
            m_percentage = static_cast<_Percentage_Type>(m_value - m_min_value) / static_cast<_Percentage_Type>(m_max_value - m_min_value) * 100;
        }
    }

protected:
    _Value_Type m_value, m_min_value, m_max_value;
    _Percentage_Type m_percentage;
};
