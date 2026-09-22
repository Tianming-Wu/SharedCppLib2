#include "fileio.hpp"
#include "platform.hpp"

#include <iomanip>

#ifndef OS_WINDOWS
#include <fcntl.h> // ::open, used by flushFile
#endif

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

size_t fileio::write(const scl2::bytearray &data)
{
    data.writeRaw(fileStream);
    return data.size();
}

size_t fileio::write(const scl2::bytearray &data, size_t count)
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



void flushFile(const fs::path& path)
{
    // A std::ofstream does not expose its file descriptor, so the file is opened a second
    // time just to flush it. Flushing needs no more than a handle to the same file.
#ifdef OS_WINDOWS
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Failed to open file for flushing: " + path.string());

    const BOOL ok = FlushFileBuffers(handle);
    CloseHandle(handle);

    if (!ok)
        throw std::runtime_error("Failed to flush file: " + path.string());
#else
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
        throw std::runtime_error("Failed to open file for flushing: " + path.string());

    const int result = ::fsync(fd);
    ::close(fd);

    if (result != 0)
        throw std::runtime_error("Failed to flush file: " + path.string());
#endif
}

size_t writeFile(const fs::path& path, const scl2::bytearray& data, bool flush) {
    {
        std::ofstream ofs(path, std::ios::binary);
        if(!ofs) {
            throw std::runtime_error("Failed to open file for writing: " + path.string());
        }
        ofs << data;
    }
    if (flush) flushFile(path);
    return data.size();
}

size_t writeFile(const fs::path &path, const std::string &data, bool flush)
{
    {
        std::ofstream ofs(path);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing: " + path.string());
        }
        ofs << data;
    }
    if (flush) flushFile(path);
    return data.size();
}

scl2::bytearray readFile(const fs::path &path)
{
    std::ifstream ifs(path, std::ios::binary);
    if(!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    scl2::bytearray ba;
    ba.readAllFromStream(ifs);
    return ba;
}

scl2::bytearray readFileRange(const fs::path &path, size_t offset, size_t length)
{
    std::ifstream ifs(path, std::ios::binary);
    if(!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    scl2::bytearray ba;

    ifs.seekg(offset, std::ios::beg);
    if(length == static_cast<size_t>(-1)) {
        ba.readAllFromStream(ifs);
    } else {
        ba.readFromStream(ifs, length);
    }

    return ba;
}

scl2::string readFileAsString(const fs::path &path)
{
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

unsigned int foreachLine(const fs::path &path, const std::function<bool(unsigned int, const std::string &)> &func)
{
    std::ifstream ifs(path);

    if(!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }

    std::string line;
    unsigned int line_number = 0;
    while (std::getline(ifs, line)) {
        if (!func(line_number, line)) break;
        ++line_number;
    }

    return line_number;
}

scl2::stringlist readAllLines(const fs::path &path)
{
    scl2::stringlist lines;
    foreachLine(path, [&](unsigned int, const std::string &line) {
        lines.push_back(line);
        return true; // continue reading
    });

    return lines;
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