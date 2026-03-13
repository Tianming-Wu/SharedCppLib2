/*
    KeyDB module for SharedCppLib2.
    Tianming-Wu <github.com/Tianming-Wu> 2026.03.12

    This module features a lightweight, fast and portable key-value
    pair database implementation.

    It supports multiple data types, and supports API-supported
    class types for auto serialize/deserialize.

    (Note: Check API definitions in SharedCppLib2 API document.
    binary-based dump/load is used first, or it can fallback to
    string-based serialize/deserialize if present.)

    When in-memory, std::map is used for fast lookups.

    When in-file, a simple binary format is used to store values.

    This library also supports a special WAL mode for special use
    cases, like running on a portable disk.

    Warning:
        Not really for production use, because some of the designs
        are not capable for handling very large amount of data.

    Maximum length supported for key and value is 65535 bytes.

    Warning:
        Do not try to use key values that are very long, or you
        may encounter severe performance degradation, because the
        current implementation is not optimized for that.

    
    Dev plan (ideas):
        For large chunk of binary data, use a special type (type
        support will also be added later): ExternalFile, and store
        the path to the file in value. Access will be redirected
        to the file, and in this way it is possible to handle
        whatever size of data, as long as the file system can handle
        it.
*/

#pragma once

#include <map>
#include <string>
#include <fstream>
#include <filesystem>
#include <type_traits>

#include "basics.hpp"
#include "bytearray.hpp"
#include "api.hpp"

namespace kdb {

typedef std::pair<std::string, std::string> kvpair;
using std::filesystem::path;
using std::filesystem::directory_entry;
namespace fs = std::filesystem;


// Policy is a 32-bit number, and consists of 2 parts:
// the lower 16-bit is runtime settings, and the upper 16-bit
// is library settings that goes with the storage file, and
// will be loaded when the file is loaded.
enum class policy : uint32_t {
    Null = 0,
    ReservedB1 = 1 << 0,
    ReadOnly = 1 << 1, // Default mode is read-write.
    WriteOnly = 1 << 2, // Probably has no effect
    AutoSave = 1 << 3, // Save each time a key is changed.
    FastSave = 1 << 4, // Only update the changed part. Need AutoSave enabled.
    EnableIndexing = 1 << 9, // Not implemented. Usually std::map is fast enough.
    MultiThread = 1 << 10, // Enable multi-thread safe features. Not implemented.

    Enabled = 1 << 16, // reserved, whatever
    AllowParrallelRead = 1 << 17, // Not safe, better use with readonly
    AllowParrallelWrite = 1 << 18, // Unusable for now, but in the future there would be sync machanism.
    WriteLock = 1 << 19,
    Encrypted = 1 << 20, // Not implemented yet
    WAL = 1 << 21,
    WALUnfinished = 1 << 22,
};
Define_Enum_BitOperators(policy)

enum class error_code : uint32_t {
    Success = 0,
    Failed = 1, // Generic failure, no more details available.

    Inaccessible = 2, // Inaccessible due to system reason)
    FileNotFound = 3, // File not found, or path is empty.
    InsufficientSpace = 4, // No space left on device, or file size limit reached.

    Locked = 10, // Will not fire if parallel access is possible
    IsReadOnly = 11, // Operation cannot be performed because database is read-only.
    NotDBFile = 12, // Not a valid database file.
    Corrupted = 13, // Database file is corrupted, or data is corrupted.
    Encrypted = 15, // Encrypted and decryption is not possible (incorrect key or not set at all).

    BufferOverflow = 30, // Buffer overflow, or data size exceeds limit.
    KeyToLarge = 31, // Key size exceeds limit that can be handled by this library.
    DataTooLarge = 32, // Data size exceeds limit that can be handled by this library.
    DisabledFeature = 33, // The feature is disabled, but relevant function is called
};

inline bool error_success(error_code code) { return code == error_code::Success; }
inline bool error_failure(error_code code) { return code != error_code::Success; }

typedef uint16_t libpolicy;

#define KDB_LIBPOLICY_MASK 0xFFFF0000
#define KDB_LIBPOLICY_SHIFT 16

#define KDB_TOLIBPOLCY(POLICY) (static_cast<libpolicy>((static_cast<uint32_t>(POLICY) & KDB_LIBPOLICY_MASK) >> KDB_LIBPOLICY_SHIFT))
#define KDB_FROMLIBPOLICY(LIBPOLICY) (static_cast<policy>(static_cast<uint32_t>(LIBPOLICY) << KDB_LIBPOLICY_SHIFT))

#pragma pack(push, 1) // Ensure no padding
struct header {
    uint32_t magic; // "KDB1"
    uint32_t dbid;
    libpolicy pol; // policy, but name unusable
    size_t write_count; // for WAL mode, also available in normal mode anyway.
    size_t key_count;
};
#pragma pack(pop)

// Make sure that header is compatible for directwrite api.
static_assert(std::is_trivially_copyable_v<header>, "Error: header not trivially copyable!");

// Header is not very large (only 26 bytes)
constexpr uint16_t header_size = static_cast<uint16_t>(sizeof(header));
// header magic
constexpr uint32_t header_magic = ( ('K') | ('D' << 8) | ('B' << 16) | ('1' << 24) );

constexpr uint16_t max_key_value_size = 65535; // Because we use uint16_t to store key and value length, so the maximum size is 65535 bytes.

#define header_member_offset(member) (offsetof(header, header::member))

// Accessible range: 0 ~ 128, and 1 bit of deleted mark.
enum class value_type : uint8_t {
    Null = 0,
    String = 1,
    WString = 2,
    Binary = 4,
    ExternalFile = 5,

