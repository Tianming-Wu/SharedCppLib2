/*
    Another implementation of a key-value pair database.

    Optimized for certain cases. This time, the data is not as heavily compressed as the
    previous version: sizes are stored as fixed-width 64-bit values instead of a reduced
    width type, so larger data can be stored. The file produced is larger. Not
    significantly larger though.

    The underlying value type is scl2::variant, so anything scl2::variant supports can be
    stored, including std::vector and std::map. A whole tree can therefore live inside a
    single key-value pair.

    The database is bound to a file. Constructing it reads the file header only, so the
    object knows what the file is before any data is loaded. Loading is explicit: open()
    reads the data, save() and close() write it back, and the destructor closes.

    Files can be compressed and encrypted. Both are recorded in the header, because the
    payload cannot be read back without knowing what was applied to it. The secret used
    for encryption never leaves this class, and the derived keys are wiped when the
    object is destroyed.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

namespace fs = std::filesystem;

#include "enum.hpp"
#include "bytearray.hpp"
#include "compression.hpp"
#include "variant.hpp"

// std::generator is the one C++23 library feature that is still unevenly available; it
// is used by entries() and nowhere else.
#if defined(__cpp_lib_generator) && __cpp_lib_generator >= 202207L
#include <generator>
#define SCL2_XKEYDB_HAS_ENTRIES 1
#endif

namespace scl2 {

namespace xkeydb {

// ══ Algorithms ════════════════════════════════════════════════════

// What the payload was put through. Recorded in the file, since reading it back requires
// knowing it. `none` means the step is skipped.

enum class cipher_algo : uint8_t {
    none = 0,
    aes_cbc_128 = 1,
    aes_cbc_256 = 2,
};

// How the payload is checked. Also recorded in the file.
//
//   crc32, sha256  need no secret and catch accidental corruption: a bad sector, a torn
//                  write, a truncated copy. They cannot catch tampering, because whoever
//                  changes the payload can recompute them.
//   hmac_sha256    needs a secret and catches tampering as well. It comes with lock(),
//                  and it is what any cipher uses.
//
// The two cheap ones are worth reaching for: a keyed check costs a KDF pass, which is
// orders of magnitude more than a checksum. See the note on default_kdf_iterations.
enum class integrity_algo : uint8_t {
    none = 0,
    crc32 = 1,
    sha256 = 2,
    hmac_sha256 = 3,
};

// ══ Configuration ═════════════════════════════════════════════════

// Instance-only policy. Never stored in the file.
enum class inst_config : uint32_t {
    None = 0,

    // Every write operation fails, and close() writes nothing back.
    ReadOnly = 1 << 0,

    // Nothing but save() ever writes: close() leaves the changes in memory, and lock(),
    // setCompression() and setIntegrity() only take effect at the next save().
    // initialize() still writes, since it has nothing to save yet.
    NoImplicitSave = 1 << 1,
};
scl2_enum_bitopex(inst_config)

// How the database file is encoded.
struct file_config {
    cipher_algo    cipher    = cipher_algo::none;
    compression_id compress  = compress_algo::none;
    integrity_algo integrity = integrity_algo::none;

    friend bool operator==(const file_config&, const file_config&) = default;
};

struct xkeydb_config {
    file_config file;
    inst_config inst = inst_config::None;
};

// Settings that can only be chosen when the database is created.
// Encryption is deliberately absent: it needs a secret, and that goes through lock().
struct init_options {
    compression_id compress  = compress_algo::none;
    integrity_algo integrity = integrity_algo::none;

    // Reserved. A write-ahead log for removable media is planned, not implemented.
    bool wal = false;
};

// ══ Status and errors ══════════════════════════════════════════════

enum class file_status : uint8_t {
    missing,        // there is nothing at the path
    ok,             // a current xkeydb file
    not_database,   // something is there, but the magic does not match
    newer_version,  // written by a newer format version
    older_version,  // written by an older format version
    unsupported,    // ours, but this build cannot read it
    damaged,        // truncated, inconsistent sizes, or a reserved field is not zero
    inaccessible,   // the path exists but could not be read
};

enum class xkeydb_error : uint32_t {
    None = 0,
    FileNotFound = 1,
    FileReadError = 2,
    FileWriteError = 3,
    AccessDenied = 4,
    InvalidFormat = 5,
    UnsupportedFormat = 6,
    AlreadyExists = 7,

    KeyNotFound = 10,
    InvalidKey = 11,
    InvalidValue = 12,
    InvalidOperation = 13,

    IsReadOnly = 20,
    NotDB = 21,
    Corrupted = 22,
    Encrypted = 23,
    EncryptFailure = 24, // the secret is wrong, or the payload was modified

    DisabledFeature = 99,

    UnknownError = 1000,
};

// Thrown by the lifecycle and configuration calls. Preconditions and I/O problems are
// exceptions; the data operations return xkeydb_error instead.
class xkeydb_exception : public std::runtime_error {
public:
    xkeydb_exception(xkeydb_error code, const std::string& message)
        : std::runtime_error(message), m_code(code) {}

    xkeydb_error code() const noexcept { return m_code; }

private:
    xkeydb_error m_code;
};

// ══ On-disk layout ═════════════════════════════════════════════════
//
//   [ header       ]  always present, 48 bytes, plain
//   [ crypto block ]  only when the header's cipher is not `none`, 72 bytes, plain
//   [ payload      ]  header.payload_size bytes
//
// The payload is the serialized key-value data, compressed first and encrypted second.
// Ciphertext does not compress, so the other order would be pointless. The crypto block
// is plain because the payload cannot be decrypted without it.

constexpr uint16_t current_format_version = 1;

constexpr uint32_t header_magic = ( ('X') | ('K' << 8) | ('D' << 16) | ('B' << 24) ); // header magic "XKDB"

#pragma pack(push, 1)
struct xkeydb_header {
    uint32_t       magic;        // header_magic
    uint16_t       version;      // current_format_version
    uint16_t       reserved;     // must be zero; the WAL marker will live here
    cipher_algo    cipher;       // 1 byte
    compression_id compress;     // 1 byte
    integrity_algo integrity;    // 1 byte
    uint8_t        reserved2;    // must be zero
    uint32_t       dbid;
    uint64_t       write_count;  // incremented by every save
    uint64_t       key_count;
    uint64_t       payload_size; // bytes on disk, after encoding
    uint64_t       raw_size;     // bytes in memory, before encoding
};

// Key material parameters and the tag. Present when the integrity is hmac_sha256, which is
// always the case when a cipher is set. Without a cipher the iv field stays zero and is
// not used.
struct crypto_block {
    uint32_t kdf_iterations;   // PBKDF2-HMAC-SHA256 rounds
    uint16_t kdf;              // 0 == PBKDF2-HMAC-SHA256
    uint16_t reserved;         // must be zero
    uint8_t  salt[16];
    uint8_t  iv[16];
    uint8_t  tag[32];          // HMAC-SHA256 over header || this block up to tag || payload
};

// The unkeyed checksum. Present when the integrity is crc32 or sha256, which only happens
// without a cipher.
struct digest_block {
    uint8_t digest[32];        // crc32 uses the first four bytes, the rest stay zero
};
#pragma pack(pop)

static_assert(sizeof(xkeydb_header) == 48, "Error: xkeydb_header size is not 48 bytes!");
static_assert(sizeof(crypto_block) == 72, "Error: crypto_block size is not 72 bytes!");
static_assert(sizeof(digest_block) == 32, "Error: digest_block size is not 32 bytes!");
static_assert(std::is_trivially_copyable_v<xkeydb_header>, "Assertion failed: xkeydb_header not trivially copyable!");
static_assert(std::is_trivially_copyable_v<crypto_block>, "Assertion failed: crypto_block not trivially copyable!");
static_assert(std::is_trivially_copyable_v<digest_block>, "Assertion failed: digest_block not trivially copyable!");

constexpr size_t header_size = sizeof(xkeydb_header);
constexpr size_t crypto_block_size = sizeof(crypto_block);
constexpr size_t digest_block_size = sizeof(digest_block);

/// PBKDF2 rounds used when lock() is not told otherwise. Measured on this implementation
/// (which is a plain HMAC loop, not an optimised one), one round costs roughly 5 us in a
/// release build, so this is ~0.1 s per unlock. The count is stored in the file, so a
/// higher value can be chosen per database without breaking anything.
inline constexpr uint32_t default_kdf_iterations = 20000;

// ══ Database ═════════════════════════════════════════════════════════

class database {
public:
    /// Reads the file header, if there is one. Never throws and never creates anything.
    explicit database(const fs::path& path) noexcept;
    ~database();

    database(const database&) = delete;
    database& operator=(const database&) = delete;

    // ── Discovery ───────────────────────────────────────────────────
    const fs::path& path() const noexcept { return m_path; }

    /// A file is at the path. Says nothing about whether it is ours.
    bool exists() const noexcept { return m_status != file_status::missing
                                       && m_status != file_status::inaccessible; }

    /// ...and it is a current xkeydb file. This is what open() requires.
    bool valid() const noexcept { return m_status == file_status::ok; }

    /// The reason behind exists() and valid(), when they are not enough.
    file_status status() const noexcept { return m_status; }

    /// Text of a status code, for logs and diagnostics.
    static const char* describe(file_status status) noexcept;

    /// Text of an error code, for logs and diagnostics.
    static const char* describe(xkeydb_error error) noexcept;

    // ── Creation ────────────────────────────────────────────────────
    /// Creates a new, empty database and writes it out. Only legal when nothing is at
    /// the path, so an existing file is never overwritten by accident.
    void initialize(const init_options& opt = {});

    // ── Encryption ──────────────────────────────────────────────────
    /// Sets the secret to protect the database with, and saves immediately. A new salt is
    /// drawn, so this also re-keys an already protected database.
    /// Passing cipher_algo::none authenticates without encrypting, which keeps the data
    /// readable and still catches tampering.
    void lock(const scl2::bytearray& secret, cipher_algo algo = cipher_algo::aes_cbc_256,
              uint32_t kdf_iterations = default_kdf_iterations);

    /// Checks `secret` against the file and keeps the derived keys on success.
    /// Throws xkeydb_exception with EncryptFailure on a wrong secret.
    void unlock(const scl2::bytearray& secret);

    /// unlock() without the exception.
    [[nodiscard]] bool tryUnlock(const scl2::bytearray& secret) noexcept;

    /// The payload is encrypted, so its contents are hidden.
    bool isEncrypted() const noexcept { return m_config.file.cipher != cipher_algo::none; }

    /// A secret is needed to read the payload, whether or not it is encrypted.
    /// This is the check that decides whether unlock() has to be called.
    bool needsSecret() const noexcept
    {
        return m_config.file.integrity == integrity_algo::hmac_sha256;
    }

    /// A secret has been verified and the derived keys are held.
    bool isUnlocked() const noexcept { return m_unlocked; }

    // ── Lifecycle ───────────────────────────────────────────────────
    /// Loads the data. Calling it again with the same argument does nothing.
    void open(inst_config inst = inst_config::None);

    /// Writes the data back and releases the file. The loaded data stays readable, so the
    /// object behaves like a read-only snapshot afterwards. With NoImplicitSave the write
    /// is skipped and the unsaved changes stay in memory only.
    void close();

    /// close() followed by open(). A change of the read-only bit alone needs no I/O and
    /// is applied in place.
    /// @note This saves first, unless NoImplicitSave is set, in which case losing the
    ///       unsaved changes is refused rather than done quietly.
    void reopen(inst_config inst = inst_config::None);

    /// Throws away the unsaved changes and reloads the file.
    void discard();

    /// Writes the data back without closing. Nothing happens when nothing has changed
    /// since the last write.
    void save();

    /// Writes the data back even when nothing has changed. Same as save(), without the
    /// check, and the only difference on the file is a bumped write_count.
    void forceSave();

    bool isOpen() const noexcept { return m_state == state::open; }

    /// Data has been loaded, so it can be read. Also true after close().
    bool hasData() const noexcept { return m_loaded; }

    /// There are changes that have not been written to the file yet.
    bool isDirty() const noexcept { return m_dirty; }

    void setCompression(compression_id id);

    /// Chooses how the payload is checked. Takes none, crc32 or sha256; a keyed check needs
    /// a secret, so hmac_sha256 is set through lock() instead.
    void setIntegrity(integrity_algo algo);

    // ── Compression algorithms of your own ──────────────────────────

    /// Registers an algorithm under an identifier of your own, so that initialize() and
    /// setCompression() can name it afterwards.
    ///
    /// The identifier has to be at or above `user_compression_base` - the range below
    /// belongs to the library - and must not be taken yet. Both halves of the provider have
    /// to be there, since one identifier is used for reading and writing alike.
    ///
    /// Register before open(): that is where the identifier recorded in a file is looked
    /// up. Registering after initialize() is too late for that file as well - it has
    /// already been written.
    void addCompressionAlgo(compression_id id, compression_provider provider);

    /// Whether an algorithm is known for `id`, built-in or registered. False for 0, which
    /// names the absence of an algorithm rather than one.
    bool hasCompressionAlgo(compression_id id) const noexcept;

    // ── State ───────────────────────────────────────────────────────
    const xkeydb_config& config() const noexcept { return m_config; }

    /// The error the destructor swallowed, if any. None when the last save succeeded.
    xkeydb_error lastError() const noexcept { return m_lastError; }

    // ── Data ────────────────────────────────────────────────────────
    //
    // A key is an arbitrary blob, hence scl2::bytearray. They are almost always text
    // though, so every key parameter also takes a std::string_view.

    size_t keyCount() const noexcept { return m_data.size(); }

    bool hasKey(const scl2::bytearray& key) const;
    bool hasKey(std::string_view key) const;

    /// The value stored under `key`, and whether it was there at all. This is how a stored
    /// null is told apart from a missing key.
    bool get(const scl2::bytearray& key, scl2::variant& out) const;
    bool get(std::string_view key, scl2::variant& out) const;

    /// The value stored under `key`. A default variant when there is none, which a stored
    /// null looks exactly like - use get() when that matters.
    scl2::variant value(const scl2::bytearray& key) const;
    scl2::variant value(std::string_view key) const;

    /// Adds a key that is not there yet. AlreadyExists when it is.
    xkeydb_error addKey(const scl2::bytearray& key, const scl2::variant& value);
    xkeydb_error addKey(std::string_view key, const scl2::variant& value);

    /// Inserts, or overwrites what is there.
    xkeydb_error setValue(const scl2::bytearray& key, const scl2::variant& value);
    xkeydb_error setValue(std::string_view key, const scl2::variant& value);

    /// Removes a key. Removing one that is not there is not an error.
    xkeydb_error eraseKey(const scl2::bytearray& key);
    xkeydb_error eraseKey(std::string_view key);

    /// Removes every key. Destructive, and nothing reaches the file until save().
    xkeydb_error clear();

    /// Every key, in lexicographic order. A snapshot: inserting while walking it is fine.
    std::vector<scl2::bytearray> keys() const;

    /// Every key that starts with `prefix`, in lexicographic order. Also a snapshot.
    std::vector<scl2::bytearray> keys(std::string_view prefix) const;

#ifdef SCL2_XKEYDB_HAS_ENTRIES
    /// Every pair, in key order, one at a time. Yields copies.
    /// @note Unlike keys(), this walks the live database, so inserting or removing
    ///       between two yielded pairs invalidates the walk. Take keys() first when that
    ///       can happen. The generator borrows from this object and must not outlive it.
    std::generator<std::pair<scl2::bytearray, scl2::variant>> entries() const;
#endif

    // ── Copying, moving and deleting the file ───────────────────────
    //
    // All four keep the encoded format as it is and flush before returning. None of them
    // consults the read-only policy: they act on the file rather than on its contents.

    /// Writes the database to another path and leaves it there. The open database is not
    /// disturbed - not its path, not its dirty flag, not its header.
    xkeydb_error takeBackup(const fs::path& backup_path);

    /// Writes the database to another path and moves the handle there, so the work that
    /// follows happens on the copy. The original file is left behind untouched. `inst`
    /// becomes the instance policy of the new handle, and defaults to read-write, which
    /// is how a read-only database can be taken somewhere it may be edited.
    /// Fails with AlreadyExists when something is already at the path.
    xkeydb_error saveAs(const fs::path& path, inst_config inst = inst_config::None);

    /// Moves the database file itself: the image goes to the new path, the old file is
    /// removed, and the handle follows. Keeps the current instance policy.
    /// Fails with AlreadyExists when something is already at the path, and leaves the old
    /// file in place when the new one cannot be written.
    xkeydb_error moveTo(const fs::path& path);

    /// Deletes the file and hands back the object in the state it had right after
    /// construction, as if nothing had ever been at the path. initialize() works again
    /// afterwards.
    xkeydb_error destroy();

private:
    enum class state : uint8_t { vacant, closed, open };

    void readHeader() noexcept;

    bool writable() const noexcept;
    bool testInst(inst_config flag) const noexcept;
    void requireOpenWritable(const char* what) const;

    /// The provider for `id`, or nullptr when `id` is 0 or nothing is registered for it.
    const compression_provider* providerFor(compression_id id) const noexcept;

    /// Throws UnsupportedFormat when `id` names an algorithm that is not registered.
    void requireCompression(compression_id id, const char* what) const;

    void deriveKeys(const scl2::bytearray& secret);
    bool verifySecret(const scl2::bytearray& secret);

    scl2::bytearray computeIntegrity(const scl2::bytearray& header_bytes, const crypto_block* blk,
                                      const scl2::bytearray& payload) const;

    scl2::bytearray readWholeFile() const;

    scl2::bytearray buildImage(xkeydb_header& header_out, crypto_block& crypto_out) const;
    void writeImage(const scl2::bytearray& image) const;
    void writeOut();

    scl2::bytearray encode(const scl2::bytearray& raw, crypto_block& blk) const;
    scl2::bytearray decode(const xkeydb_header& hdr, const crypto_block& blk,
                           const scl2::bytearray& payload) const;

    scl2::bytearray _dump() const;         // the data section, as bytes
    void _load(scl2::bytearray& data);     // bytes, as the data section

    fs::path    m_path;
    file_status m_status = file_status::missing;

    state m_state  = state::vacant;
    bool  m_loaded = false;  // the data section has been parsed at least once
    bool  m_dirty  = false;

    xkeydb_config m_config;
    xkeydb_error  m_lastError = xkeydb_error::None;

    xkeydb_header m_header{};  // as on disk
    crypto_block  m_crypto{};  // as on disk; the iv is refreshed by every save

    scl2::secure_bytearray m_encKey;
    scl2::secure_bytearray m_macKey;
    bool m_unlocked = false;

    std::map<scl2::bytearray, scl2::variant> m_data;

    /// Algorithms registered by the caller, keyed by the identifier a file records.
    std::map<uint8_t, compression_provider> m_providers;
};


} // namespace xkeydb

} // namespace scl2