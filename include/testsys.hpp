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
        ++m_tr.total;
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
        return false;
    }

    inline bool expect_false(bool cond, const std::string& itemName, const std::string& error = "") {
        return expect_true(!cond, itemName, error);
    }

    inline bool expect_value(const auto& actual, const auto& expected, const std::string& itemName) {
        ++m_tr.total;
        if (actual == expected) {
            std::printf("[PASS] %s\n", itemName.c_str());
            ++m_tr.passes;
            return true;
        }

        std::printf("[FAIL] %s - expected: %s, actual: %s\n", itemName.c_str(), std::to_string(expected).c_str(), std::to_string(actual).c_str());
        return false;
    }

#ifdef MSVC_VERSION
    // This is a helper function to check if a memory block is uninitialized (0xCC in MSVC debug mode).
    // This can help detect memory management issues in the tested code.
    inline bool check_invalid_memory(void* memptr, size_t size, const std::string& itemName) {
        // on MSVC, check whether memory is 0xCC (-51). This is the pattern used by MSVC debug allocator for uninitialized memory.
        // This might report false positives if the tested code actually used 0xCC.
        // But at least you can be aware of that there might be a problem with memory management in the tested code.

        unsigned char* bytes = static_cast<unsigned char*>(memptr);
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] != 0xCC) {
                std::printf("[FAIL] %s - memory at %p is not invalid (0x%02X at offset %zu)\n", itemName.c_str(), memptr, bytes[i], i);
                return false;
            }
        }

        std::printf("[PASS] %s - memory at %p is invalid (0xCC)\n", itemName.c_str(), memptr);

        return true;
    }
#endif

};


// alias
using testsystem = test;


} // namespace scl2