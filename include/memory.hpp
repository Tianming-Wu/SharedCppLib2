#pragma once

/*
    Memory module for SharedCppLib2

    This is an advanced memory management module, which mostly focus on
    injection. That means reading/writing raw memory without knowing the types
    and features of the data stored there.

    It does not access other process memory! It's for something like dll injection,
    so it's still running in the same process space. We just don't know what the memory is.

    Note:
        This module is highly platform dependent, and currently only Windows is supported.
        Well SharedCppLib2 is mostly multiplatform, so I may have to deal with this later.
*/
#include "platform.hpp"

#ifdef OS_WINDOWS

#undef CopyMemory

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <optional>

#include "basics.hpp"
#include "bytearray.hpp"
#include "typemask.hpp"

namespace scl {

typedef int offset_t; // offsets should be signed integers, but which size?
using ptr_t = ::ptr_t;
using protect_t = ::dword_t;


template <typename T>
T fetch(ptr_t address) {
    return *reinterpret_cast<T*>(address);
}


struct MemoryRegion {
    ptr_t base;
    size_t size;
    protect_t protect;
};


/// @brief NestedPointer class for handling multi-level pointers.
class NestedPointer {
public:
    NestedPointer() = default;
    NestedPointer(ptr_t baseAddress, const std::vector<offset_t>& offsets);
    ~NestedPointer() = default;

    enable_copy_move(NestedPointer)

    ptr_t resolve(ptr_t processHandle) const;

    template <typename T>
    T resolve_as(ptr_t processHandle) const {
        return fetch<T>(resolve(processHandle));
    }

private:
    ptr_t baseAddress = nullptr;
    std::vector<offset_t> offsets;
};


/// @brief MemoryPattern class provide a pattern construction functionality for memory scanning.
/// Format: "AA BB ? CC ?? DD", where "?" or "??" is a wildcard byte.
class MemoryPattern {
public:
    MemoryPattern(const std::string& pattern);
    ~MemoryPattern() = default;

    enable_copy_move(MemoryPattern)

    constexpr size_t size() const;
    std::optional<byte_t> at(size_t index) const;
    std::optional<byte_t>& operator[](size_t index);

private:
    friend class PatternScanner;
    std::vector<std::optional<byte_t>> bytes;
    std::string mask;
};


// Usually if you get more than one result, you just effectively failed to create a valid pattern.
struct PatternScannerResult {
    bool valid; // will be false if got more than one result, or no result at all
    ptr_t address;

    inline operator bool() const { return valid; }

    template <typename T>
    explicit operator T() const {
        if (!valid) {
            throw std::runtime_error("PatternScannerResult: invalid result cannot be converted");
        }
        return fetch<T>(address);
    }
    
};


class PatternScanner {
public:
    static PatternScannerResult SearchMemoryRegion(MemoryRegion mrgn, MemoryPattern pattern);
    static PatternScannerResult SearchProcessMemory(MemoryPattern pattern);

protected:
    static PatternScannerResult __Search(ptr_t baseAddress, size_t regionSize, MemoryPattern pattern);
    static PatternScannerResult __FullSearch(ptr_t baseAddress, size_t regionSize, MemoryPattern pattern);

};


struct ValueScannerResult {
    std::vector<ptr_t> addresses;
};


class ValueScanner {
public:
    static ValueScannerResult SearchMemoryRegion(MemoryRegion mrgn, const std::bytearray &value);
    static ValueScannerResult SearchProcessMemory(const std::bytearray &value);

};


protect_t GainRWXPermission(ptr_t address, size_t size);
protect_t RestorePermission(ptr_t address, size_t size, protect_t oldProtect);


// These functions allow you to save a chunk of memory for later use.
// It is also possible to use this to dump? (pause first!)
std::bytearray CopyMemory(MemoryRegion mrgn);
void ApplyMemory(MemoryRegion mrgn, std::bytearray data);
std::bytearray ReplaceMemory(MemoryRegion mrgn, std::bytearray data);


/// @brief Helper functions.
namespace Helper {

// I don't even know if this is possible.
// But I think we need some way to pause the current process
// while scanning memory, to avoid memory being changed during scanning.
// void PauseCurrentProcess();
// void ResumeCurrentProcess();

};


} // namespace scl

#endif // OS_WINDOWS