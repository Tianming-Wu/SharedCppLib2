#include "memory.hpp"

#ifdef OS_WINDOWS

#include <windows.h>

// again, MACRO namespace pollution. and it's just memcpy.
#undef CopyMemory

scl::NestedPointer::NestedPointer(ptr_t baseAddress, const std::vector<offset_t> &offsets)
    : baseAddress(baseAddress), offsets(offsets)
{}

ptr_t scl::NestedPointer::resolve(ptr_t processHandle) const
{
    pbyte_t address = reinterpret_cast<pbyte_t>(baseAddress);
    for(auto &offset : offsets) {
        address = address + offset;
    }
    return address;
}

scl::PatternScannerResult scl::PatternScanner::SearchMemoryRegion(MemoryRegion mrgn, MemoryPattern pattern)
{
    return __Search(mrgn.base, mrgn.size, pattern);
}

scl::PatternScannerResult scl::PatternScanner::SearchProcessMemory(MemoryPattern pattern)
{
    MEMORY_BASIC_INFORMATION mbi;
    LPVOID address = NULL;
    
    while (VirtualQuery(address, &mbi, sizeof(mbi))) {
        // 只扫描已提交的、可执行的内存（代码段）
        if (mbi.State == MEM_COMMIT && 
            (mbi.Protect & PAGE_EXECUTE_READ || mbi.Protect & PAGE_EXECUTE_READWRITE)) {
            
            PatternScannerResult result = __Search(reinterpret_cast<ptr_t>(mbi.BaseAddress), mbi.RegionSize, pattern);
            if (result) {
                return result;
            }
        }
        
        address = (LPBYTE)mbi.BaseAddress + mbi.RegionSize;
    }
    
    return PatternScannerResult { .valid = false, .address = nullptr };
}

scl::PatternScannerResult scl::PatternScanner::__Search(ptr_t baseAddress, size_t regionSize, MemoryPattern pattern)
{
    pbyte_t memory = reinterpret_cast<pbyte_t>(baseAddress);
    
    for (size_t i = 0; i < regionSize - pattern.bytes.size(); i++) {
        bool found = true;
        for (size_t j = 0; j < pattern.bytes.size(); j++) {
            if (pattern.bytes[j].has_value() && memory[i + j] != pattern.bytes[j].value()) {
                found = false;
                break;
            }
        }
        if (found) {
            // Well we should do further checks here to ensure only one result is found.
            // But that probably causes performance issues, so just return the first found result.
            // Now there's a __FullSearch function for that purpose.
            return PatternScannerResult { .valid = true, .address = memory + i };
        }
    }
    return PatternScannerResult { .valid = false, .address = nullptr };
}

scl::PatternScannerResult scl::PatternScanner::__FullSearch(ptr_t baseAddress, size_t regionSize, MemoryPattern pattern)
{
    pbyte_t memory = reinterpret_cast<pbyte_t>(baseAddress);

    pbyte_t result = nullptr;
    bool multiple_found = false;
    
    for (size_t i = 0; i < regionSize - pattern.bytes.size(); i++) {
        bool found = true;
        for (size_t j = 0; j < pattern.bytes.size(); j++) {
            if (pattern.bytes[j].has_value() && memory[i + j] != pattern.bytes[j].value()) {
                found = false;
                break;
            }
        }
        if (found) {
            if (result == nullptr) {
                result = memory + i;
            } else {
                multiple_found = true;
                break;
            }
        }
    }
    return PatternScannerResult { .valid = (!multiple_found && result != nullptr), .address = result };
}

scl::MemoryPattern::MemoryPattern(const std::string &pattern)
{
    std::istringstream iss(pattern);
    std::string token;
    while (iss >> token) {
        if (token == "??" || token == "?") {
            bytes.push_back(std::nullopt);
            mask += "?";
        } else {
            bytes.push_back(std::stoi(token, nullptr, 16));
            mask += "x";
        }
    }
}

constexpr size_t scl::MemoryPattern::size() const
{
    return bytes.size();
}

std::optional<byte_t> scl::MemoryPattern::at(size_t index) const
{
    return bytes.at(index);
}

std::optional<byte_t> &scl::MemoryPattern::operator[](size_t index)
{
    return bytes[index];
}

scl::protect_t scl::GainRWXPermission(ptr_t address, size_t size)
{
    return protect_t();
}

scl::protect_t scl::RestorePermission(ptr_t address, size_t size, protect_t oldProtect)
{
    return protect_t();
}

std::bytearray scl::CopyMemory(MemoryRegion mrgn)
{
    std::bytearray data;
    for(size_t i = 0; i < mrgn.size; i++) {
        // what the hell is this? And amazingly useful anyway
        data.push_back(static_cast<std::byte>(*(reinterpret_cast<pbyte_t>(mrgn.base) + i)));
    }
    return data;
}

void scl::ApplyMemory(MemoryRegion mrgn, std::bytearray data)
{
    if(mrgn.size != data.size()) {
        throw std::runtime_error("scl::ApplyMemory: data size does not match memory region size");
    }

    // Maybe need to unlock first, then restore protection?
    // We actually NEED to finish it later.

    for(size_t i = 0; i < mrgn.size; i++) {
        *(reinterpret_cast<pbyte_t>(mrgn.base) + i) = static_cast<byte_t>(data.at(i));
    }
}

std::bytearray scl::ReplaceMemory(MemoryRegion mrgn, std::bytearray data)
{
    std::bytearray oldData = CopyMemory(mrgn);
    ApplyMemory(mrgn, data);
    return oldData;
}

#endif // OS_WINDOWS