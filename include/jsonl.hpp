/*
    jsonl for SharedCppLib2.

    jsonl is a simple format for storing multiple json objects in a single file.
    Each line is a json object, and the file can be read line by line.

    Thus, it is perfect for storing large amount of json data in chronological order,
    and can be read in a streaming manner, only requiring to parse a sinle line at a time.

    [SCL_STANDALONE_MODULE]
    version: 1.0.0
    cpp_generation: cxx17 - cxx23
    standalone_dependency: [json / json2]
*/
#pragma once

#ifndef SCL2_JSONL_CUSTOMIMPL
    #ifdef SCL2_JSONL_USE_JSON2
        #include "json2.hpp"
        #define jsonl_impl ::scl2::json2
    #else
        #include "json.hpp"
        #define jsonl_impl ::scl2::json
    #endif
#else
    #define jsonl_impl SCL2_JSONL_CUSTOMIMPL
#endif

#ifndef SCL2_JSONL_SMALLERBUFFER
    #define SCL2_JSONL_BUFFERSIZE 1048576 // 1 MB buffer size
#else
    #define SCL2_JSONL_BUFFERSIZE 1024 // default buffer size (1kb)
#endif

#include <fstream>

namespace scl2 {

class jsonl {
public:
    jsonl() = default;
    jsonl(const std::filesystem::path& path, bool append = true);
    
    ~jsonl();

    void open(bool append = true);
    void open(const std::filesystem::path& path, bool append = true);
    bool isOpen() const { return m_file.is_open(); }

    bool close();

    // save the current file, and open a new file.
    void splitFile(const std::filesystem::path& newPath);

    void append(const jsonl_impl& target);

    /// @warning This function may create a large amount of json objects.
    /// It is quite dangerous if the file can be very large. 
    std::vector<jsonl_impl> parseAll() const;

    /// @brief Read the last n lines of the jsonl file.
    /// optimized for large files, will not read the entire file into memory,
    /// only the selected lines.
    std::vector<jsonl_impl> getLastN(size_t n) const;

    // get the last line of the jsonl file, or an empty json object if the file is empty.
    jsonl_impl tail() const;

    // use chunked fetch to read the file.
    size_t lineCount() const;

private:
    std::filesystem::path m_path;

    mutable std::fstream m_file;
    mutable char m_buffer[SCL2_JSONL_BUFFERSIZE];

};

} // namespace scl2