    // Deleted mark
    Deleted_Mask = 0x80, // 1000 0000
};
Define_Enum_BitOperators(value_type) // to verify deleted mark.

// For future use, not fully implemented yet.
// This will be the middle layer to support types other than
// string, and also support auto serialize/deserialize for API-supported
// class types.
struct value_t {
    value_type type;
    std::bytearray valueData; // for binary data or serialized data.

    size_t file_offset = 0; // To make DirectAccess faster.

    inline bool isDeleted() const { return (type & value_type::Deleted_Mask) != value_type::Null; }
    inline void markDeleted(bool del = true) {
        if(del) type = static_cast<value_type>(static_cast<uint8_t>(type) | static_cast<uint8_t>(value_type::Deleted_Mask));
        else type = static_cast<value_type>(static_cast<uint8_t>(type) & ~static_cast<uint8_t>(value_type::Deleted_Mask));
    }

    value_t();
    value_t(const char* cstr, size_t file_offset = 0);
    value_t(const std::string& str, size_t file_offset = 0);
    value_t(const std::wstring& wstr, size_t file_offset = 0);
    value_t(const std::bytearray& data, size_t file_offset = 0);
    value_t(const path& filepath, size_t file_offset = 0);

    std::string asString() const;
    std::wstring asWString() const;
    std::bytearray asBinary() const;
    path asExternalFile() const;

    bool isNull() const;

    size_t size() const; // Equivalent to valueData.size()
};

class database {
public:

    database();
    database(policy pol);
    database(const path& p);
    database(const path& p, policy pol);

    ~database();

    
    // normal operations

    /** @brief Add a key-value pair to the database. If the key already exists, it will be overwritten.
     */
    error_code addKey(const std::string& key, const value_t& value);

    /** @brief Delete a key from the database. If the key does not exist, do nothing.
     */
    error_code deleteKey(const std::string& k);

    /** @brief Check whether a key exists in the database.
     * @return true if the key exists, false otherwise.
     */
    bool hasKey(const std::string& k);

    /** @brief Get the value associated with a key. If the key does not exist, return the default value.
     * @return the value associated with the key, or the default value if the key does not exist.
     */
    value_t getValue(const std::string& k, const value_t& def = {});


    // access related operations

    /** @brief Lock the database for exclusive access. 
     * @return true if the lock is acquired successfully, false otherwise.
     */
    bool lock();

    /** @brief Try to lock the database for exclusive access without blocking.
     * @return true if the lock is acquired successfully, false otherwise.
     */
    bool try_lock();

    /** @brief Unlock the database.
     * @return true if the lock is released successfully, false otherwise.
     */
    bool release() const;

    /** @brief Check whether the database is currently locked.
     * @return true if the database is locked, false otherwise.
     * 
     * Note: is does not mean lock is owned by current instance, it
     * can be someone else. To check that, use `ownlock()` instead.
     */
    bool locked() const;

    /** @brief Check whether the lock is owned by current instance.
     * @return true if the lock is owned by current instance, false otherwise.
     */
    bool ownlock() const;

    // update if edited
    error_code load(); // load (from scratch)
    error_code save(); // save (entirely)


    /** @brief Check whether the database file exists.
     * @return true if the database file exists, false otherwise.
     */
    bool exists() const;


    static std::string translate_error(error_code code); // translate error code to human readable string.

protected:
    error_code instant_save(); // For automatic trigger by "AutoSave"

    bool testPolicy(policy p) const;
    bool testPolicy(policy p, policy src) const;

    bool writePolicy(policy p) const; // write the policy to database file.
    policy readPolicy() const; // read the policy from database file.

    error_code init_db(); // Create the database file and write header.

    header makeHeader() const;
    void loadHeader(const header& h);

    error_code walwrite(const kvpair& kv); // write a key-value pair to WAL file. Not implemented yet.
    error_code waldel(const kvpair& kv); // write a delete mark for a key to WAL file. Not implemented yet.
    error_code walclear(); // clear the WAL file.
    bool walexists() const; // check whether the WAL file exists.
    error_code walapply(); // Apply the WAL file to the database file.

    bool canRead() const; // test policy to check whether read is possible.
    bool canWrite() const; // test policy to check whether write is possible.

    static bool checkPolicyValid(policy p); // check if policy is valid 

private:
    policy m_policy = policy::Null;
    std::map<std::string, value_t> m_data;
    mutable uint32_t m_dbid = 0;

    // for fast save, store the changed key-value pairs here.
    // Maybe also for wal.
    std::map<std::string, value_t> m_data_diff;

    path m_path, m_wal_path;
    mutable std::fstream m_file;

    mutable bool m_ownlock = false; // whether the lock is owned by current instance

    // header content
    size_t h_write_count = 0;
};

using db = database; // Alias for convenience

} // namespace kdb