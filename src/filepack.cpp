#include "filepack.hpp"

#include <algorithm>

namespace scl2 {

// Metadata that does not expose to users.
namespace {

struct filepack_metadata {
    uint64_t file_count;

    std::vector<std::pair<std::string, uint64_t>> entries; // filename and file size

    static filepack_metadata load(const std::bytearray_view& data);
    static std::bytearray dump(const filepack_metadata& metadata);
};

filepack_metadata filepack_metadata::load(const std::bytearray_view &data)
{
    filepack_metadata metadata;
    metadata.file_count = data.read<uint64_t>(); // file count

    for(uint64_t i = 0; i < metadata.file_count; i++) {
        std::string filename = data.readString(); // filename
        uint64_t filesize = data.read<uint64_t>(); // file size
        metadata.entries.emplace_back(std::move(filename), filesize);
    }
    return metadata;
}

std::bytearray filepack_metadata::dump(const filepack_metadata &metadata)
{
    std::bytearray data;
    data.append(metadata.file_count); // file count

    if(metadata.file_count != metadata.entries.size()) {
        throw std::runtime_error("filepack_metadata::dump: file count does not match entries size");
    }

    for(const auto& [filename, filesize] : metadata.entries) {
        data.addString(filename); // filename
        data.append(filesize); // file size
    }
    return data;
}
};

filepack::filepack()
{
}

void filepack::addFile(const std::string &filename, const std::bytearray &data)
{
    entries.push_back({filename, data});
}

uint64_t filepack::getFileCount() const
{
    return entries.size();
}

filepack_entry filepack::getFile(const std::string &filename) const
{
    auto it = std::find_if(entries.begin(), entries.end(), [&filename](const filepack_entry& entry) {
        return entry.filename == filename;
    });
    if(it == entries.end()) {
        throw std::runtime_error("filepack::getFile: file not found");
    }
    return *it;
}

std::vector<filepack_entry> filepack::getAllFiles() const
{
    return entries;
}

void filepack::sortFiles(SortMethod method, SortOrder order)
{
    auto comparator = [method, order](const filepack_entry& a, const filepack_entry& b) {
        int cmp = 0;
        switch(method) {
            case SortByName:
                cmp = a.filename.compare(b.filename);
                break;
            case SortBySize:
                cmp = (a.data.size() < b.data.size()) ? -1 : ((a.data.size() > b.data.size()) ? 1 : 0);
                break;
        }
        return (order == SortOrder::AscendingOrder) ? (cmp < 0) : (cmp > 0);
    };
    std::sort(entries.begin(), entries.end(), comparator);
}

filepack filepack::load(const std::bytearray_view &data)
{
    // bytearray_view passing support serialized parsing.
    filepack_metadata metadata = filepack_metadata::load(data);

    // We can then parse the rest of the data according to the metadata.
    filepack pack;
    for(const auto& [filename, filesize] : metadata.entries) {
        std::bytearray filedata = data.readBytes(filesize);
        pack.entries.push_back({filename, std::move(filedata)});
    }
    return pack;
}

std::bytearray filepack::dump(const filepack &pack)
{
    std::bytearray data;

    // make a metadata object first
    filepack_metadata metadata;
    metadata.file_count = pack.entries.size();
    for(const auto& entry : pack.entries) {
        metadata.entries.emplace_back(entry.filename, entry.data.size());
    }

    // dump metadata first
    data.append(filepack_metadata::dump(metadata));

    // then dump file data sequentially
    for(const auto& entry : pack.entries) {
        data.append(entry.data);
    }
    return data;
}


} // namespace scl2