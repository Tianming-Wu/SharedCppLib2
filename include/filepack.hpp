/*
    File packaging and unpackaging module for SharedCppLib2.
    Tianming Wu <github.com/Tianming-Wu> 2026.03.25

    This module provides some utility functions for packaging
    multiple files into one file, and unpackaging them back. It can
    be used for simple archiving purposes, or for embedding resources
    into executables.

    It does not come with any compression features, but it is still
    possible and would be easy to do that.

    For encryption support, this class supports encrypting methods
    that provides SharedCppLib2 encryption API. Currently there is no
    such encryption method included in SharedCppLib2. (Afterall, you
    got an bytearray, and you can just encrypt it yourself before
    writing to a file. It's free.)

*/

#pragma once

#include <vector>

#include "api.hpp"
#include "bytearray.hpp"

namespace scl2 {

struct filepack_entry {
    std::string filename;
    std::bytearray data;
};


class filepack {
public:
    filepack();

    void addFile(const std::string& filename, const std::bytearray& data);

    uint64_t getFileCount() const;

    filepack_entry getFile(const std::string& filename) const;
    std::vector<filepack_entry> getAllFiles() const;

    enum SortMethod { SortByName, SortBySize };
    enum SortOrder { AscendingOrder, DescendingOrder };

    // Note: using std::sort, may not be stable.
    void sortFiles(SortMethod method, SortOrder order = AscendingOrder);

    static filepack load(const std::bytearray_view& data);
    static std::bytearray dump(const filepack& pack);

private:
    std::vector<filepack_entry> entries;
};

// API compatibility checks
scl2_check_generic_dump_load(filepack);


} // namespace scl2