#include "abstract.hpp"

#include <filesystem>

// stringlist makes everything easier
#include <stringlist.hpp>

abstract_path::abstract_path(const std::string &str)
{
    m_components = std::stringlist::split(str, m_separator);

    if(m_components.size() > 0 && m_components.at(0).empty()) {
        is_root = true;
        // will be removed by remove_empty below
    }

    m_components.remove_empty(); // remove empty components caused by consecutive separators
}

abstract_path abstract_path::from(const std::string &str, const std::string &separator)
{
    abstract_path ap;
    ap.m_separator = separator[0];

    ap.m_components = std::stringlist::split(str, ap.m_separator);

    if(ap.m_components.size() > 0 && ap.m_components.at(0).empty()) {
        ap.is_root = true;
        // will be removed by remove_empty below
    }

    ap.m_components.remove_empty(); // remove empty components caused by consecutive separators
    return ap;
}

abstract_path abstract_path::operator/(const abstract_path &ncomp)
{
    abstract_path newPath = *this;
    newPath.m_components.append(ncomp.m_components); // stringlist also provides append function, which is perfect for this case.
    return newPath;
}

abstract_path &abstract_path::operator/=(const abstract_path &ncomp)
{
    m_components.append(ncomp.m_components);
    return *this;
}

std::string abstract_path::operator[](size_t index) const
{
    return m_components.at(index);
}

std::string abstract_path::string() const
{
    return (is_root ? std::string(1, m_separator) : "") + m_components.join(std::string(1, m_separator));
}

std::string abstract_path::join(const std::string &separator) const
{
    return (is_root ? separator : "") + m_components.join(separator);
}

std::filesystem::path abstract_path::toFilesystemPath() const
{
    return std::filesystem::path(string());
}

std::stringlist abstract_path::toStringList() const
{
    return m_components;
}

uint32_t abstract_path::depth() const
{
    return static_cast<uint32_t>(m_components.size());
}
