/*
    File abstraction layer for SharedCppLib2.

    This module provides a more-friendly interface for filesystem operations.
*/

#pragma once
#include <filesystem>

#include "api.hpp"
#include "macros.hpp"

namespace scl2 {

namespace fs = std::filesystem;


class file {
public:
    default_constructor_destructor(file)

    file(const std::string& path);

    void setPath(const fs::path& path);
    fs::path path() const;

    bool exists() const;
    bool is_regular_file() const;
    bool is_directory() const;

    // create an empty file if not exists
    bool create() const;

    // for filesystem level locking
    // bool lock() const;
    // bool locked() const;
    // bool ownlock() const;
    // bool release() const;

private:
    fs::path m_path;
};



} // namespace scl2