/*
    Testing System module for SharedCppLib2.
    Tianming-Wu 2026.03.19

    This module is header-only.

    This module is a simple testing system for unit tests. It is detached
    from the main library and can be used independently.

    It provides a simple interface for defining and running tests, and it will
    report the results in a simple format.

*/

#pragma once
#include <cstdio>
#include <string>

#include <functional>

namespace scl2 {

/*
    The conclusion of total test results.
*/
struct test_result {
    size_t passes, total;
};


class test {
    test_result m_tr;
public:
    test() : m_tr{0, 0} {}

    inline test_result result() const { return m_tr; }

    inline bool expect_true(bool cond, const std::string& itemName, const std::string& error = "") {
        if (cond) {
            std::printf("[PASS] %s\n", itemName.c_str());
            ++m_tr.passes;
            return true;
        }

        std::printf("[FAIL] %s", itemName.c_str());
        if (!error.empty()) {
            std::printf(" - %s", error.c_str());
        }
        std::printf("\n");
        ++m_tr.total;
        return false;
    }

    inline bool expect_false(bool cond, const std::string& itemName, const std::string& error = "") {
        return expect_true(!cond, itemName, error);
    }

    inline bool expect_value(const auto& actual, const auto& expected, const std::string& itemName) {
        if (actual == expected) {
            std::printf("[PASS] %s\n", itemName.c_str());
            ++m_tr.passes;
            return true;
        }

        std::printf("[FAIL] %s - expected: %s, actual: %s\n", itemName.c_str(), std::to_string(expected).c_str(), std::to_string(actual).c_str());
        ++m_tr.total;
        return false;
    }

};


// alias
using testsystem = test;


} // namespace scl2