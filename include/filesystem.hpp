/*
    Filesystem module for SharedCppLib2.

    This library uses new features from C++23 and in an platform-
    specific optimization.

    It allows iterating through NTFS filesystems at insane speed.
    It requires administrative privileges to work. Otherwise it 
    throws an scl2::insufficient_privileges_exception.

    However, usually extreme high speed means optimizing everything
    to its limit, and often involves preallocating, caching and other
    techniques that are not modern C++ style. The target is to find a
    balanced solution.

    For NTFS_FRN_INDEX_BUILD, it can work at around 174000 Entries/second,
    and for FRN_INDEX_QUERY, it can work at around 4.175000e+06 entries/s.
    It is good enough for any Everything-style application.
    
    For NTFS_FAST_NAME_ITERATOR (which is a bit heavier), it can run at
    around 526500 entries/s. These numbers are tested on my computer, but
    you should see them faster than normal std::recursive_directory_iterator
    in most cases.
    Unluckily, stadard library implementation seems to include
    some sort of caching that makes it faster than expected after multiple
    tests, so I cannot get a decent comparison.
*/

#pragma once

#include <filesystem>
#include <generator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "platform.hpp"
#include "macros.hpp"
#include "engineering.hpp"
#include "sclexcept.hpp"

#ifdef OS_WINDOWS
#include <winioctl.h>
#endif

namespace scl2::filesystem {

using path = std::filesystem::path;

std::generator<path> fast_directory_iterator();

#ifdef OS_WINDOWS

constexpr size_t ntfs_readbuf_size = 256_Ki;

struct ntfs_file_entry {
    DWORDLONG FileReferenceNumber;
    DWORDLONG ParentFileReferenceNumber;
    USN Usn;
    DWORD FileAttributes;
    LARGE_INTEGER FileTime;
    DWORD FileNameLength;
    std::wstring FileName;
};

// Lightweight NTFS entry for high-throughput scanning.
struct ntfs_fast_entry {
    DWORD FileAttributes;
    std::wstring FileName;
};

struct ntfs_delta_entry {
    DWORDLONG FileReferenceNumber;
    DWORDLONG ParentFileReferenceNumber;
    USN Usn;
    DWORD Reason;
    DWORD FileAttributes;
    std::wstring FileName;
};

struct ntfs_index_entry {
    DWORDLONG FileReferenceNumber;
    DWORDLONG ParentFileReferenceNumber;
    DWORD FileAttributes;
    std::wstring FileName;
    std::wstring FileNameLower;
};

// NTFS filesystem handler: read MFT/USN directly.
// This module requires administrative privileges to run.
class ntfs {
public:
    ntfs();
    ~ntfs();

    ntfs(const ntfs&) = delete;
    ntfs& operator=(const ntfs&) = delete;

    // require image path like R"(\\.\C:)"
    bool open(const char* image_path);

    std::generator<ntfs_file_entry> fast_iterator();
    std::generator<ntfs_fast_entry> fast_name_iterator();
    std::generator<ntfs_delta_entry> delta_iterator(USN start_usn);

public:
    HANDLE hvol;
    USN_JOURNAL_DATA_V0 usnjdata;
};

class ntfs_frn_index {
public:
    bool build(const char* image_path);
    bool build(ntfs& volume);
    bool update_incremental(const char* image_path);
    bool update_incremental(ntfs& volume);

    void clear();
    bool empty() const;
    size_t size() const;
    size_t file_count() const;
    size_t dir_count() const;
    USN last_usn() const;

    const ntfs_index_entry* find(DWORDLONG frn) const;

    size_t query_name_contains(std::wstring_view keyword, std::vector<DWORDLONG>& out, size_t max_results = 0) const;
    std::vector<DWORDLONG> query_name_contains(std::wstring_view keyword, size_t max_results = 0) const;

    std::wstring rebuild_full_path(DWORDLONG start_frn, std::wstring_view volume_root) const;

private:
    void upsert_entry(const ntfs_index_entry& entry);
    void erase_entry(DWORDLONG frn);

    std::unordered_map<DWORDLONG, ntfs_index_entry> m_index;
    size_t m_file_count = 0;
    size_t m_dir_count = 0;
    USN m_last_usn = 0;
};

std::wstring ntfs_image_to_volume_root(const char* image_path);

std::generator<path> fast_ntfs_iterator();
std::generator<path> fast_ntfs_iterator(const path& filter);

#endif // OS_WINDOWS


} // namespace scl2::filesystem