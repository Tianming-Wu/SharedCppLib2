#include "fileio.hpp"

#include <iomanip>

namespace scl2 {

namespace { // helper

std::ios_base::openmode translate_open_mode(file_mode_flag mode) {
    std::ios_base::openmode open_mode = static_cast<std::ios_base::openmode>(0);

    if ((mode & file_mode_flag::Read) == file_mode_flag::Read) {
        open_mode |= std::ios::in;
    }
    if ((mode & file_mode_flag::Write) == file_mode_flag::Write) {
        open_mode |= std::ios::out;
    }
    if ((mode & file_mode_flag::Append) == file_mode_flag::Append) {
        open_mode |= std::ios::app;
    }
    if ((mode & file_mode_flag::Binary) == file_mode_flag::Binary) {
        open_mode |= std::ios::binary;
    }
    if ((mode & file_mode_flag::Truncate) == file_mode_flag::Truncate) {
        open_mode |= std::ios::trunc;
    }
    // Note: std::fstream does not have a direct equivalent for the Create flag. 
    // It will create the file if it does not exist when opened with std::ios::out, so we don't need to handle it separately.

    return open_mode;
}

} // namespace <unnamed> (helper)



fileio::fileio(const std::string &path, file_mode_flag mode)
    : fileStream(path, translate_open_mode(mode))
{}

size_t fileio::write(const std::bytearray &data)
{
    data.writeRaw(fileStream);
    return data.size();
}

size_t fileio::write(const std::bytearray &data, size_t count)
{
    data.subarr(0, count).writeRaw(fileStream);
    return count;
}

bool fileio::good() const
{
    return fileStream.good();
}

void fileio::close()
{
    fileStream.close();
}



size_t writeFile(const fs::path& path, const std::bytearray& data) {
    std::ofstream ofs(path, std::ios::binary);
    if(!ofs) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    ofs << data;
    return data.size();
}

std::bytearray readFile(const fs::path &path)
{
    std::ifstream ifs(path, std::ios::binary);
    if(!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    std::bytearray ba;
    ba.readAllFromStream(ifs);
    return ba;
}

// Helper function for sync.
namespace {
std::strong_ordering compareFileTimestamp(file_timestamp src_tm, const fs::path& target)
{
    std::error_code ec;
    auto target_tm = fs::last_write_time(target, ec);
    if (ec) {
        // If we fail to get the timestamp of the target file, we can only say that src is newer if it has a valid timestamp.
        return src_tm != file_timestamp{} ? std::strong_ordering::greater : std::strong_ordering::equal;
    }

    if (src_tm < target_tm) {
        return std::strong_ordering::less;
    } else if (src_tm > target_tm) {
        return std::strong_ordering::greater;
    } else {
        return std::strong_ordering::equal;
    }
}}

} // namespace scl2