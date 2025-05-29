/*
    High Precision Array (hparray)

    namespace: none
    classes: hpintarray, hpfloatarray*, hpfracarray*
*/

#pragma once
#include "hpint.hpp"
#include "stringlist.hpp"

class hpintarray : public std::vector<hpint>
{
public:
    hpintarray() {}
    hpintarray(int size, const hpint& value) : std::vector<hpint>(size, value) {}
    hpintarray(const std::initializer_list<hpint>& init) : std::vector<hpint>(init) {}
    hpintarray(const std::vector<hpint>& vec) : std::vector<hpint>(vec) {}


    hpint max() const {
        if (empty()) throw std::runtime_error("hpintarray::max: empty array");
        hpint maxValue = at(0);
        for (const hpint& value : *this) {
            if (value > maxValue) {
                maxValue = value;
            }
        }
        return maxValue;
    }

    hpint min() const {
        if (empty()) throw std::runtime_error("hpintarray::min: empty array");
        hpint minValue = at(0);
        for (const hpint& value : *this) {
            if (value < minValue) {
                minValue = value;
            }
        }
        return minValue;
    }
    
    hpint sum() const {
        hpint total(0);
        for (const hpint& value : *this) {
            total += value;
        }
        return total;
    }

    hpint average() const {
        if (empty()) throw std::runtime_error("hpintarray::average: empty array");
        hpint total = sum();
        return total / static_cast<int>(size());
    }

    hpint median() const {
        if (empty()) throw std::runtime_error("hpintarray::median: empty array");
        hpintarray sortedArray = *this;
        std::sort(sortedArray.begin(), sortedArray.end());
        size_t mid = size() / 2;
        if (size() % 2 == 0) {
            return (sortedArray[mid - 1] + sortedArray[mid]) / 2;
        } else {
            return sortedArray[mid];
        }
    }

    hpint product() const {
        hpint total(1);
        for (const hpint& value : *this) {
            total *= value;
        }
        return total;
    }

    hpintarray sort() const {
        hpintarray sortedArray = *this;
        std::sort(sortedArray.begin(), sortedArray.end());
        return sortedArray;
    }

    static hpintarray from_stringlist(const std::stringlist& strlist) {
        hpintarray result;
        for (const std::string& str : strlist) {
            hpint value = hpint::from_string(str);
            result.push_back(value);
        }
        return result;
    }

    /// @brief 
    /// @warning supported splits are: ",", " ", "\t", "\n" 
    static hpintarray from_string(const std::string& str) {
        hpintarray result;
        std::stringlist strlist(str, std::stringlist({","," ","\t","\n"}));
        strlist.remove_empty();
        for (const std::string& s : strlist) {
            hpint value = hpint::from_string(s);
            result.push_back(value);
        }
        return result;
    }

};