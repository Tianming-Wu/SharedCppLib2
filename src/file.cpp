#include "file.hpp"

namespace scl2 {

file::file(const std::string &path)
    : m_path(path)
{}

fs::path file::path() const
{
    return m_path;
}

bool file::exists() const
{
    return fs::exists(m_path);
}

bool file::is_regular_file() const
{
    return fs::is_regular_file(m_path);
}

bool file::is_directory() const
{
    return fs::is_directory(m_path);
}

bool file::create() const
{
    return false;
}

void file::setPath(const fs::path &path)
{
    m_path = path;
}

} // namespace scl2
