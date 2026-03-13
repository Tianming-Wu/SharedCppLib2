#include "keydb.hpp"

#include <random>
#include <stdexcept>

namespace kdb {

/*
    General implementation details:

    File structure:
    [Header] (24B) (see definition in hpp)
    [Data] (Dynamic) -> {
        [Type] (1B) -> {
            1b: Deleted mark
            7b: type id (value_type)
        }
        [Key Length] (2B)
        [Value Length] (2B)
        [Key] (Dynamic)
        [Value] (Dynamic)
    }

    WAL File structure:
    (just a little different from db file)
    [DBID] (4B)
    [Write Count] (8B) - Note:
        This is kept the same as when it was initialized,
        so we can use it to determine how many writes have not
        been applied to the database file, or if the wal is ever
        compatible.
    [Data] (Dynamic) -> {
        [Type] (1B) -> {
            1b: Deleted mark
            7b: type id (value_type)
        }
        [Key Length] (2B)
        [Value Length] (2B)
        [Key] (Dynamic)
        [Value] (Dynamic)
    }

*/


database::database()
{
}

database::database(policy pol)
    : m_policy(pol)
{
}

database::database(const path &p)
    : m_path(p)
{
    if(!exists()) {
        error_code ec;
        if(error_failure(ec = init_db())) {
            throw std::runtime_error(std::string("Failed to initialize database: " + translate_error(ec)));
        }
    } else {
        error_code ec;
        if(error_failure(ec = load())) {
            throw std::runtime_error(std::string("Failed to load database: " + translate_error(ec)));
        }
    }
}

database::database(const path &p, policy pol)
    : m_path(p), m_policy(pol)
{
    if(!exists()) {
        error_code ec;
        if(error_failure(ec = init_db())) {
            throw std::runtime_error(std::string("Failed to initialize database: " + translate_error(ec)));
        }
    } else {
        error_code ec;
        if(error_failure(ec = load())) {
            throw std::runtime_error(std::string("Failed to load database: " + translate_error(ec)));
        }

        if(testPolicy(policy::ReadOnly)) {

        } else {
            if(testPolicy(policy::AutoSave) && testPolicy(policy::FastSave)) {
                // If "FastSave" is enabled, we should make sure
                // m_file is opened in read-write mode, because we need to update the header
                // when saving.
            }
        }
        
    }
}

database::~database()
{
    // Run a full-save to make sure everything is saved.
    // It should always happen no matter AutoSave is enabled or not,
    // unless the database is in read-only mode.
    if(!testPolicy(policy::ReadOnly)) save();

    if(ownlock()) {
        release();
    }
}

error_code database::addKey(const std::string& key, const value_t& value)
{
    // Cannot add key in read-only mode.
    if(testPolicy(policy::ReadOnly)) return error_code::IsReadOnly;

    if(key.size() > max_key_value_size) {
        return error_code::KeyToLarge;
    } else if (value.size() > max_key_value_size) {
        return error_code::DataTooLarge;
    }

    try {
        m_data[key] = { value };
        m_data_diff[key] = { value };
    } catch(...) {
        return error_code::Failed;
    }

    if (testPolicy(policy::AutoSave)) {
        return instant_save();
    }
    
    return error_code::Success;
}

error_code database::deleteKey(const std::string &k)
{
    // Cannot delete key in read-only mode.
    if(testPolicy(policy::ReadOnly)) return error_code::IsReadOnly;

    try {
        auto it = m_data.find(k);
        size_t file_offset = 0;
        if(it != m_data.end()) {
            file_offset = it->second.file_offset;
        }

        m_data.erase(k); // Remove from data
        m_data_diff[k].file_offset = file_offset;
        m_data_diff[k].markDeleted(true); // Mark as deleted in diff, in case "FastSave" need it.
    } catch(...) {
        return error_code::Failed;
    }

    if (testPolicy(policy::AutoSave)) {
        return instant_save();
    }
    
    return error_code::Success;
}

bool database::hasKey(const std::string &k)
{
    auto it = m_data.find(k);
    return (it != m_data.end()) && !it->second.isDeleted();
}

value_t database::getValue(const std::string &k, const value_t& def)
{
    auto it = m_data.find(k);
    if (it != m_data.end()) {
        auto& v = it->second;
        return v.isDeleted() ? def : v;
    }
    return def;
}


// currently lock() is also non-blocking, due
// to some design restrictions.
bool database::lock()
{
    if(ownlock()) return true;
    else if(locked()) return false;

    m_ownlock = true;

    // Write the 'lock' bit to database.
    return writePolicy(m_policy | policy::WriteLock);
}

bool database::try_lock()
{
    if(ownlock()) return true;
    else if(locked()) return false;

    m_ownlock = true;

    return writePolicy(m_policy | policy::WriteLock);
}

bool database::release() const
{
    if(!ownlock()) return false;

    m_ownlock = false;

    return writePolicy(m_policy & policy::WriteLock);
}

bool database::locked() const
{
    policy pol;
    pol = readPolicy();

    // regard inaccessible as locked
    return (pol == policy::Null) || ( (pol & policy::WriteLock) != policy::Null );
}

bool database::ownlock() const
{
    return m_ownlock;
}

error_code database::load()
{
    if(m_path.empty()) return error_code::FileNotFound;
    if(m_file.is_open()) m_file.close();

    m_file.open(m_path, std::ios::in | std::ios::binary);
    if(!m_file.is_open()) return error_code::Inaccessible;

    // read header
    header h;
    m_file.read((char*)&h, header_size);
    if(h.magic != header_magic) {
        m_file.close();
        return error_code::NotDBFile;
    }

    // apply header info
    loadHeader(h);

    // Read policy from header.
    // This DOES NOT INCLUDE THE CURRENT POLICY, ONLY DATABASE SETTINGS
    policy pol = readPolicy();

    // Test if database is locked.
    if(testPolicy(policy::WriteLock, pol) && !ownlock()) {
        if(testPolicy(policy::AllowParrallelRead, pol)) {
            if(!testPolicy(policy::ReadOnly)) {
                m_file.close();
                return error_code::Locked;
            } // Else: Database allow parallel read and readonly enabled, bypass.
        } else {
            m_file.close();
            return error_code::Locked;
        }
    }

    // merge into current policy.
    m_policy = pol | m_policy;


    // read key-value pairs
    m_data.clear();
    for(size_t i = 0; i < h.key_count; i++) {
        uint16_t klen, vlen;
        value_type type;
        size_t offset = m_file.tellg(); // The position of this key-value pair in file, for "delete mark" and "FastSave".

        m_file.read((char*)&type, sizeof(type));
        m_file.read((char*)&klen, sizeof(klen));
        m_file.read((char*)&vlen, sizeof(vlen));

        std::string k(klen, '\0');
        std::bytearray v;
        v.resize(static_cast<size_t>(vlen));
        if(klen != 0) m_file.read(k.data(), klen);
        if(vlen != 0) m_file.read((char*)v.data(), vlen);

        // This ususally indicates that there was a duplicated key,
        // probably caused by "FastSave". We reduce the index by one
        // To make sure corruption detection works fine, and keep the
        // latest value. It will be corrected after a save().
        if(m_data.find(k) != m_data.end()) i--;

        // We should not add this key then
        if((type & value_type::Deleted_Mask) != value_type::Null) {
            h.key_count -= 1;
            i--;
            continue;
        }

        value_t loaded;
        loaded.type = type;
        loaded.valueData = std::move(v);
        loaded.file_offset = offset;
        m_data[k] = std::move(loaded);
    }

    // If there are still remaining data in file, report the database file as corrupted.
    return m_file.peek() == EOF ? error_code::Success : error_code::Corrupted;
}

error_code database::save()
{
    if(m_path.empty()) return error_code::FileNotFound;
    if(m_file.is_open()) m_file.close();

    m_file.open(m_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if(!m_file.is_open()) return error_code::Inaccessible;

    // write header
    header h = makeHeader();
    m_file.write((char*)&h, header_size);

    // write key-value pairs
    for(const auto& [k, v] : m_data) {
        // It is not possible for klen or vlen to overflow, because
        // it was verified while inserting.
        uint16_t klen = static_cast<uint16_t>(k.size());
        uint16_t vlen = static_cast<uint16_t>(v.valueData.size());

        m_file.write((char*)&v.type, sizeof(value_type)); // Write the type flag
        m_file.write((char*)&klen, sizeof(klen));
        m_file.write((char*)&vlen, sizeof(vlen));
        m_file.write(k.data(), klen);

        // still need cast since std::byte is explicitly not implicitly converible.
        m_file.write((char*)v.valueData.data(), vlen);

        ///TODO: Report InsufficientSpace when happened.
    }

    return error_code::Success;
}

bool database::exists() const
{
    return (!m_path.empty()) && fs::exists(m_path);
}

// Instantly save the database.
// This is the behavior triggered by "AutoSave" policy.
error_code database::instant_save()
{
    if(testPolicy(policy::FastSave)) {
        // What is special here is that it will not write the entire database,
        // but only the changed values, and update the header accordingly.

        // Here's how it works:
        // Just save whatever was added or modified since last save (tracked
        // by m_data_diff), and only append those to the end of the file.
        // Then we only need to update two values in header.

        // Then, in the next full load-save cycle, std::map will "automatically"
        // handle those "duplicated" keys and only keep the latest value, and sort
        // them as well. Simple and nothing to worry about.

        // This mode requires the file to not be closed, so it should be opened here.
        // No more checking will happen here for better performance.

        m_file.seekp(0, std::ios::end); // move to the end of file for appending

        for(const auto& [k, v] : m_data_diff) {
            if(v.isDeleted()) {
                // seek to the position
                m_file.seekp(v.file_offset, std::ios::beg);
                // mark as deleted
                m_file.write((char*)&v.type, sizeof(value_type));

                // go back to file end for next appending
                m_file.seekp(0, std::ios::end);

            } else {
                uint16_t klen = static_cast<uint16_t>(k.size());
                uint16_t vlen = static_cast<uint16_t>(v.valueData.size());

                m_file.write((char*)&v.type, sizeof(value_type)); // Write the type flag.
                m_file.write((char*)&klen, sizeof(klen));
                m_file.write((char*)&vlen, sizeof(vlen));
                m_file.write(k.data(), klen);
                m_file.write((char*)v.valueData.data(), vlen);

            }

            ///TODO: Report InsufficientSpace when happened.
        }

        // After appending, update the header.
        header h = makeHeader();
        m_file.seekp(0, std::ios::beg); // move back to the beginning of file for updating header
        m_file.write((char*)&h, header_size);

        m_file.flush(); // Make sure data is written.

        m_data_diff.clear(); // Clear the diff after saving.

        // done.
        return error_code::Success;

    } else {
        // anyway, this is still simple enough for small amount of data.
        // Just do a full-save.
        return save();
    }
}

bool database::testPolicy(policy p) const
{
    return (( p & m_policy ) != policy::Null);
}

bool database::testPolicy(policy p, policy src) const
{
    return (( p & src ) != policy::Null);
}

bool database::writePolicy(policy p) const
{
    libpolicy lp = KDB_TOLIBPOLCY(p | policy::Enabled); // Enabled bit ensures libpolicy != 0.
    // write lp to database file.

    if(m_file.is_open()) {
        std::streampos old_pos = m_file.tellp();

        m_file.seekp(offsetof(header, header::pol)); // directly write to policy part of header
        m_file.write((char*)&lp, sizeof(lp));
        m_file.flush();

        if(old_pos != std::streampos(-1)) {
            m_file.seekp(old_pos);
        }

        return true;
    }
    return false;
}

policy database::readPolicy() const
{
    if(m_file.is_open()) {
        std::streampos old_pos = m_file.tellg();

        m_file.seekg(offsetof(header, header::pol)); // directly read from policy part of header
        libpolicy lp;
        m_file.read((char*)&lp, sizeof(lp));

        if(old_pos != std::streampos(-1)) {
            m_file.seekg(old_pos);
        }

        return KDB_FROMLIBPOLICY(lp);
    }
    return policy::Null;
}

error_code database::init_db()
{
    if(m_path.empty()) return error_code::FileNotFound;

    if(exists()) fs::remove(m_path);

    // Create the file and write header
    m_file.open(m_path, std::ios::out | std::ios::binary);
    if(!m_file.is_open()) return error_code::Inaccessible;

    header h = makeHeader(); // this will return an empty header.
    m_file.write((char*)&h, header_size);
    m_file.flush();

    ///TODO: Although it is very unlikely to fail here, report InsufficientSpace when happened.
    
    return error_code::Success;
}

header database::makeHeader() const
{
    if(m_dbid == 0) {
        // DBID is not set, so this is probably called by init_db().
        // We generate a random DBID here, and it will be saved to file by init_db().
        m_dbid = static_cast<uint32_t>(std::random_device{}());
    }
    return header {
        .magic = header_magic,
        .dbid = m_dbid,
        .pol = KDB_TOLIBPOLCY(m_policy | policy::Enabled), // Enabled bit ensures libpolicy != 0 (distinguishes from policy::Null).
        .write_count = h_write_count,
        .key_count = m_data.size()
    };
}

void database::loadHeader(const header &h)
{
    h_write_count = h.write_count;
    m_dbid = h.dbid;
}

error_code database::walwrite(const kvpair &kv)
{
    if(!testPolicy(policy::WAL)) return error_code::DisabledFeature;
    if(m_path.empty()) return error_code::FileNotFound;
    else if (m_wal_path.empty()) m_wal_path = m_path.string() + ".wal";

    std::ofstream wal_file(m_wal_path, std::ios::out | std::ios::binary | std::ios::app);
    if(!wal_file.is_open()) return error_code::Inaccessible;
}

error_code database::waldel(const kvpair &kv)
{
    if(!testPolicy(policy::WAL)) return error_code::DisabledFeature;
    if(m_path.empty()) return error_code::FileNotFound;
    else if (m_wal_path.empty()) m_wal_path = m_path.string() + ".wal";

    std::ofstream wal_file(m_wal_path, std::ios::out | std::ios::binary | std::ios::app);


}

error_code database::walclear()
{
    if(!testPolicy(policy::WAL)) return error_code::DisabledFeature;
    if(m_path.empty()) return error_code::FileNotFound;
    else if (m_wal_path.empty()) m_wal_path = m_path.string() + ".wal";

    // Just remove the wal file, it will be recreated when needed.
    if(fs::exists(m_wal_path)) {
        if(!fs::remove(m_wal_path)) {
            return error_code::Inaccessible;
        }
    }
    return error_code::Success;
}

bool database::walexists() const
{
    if(!testPolicy(policy::WAL)) return false;
    if(m_path.empty()) return false;

    return fs::exists(m_wal_path.empty() ? path(m_path.string() + ".wal") : m_wal_path);
}

error_code database::walapply()
{
    if(!testPolicy(policy::WAL)) return error_code::DisabledFeature;
    if(m_path.empty()) return error_code::FileNotFound;
    else if (m_wal_path.empty()) m_wal_path = m_path.string() + ".wal";


}

bool database::canRead() const
{
    return !testPolicy(policy::WriteOnly);
}

bool database::canWrite() const
{
    if(ownlock()) return true;
    else if(locked()) return false;

    if(testPolicy(policy::ReadOnly)) return false;

    return true;
}

bool database::checkPolicyValid(policy p)
{
    // FastSave is only available when AutoSave is enabled, but we do
    // not check it, just ignore it.

    // I really dislike macros, but better than a lambda anyway
    #define __testpolicy(P) ((p & policy::P) != policy::Null)

    // ReadOnly is incompatible with WriteOnly.
    if( __testpolicy(ReadOnly) && __testpolicy(WriteOnly)) return false;

    #undef __testpolicy

    return true;
}

// Translate error code to human readable string.
// switch() is already efficient enough for enum.
std::string database::translate_error(error_code code)
{
    switch(code) {
        case error_code::Success:
            return "Success";
        case error_code::Failed:
            return "Failed (no more details available)";
        
        case error_code::Inaccessible:
            return "File inaccessible";
        case error_code::FileNotFound:
            return "File not found";
        case error_code::InsufficientSpace:
            return "Insufficient space";

        case error_code::Locked:
            return "Database is locked by another process";
        case error_code::IsReadOnly:
            return "Database is read-only";
        case error_code::NotDBFile:
            return "Not a valid database file";
        case error_code::Corrupted:
            return "Database file is corrupted";
        case error_code::Encrypted:
            return "Encrypted and decryption is not possible";

        case error_code::BufferOverflow:
            return "Buffer overflow, or data size exceeds limit";
        case error_code::KeyToLarge:
            return "Key size exceeds limit that can be handled by this library";
        case error_code::DataTooLarge:
            return "Data size exceeds limit that can be handled by this library";
        case error_code::DisabledFeature:
            return "The feature is disabled, but relevant function is called";

        default:
            return "Unknown error";
    }
}

value_t::value_t()
    : type(value_type::Null), valueData(), file_offset(0)
{}

value_t::value_t(const char *cstr, size_t file_offset)
    : type(value_type::String), valueData(std::string(cstr ? cstr : "")), file_offset(file_offset)
{}

value_t::value_t(const std::string &str, size_t file_offset)
    : type(value_type::String), valueData(str), file_offset(file_offset)
{}

value_t::value_t(const std::wstring &wstr, size_t file_offset)
    : type(value_type::WString), valueData(std::bytearray::fromStdWString(wstr)), file_offset(file_offset)
{}

value_t::value_t(const std::bytearray &data, size_t file_offset)
    : type(value_type::Binary), valueData(data), file_offset(file_offset)
{}

value_t::value_t(const path &filepath, size_t file_offset)
    : type(value_type::ExternalFile), valueData(filepath.string()), file_offset(file_offset)
{}

// Note: Read should not happen when marked as deleted,
// So we do not process deleted flag here and just let
// it fail because of type mismatch.

std::string value_t::asString() const
{
    if(type != value_type::String) {
        throw std::runtime_error("Value is not a string");
    }
    return valueData.toStdString();
}

std::wstring value_t::asWString() const
{
    if(type != value_type::WString) {
        throw std::runtime_error("Value is not a wide string");
    }
    return valueData.toStdWString();
}

std::bytearray value_t::asBinary() const
{
    if(type != value_type::Binary) {
        throw std::runtime_error("Value is not binary");
    }
    return valueData;
}

path value_t::asExternalFile() const
{
    if(type != value_type::ExternalFile) {
        throw std::runtime_error("Value is not an external file path");
    }
    return path(valueData.toStdString());
}

bool value_t::isNull() const
{
    return (type & ~value_type::Deleted_Mask) == value_type::Null;
}

size_t value_t::size() const
{
    return valueData.size();
}

} // namespace kdb