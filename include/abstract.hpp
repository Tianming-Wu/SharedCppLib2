/*
    Abstract types used by some libraries

    This is NOT abstract classes in the OOP sense, but rather
    some general-purpose types that are not tied to any specific
    library or context, and can be used in various ways.
*/

#pragma once

#include <basics.hpp>
#include <string>
#include <vector>

#include <stringlist.hpp>

// forward declaration
namespace std {
    namespace filesystem {
        class path;
    }
}

// Abstract path used for tree-like path structures
// for example, filesystem
class abstract_path {
public:
    abstract_path() = default;
    abstract_path(const std::string& str);

    // only if you need a custom separator
    static abstract_path from(const std::string& str, const std::string& separator);

    ~abstract_path() = default;

    enable_copy_move(abstract_path)

    // Append a new component to the path
    abstract_path operator/(const abstract_path& ncomp);
    abstract_path& operator/=(const abstract_path& ncomp);

    // no, no + operator, it doesn't make sense

    // We cannot provide any kind of "exists" or "is_directory" methods here,
    // since this class has nothing to do with actual filesystem.
    // And it doesn't know the context where it is used.

    // Getters
    std::string operator[](size_t index) const;

    std::string string() const;
    std::string join(const std::string& separator) const; // in case user wants a different separator
    std::filesystem::path toFilesystemPath() const;

    std::stringlist toStringList() const;

    uint32_t depth() const;

private:
    std::stringlist m_components;
    char m_separator = '/';

    bool is_root = {false}; // if the path is root (starts with separator)

};