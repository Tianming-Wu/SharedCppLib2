#include "xkeydb.hpp"

#include <cstring>
#include <fstream>
#include <random>
#include <stdexcept>

#include "aes.hpp"
#include "crc32.hpp"
#include "fileio.hpp"
#include "hmac.hpp"
#include "platform.hpp" // OS_WINDOWS, and windows.h on Windows
#include "sha256.hpp"
#include "zlib.hpp"

namespace scl2::xkeydb {

namespace {

constexpr uint16_t kdf_pbkdf2_sha256 = 0;

constexpr size_t kdf_salt_size = 16;
constexpr size_t kdf_iv_size = 16;
constexpr size_t kdf_mac_key_size = 32; // HMAC-SHA256 output

constexpr size_t cipher_key_size(cipher_algo a) noexcept
{
    switch (a) {
    case cipher_algo::aes_cbc_128: return 16;
    case cipher_algo::aes_cbc_256: return 32;
    default:                       return 0;
    }
}

constexpr bool cipher_known(cipher_algo a) noexcept
{
    return a == cipher_algo::none || cipher_key_size(a) != 0;
}

constexpr bool compress_id_known(compression_id id) noexcept
{
    // An application's identifier always passes: what it means is decided by the registry,
    // and registering happens after the header has already been read.
    if (id.isUser()) return true;

    switch (static_cast<compress_algo>(id.value())) {
    case compress_algo::none:
    case compress_algo::zlib:  return true;
    default:                   return false;
    }
}

constexpr bool integrity_known(integrity_algo a) noexcept
{
    return a == integrity_algo::none || a == integrity_algo::crc32
        || a == integrity_algo::sha256 || a == integrity_algo::hmac_sha256;
}

// A keyed check keeps its parameters in the crypto block, an unkeyed one only needs its
// digest. A file never carries both.
constexpr bool integrity_is_keyed(integrity_algo a) noexcept
{
    return a == integrity_algo::hmac_sha256;
}

constexpr bool integrity_is_unkeyed(integrity_algo a) noexcept
{
    return a == integrity_algo::crc32 || a == integrity_algo::sha256;
}

xkeydb_error status_to_error(file_status s) noexcept
{
    switch (s) {
    case file_status::missing:       return xkeydb_error::FileNotFound;
    case file_status::ok:            return xkeydb_error::None;
    case file_status::not_database:  return xkeydb_error::NotDB;
    case file_status::newer_version:
    case file_status::older_version:
    case file_status::unsupported:   return xkeydb_error::UnsupportedFormat;
    case file_status::damaged:       return xkeydb_error::InvalidFormat;
    case file_status::inaccessible:  return xkeydb_error::AccessDenied;
    }
    return xkeydb_error::UnknownError;
}

scl2::bytearray to_bytes(const void* p, size_t n) { return scl2::bytearray(p, n); }

/// The built-ins, held as providers, so that a built-in identifier and an application's go
/// through the same path. A captureless lambda fits in std::function without allocating.
const compression_provider builtin_zlib = compression_provider::from<scl2::zlib>();

/// The provider behind a built-in identifier, or nullptr when there is none.
const compression_provider* builtin_provider(compress_algo algo) noexcept
{
    switch (algo) {
    case compress_algo::zlib: return &builtin_zlib;
    default:                  return nullptr;
    }
}

scl2::bytearray key_from(std::string_view key)
{
    return scl2::bytearray(key.data(), key.size());
}

bool starts_with(const scl2::bytearray& key, const scl2::bytearray& prefix)
{
    if (prefix.empty()) return true;
    if (key.size() < prefix.size()) return false;

    return std::memcmp(key.data(), prefix.data(), prefix.size()) == 0;
}

void fill_random(void* dst, size_t n)
{
    scl2::bytearray r = scl2::bytearray::randomarray(n);
    std::memcpy(dst, r.data(), n);
    r.wipe();
}

/// PBKDF2-HMAC-SHA256 (RFC 8018). The iteration count is stored in the file, so it can be
/// raised later without locking anyone out of an existing database.
scl2::bytearray pbkdf2_sha256(const scl2::bytearray& password, const scl2::bytearray& salt,
                              uint32_t iterations, size_t out_len)
{
    if (iterations == 0) iterations = 1;

    constexpr size_t hlen = 32; // SHA-256
    const size_t blocks = (out_len + hlen - 1) / hlen;

    scl2::bytearray out;
    out.reserve(blocks * hlen);

    for (size_t i = 1; i <= blocks; ++i) {
        const uint32_t idx = static_cast<uint32_t>(i);
        const std::byte be[4] = {
            std::byte{static_cast<unsigned char>((idx >> 24) & 0xFFu)},
            std::byte{static_cast<unsigned char>((idx >> 16) & 0xFFu)},
            std::byte{static_cast<unsigned char>((idx >>  8) & 0xFFu)},
            std::byte{static_cast<unsigned char>( idx        & 0xFFu)},
        };

        scl2::bytearray msg = salt;
        msg.append(be, 4);

        scl2::bytearray u = scl2::hmac<scl2::sha256>::compute(msg, password);
        scl2::bytearray t = u;

        for (uint32_t j = 1; j < iterations; ++j) {
            u = scl2::hmac<scl2::sha256>::compute(u, password);
            for (size_t k = 0; k < hlen; ++k) t[k] = t[k] ^ u[k];
        }

        out.append(t);
    }

    out.resize(out_len);
    return out;
}

scl2::bytearray cipher_apply(bool encrypt, cipher_algo algo, const scl2::bytearray& data,
                             const scl2::bytearray& key, const scl2::bytearray& iv)
{
    scl2::bytearray k = key;
    k.append(iv); // aes_cbc takes key || iv

    switch (algo) {
    case cipher_algo::aes_cbc_128:
        return encrypt ? scl2::aes_cbc_128::encrypt(data, k) : scl2::aes_cbc_128::decrypt(data, k);
    case cipher_algo::aes_cbc_256:
        return encrypt ? scl2::aes_cbc_256::encrypt(data, k) : scl2::aes_cbc_256::decrypt(data, k);
    default:
        throw xkeydb_exception(xkeydb_error::DisabledFeature, "xkeydb: unsupported cipher");
    }
}

bool constant_time_equal(const scl2::bytearray& a, const scl2::bytearray& b)
{
    if (a.size() != b.size()) return false;

    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

// Reading through readBytes() rather than read<T>() keeps the call unambiguous: read<T>()
// has a second overload for generic_load types.
uint64_t take_u64(scl2::bytearray& d)
{
    if (d.remaining() < sizeof(uint64_t))
        throw xkeydb_exception(xkeydb_error::InvalidFormat, "xkeydb: the data section ends early");

    const scl2::bytearray raw = d.readBytes(sizeof(uint64_t));
    uint64_t v = 0;
    std::memcpy(&v, raw.data(), sizeof(v));
    return v;
}

scl2::bytearray take_bytes(scl2::bytearray& d, uint64_t length)
{
    if (length > d.remaining())
        throw xkeydb_exception(xkeydb_error::InvalidFormat, "xkeydb: a length in the data section is out of range");

    return d.readBytes(static_cast<size_t>(length));
}

} // namespace

// ══ Discovery ════════════════════════════════════════════════════════

database::database(const fs::path& path) noexcept
    : m_path(path)
{
    readHeader();
}

database::~database()
{
    if (m_state != state::open) return;

    // Nothing can be reported from here, so the error is kept for lastError().
    try { close(); }
    catch (const xkeydb_exception& e) { m_lastError = e.code(); }
    catch (...) { m_lastError = xkeydb_error::UnknownError; }
}

void database::readHeader() noexcept
{
    std::error_code ec;

    if (!fs::exists(m_path, ec)) {
        m_status = ec ? file_status::inaccessible : file_status::missing;
        return;
    }
    if (!fs::is_regular_file(m_path, ec) || ec) {
        m_status = file_status::inaccessible;
        return;
    }

    std::ifstream ifs(m_path, std::ios::binary);
    if (!ifs) {
        m_status = file_status::inaccessible;
        return;
    }

    xkeydb_header hdr{};
    ifs.read(reinterpret_cast<char*>(&hdr), static_cast<std::streamsize>(sizeof(hdr)));
    const std::streamsize got = ifs.gcount();

    // The magic is checked before the length, so a file that is simply not ours is told
    // apart from one of ours that got truncated.
    if (got < 4 || hdr.magic != header_magic) {
        m_status = file_status::not_database;
        return;
    }

    if (got != static_cast<std::streamsize>(sizeof(hdr))) {
        m_status = file_status::damaged;
        return;
    }

    if (hdr.version != current_format_version) {
        m_status = (hdr.version > current_format_version) ? file_status::newer_version
                                                          : file_status::older_version;
        return;
    }

    // Reserved fields have to be zero, so a file from a version that reuses them is
    // rejected instead of being read with the wrong meaning.
    if (hdr.reserved != 0 || hdr.reserved2 != 0) {
        m_status = file_status::damaged;
        return;
    }

    if (!cipher_known(hdr.cipher) || !compress_id_known(hdr.compress) || !integrity_known(hdr.integrity)) {
        m_status = file_status::unsupported;
        return;
    }

    // A cipher always brings its own keyed check along.
    if (hdr.cipher != cipher_algo::none && hdr.integrity != integrity_algo::hmac_sha256) {
        m_status = file_status::unsupported;
        return;
    }

    // The sizes have to describe the file exactly; anything else means it is not intact.
    const uint64_t expected = static_cast<uint64_t>(header_size)
                            + (integrity_is_keyed(hdr.integrity) ? static_cast<uint64_t>(crypto_block_size) : 0u)
                            + (integrity_is_unkeyed(hdr.integrity) ? static_cast<uint64_t>(digest_block_size) : 0u)
                            + hdr.payload_size;
    const uint64_t actual = fs::file_size(m_path, ec);
    if (ec || actual != expected) {
        m_status = file_status::damaged;
        return;
    }

    m_header = hdr;
    m_config.file.cipher = hdr.cipher;
    m_config.file.compress = hdr.compress;
    m_config.file.integrity = hdr.integrity;
    m_state = state::closed;
    m_status = file_status::ok;
}

const char* database::describe(file_status status) noexcept
{
    switch (status) {
    case file_status::missing:       return "there is no file at the path";
    case file_status::ok:            return "ok";
    case file_status::not_database:  return "not a xkeydb file";
    case file_status::newer_version: return "written by a newer format version";
    case file_status::older_version: return "written by an older format version";
    case file_status::unsupported:   return "unsupported encoding";
    case file_status::damaged:       return "damaged";
    case file_status::inaccessible:  return "the path could not be read";
    }
    return "unknown";
}

const char* database::describe(xkeydb_error error) noexcept
{
    switch (error) {
    case xkeydb_error::None:              return "none";
    case xkeydb_error::FileNotFound:      return "no such file";
    case xkeydb_error::FileReadError:     return "the file could not be read";
    case xkeydb_error::FileWriteError:    return "the file could not be written";
    case xkeydb_error::AccessDenied:      return "access denied";
    case xkeydb_error::InvalidFormat:     return "the file is not intact";
    case xkeydb_error::UnsupportedFormat: return "the encoding is not supported";
    case xkeydb_error::AlreadyExists:     return "something is already there";
    case xkeydb_error::KeyNotFound:       return "no such key";
    case xkeydb_error::InvalidKey:        return "the key is not usable";
    case xkeydb_error::InvalidValue:      return "the value is not usable";
    case xkeydb_error::InvalidOperation:  return "the call does not fit the current state";
    case xkeydb_error::IsReadOnly:        return "the database is open read-only";
    case xkeydb_error::NotDB:             return "not a xkeydb file";
    case xkeydb_error::Corrupted:         return "the checksum does not match";
    case xkeydb_error::Encrypted:         return "a secret is needed";
    case xkeydb_error::EncryptFailure:    return "the secret is wrong, or the file was modified";
    case xkeydb_error::DisabledFeature:   return "the feature is not available";
    case xkeydb_error::UnknownError:      return "unknown error";
    }
    return "unknown";
}

// ══ Preconditions ════════════════════════════════════════════════════

bool database::testInst(inst_config flag) const noexcept
{
    return (static_cast<uint32_t>(m_config.inst) & static_cast<uint32_t>(flag))
        == static_cast<uint32_t>(flag);
}

const compression_provider* database::providerFor(compression_id id) const noexcept
{
    if (id.isNone()) return nullptr;

    if (id.isBuiltin()) return builtin_provider(static_cast<compress_algo>(id.value()));

    const auto it = m_providers.find(id.value());
    return it == m_providers.end() ? nullptr : &it->second;
}

void database::requireCompression(compression_id id, const char* what) const
{
    if (id.isNone()) return;
    if (providerFor(id)) return;

    throw xkeydb_exception(xkeydb_error::UnsupportedFormat,
        std::string("xkeydb::") + what + ": no provider is registered for compression id "
        + std::to_string(id.value()));
}

void database::addCompressionAlgo(compression_id id, compression_provider provider)
{
    if (id.isNone() || id.isBuiltin())
        throw xkeydb_exception(xkeydb_error::InvalidValue,
            "xkeydb::addCompressionAlgo: identifiers below user_compression_base belong to the library");
    if (!provider.isUsable())
        throw xkeydb_exception(xkeydb_error::InvalidValue,
            "xkeydb::addCompressionAlgo: the provider needs both a compress and a decompress function");
    if (m_state == state::open)
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb::addCompressionAlgo: register before open()");
    if (m_providers.find(id.value()) != m_providers.end())
        throw xkeydb_exception(xkeydb_error::AlreadyExists,
            "xkeydb::addCompressionAlgo: that identifier is already registered");

    m_providers.emplace(id.value(), std::move(provider));
}

bool database::hasCompressionAlgo(compression_id id) const noexcept
{
    return providerFor(id) != nullptr;
}

bool database::writable() const noexcept
{
    return m_state == state::open && !testInst(inst_config::ReadOnly);
}

void database::requireOpenWritable(const char* what) const
{
    if (m_state != state::open)
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            std::string("xkeydb::") + what + ": the database is not open");

    if (testInst(inst_config::ReadOnly))
        throw xkeydb_exception(xkeydb_error::IsReadOnly,
            std::string("xkeydb::") + what + ": the database is open read-only");
}

// ══ Creation ═════════════════════════════════════════════════════════

void database::initialize(const init_options& opt)
{
    if (exists())
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb::initialize: there is already a file at " + m_path.string());
    if (opt.wal)
        throw xkeydb_exception(xkeydb_error::DisabledFeature,
            "xkeydb::initialize: the write-ahead log is not implemented");
    if (!compress_id_known(opt.compress))
        throw xkeydb_exception(xkeydb_error::DisabledFeature,
            "xkeydb::initialize: unsupported compression algorithm");
    requireCompression(opt.compress, "initialize");
    if (!integrity_known(opt.integrity) || opt.integrity == integrity_algo::hmac_sha256)
        throw xkeydb_exception(xkeydb_error::DisabledFeature,
            "xkeydb::initialize: unsupported integrity algorithm");

    m_header = xkeydb_header{};
    m_header.magic = header_magic;
    m_header.version = current_format_version;
    m_header.dbid = std::random_device{}();

    m_crypto = crypto_block{};
    m_config = xkeydb_config{};
    m_config.file.compress = opt.compress;
    m_config.file.integrity = opt.integrity;

    m_data.clear();
    m_loaded = true;
    m_dirty = true;
    m_unlocked = false;
    m_state = state::open;
    m_status = file_status::ok;

    try {
        save();
    }
    catch (...) {
        // Nothing usable was created, so the object goes back to where it started.
        m_state = state::vacant;
        m_status = file_status::missing;
        throw;
    }
}

// ══ Encryption ═══════════════════════════════════════════════════════

void database::lock(const scl2::bytearray& secret, cipher_algo algo, uint32_t kdf_iterations)
{
    requireOpenWritable("lock");

    if (!cipher_known(algo))
        throw xkeydb_exception(xkeydb_error::DisabledFeature, "xkeydb::lock: unsupported cipher");
    if (kdf_iterations == 0)
        throw xkeydb_exception(xkeydb_error::InvalidValue,
            "xkeydb::lock: the iteration count must not be zero");

    // A fresh salt and iteration count belong to the new key. Applying lock() to a
    // database that is already protected therefore re-keys it.
    m_crypto = crypto_block{};
    m_crypto.kdf = kdf_pbkdf2_sha256;
    m_crypto.kdf_iterations = kdf_iterations;
    fill_random(m_crypto.salt, kdf_salt_size);

    m_config.file.cipher = algo;
    m_config.file.integrity = integrity_algo::hmac_sha256;
    deriveKeys(secret);
    m_unlocked = true;

    // The new configuration belongs to the file. Without NoImplicitSave it goes out now;
    // with it, it stays pending, which is what the dirty flag is for.
    m_dirty = true;
    if (!testInst(inst_config::NoImplicitSave)) save();
}

void database::unlock(const scl2::bytearray& secret)
{
    if (m_state == state::open)
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb::unlock: the database is already open");

    if (m_state != state::closed || m_status != file_status::ok)
        throw xkeydb_exception(status_to_error(m_status),
            std::string("xkeydb::unlock: ") + describe(m_status));

    if (!needsSecret())
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb::unlock: the database needs no secret");

    if (!verifySecret(secret)) {
        m_unlocked = false;
        m_encKey.wipe();
        m_macKey.wipe();
        throw xkeydb_exception(xkeydb_error::EncryptFailure,
            "xkeydb::unlock: wrong secret, or the file was modified");
    }

    m_unlocked = true;
}

bool database::tryUnlock(const scl2::bytearray& secret) noexcept
{
    try {
        unlock(secret);
        return true;
    }
    catch (...) {
        m_unlocked = false;
        return false;
    }
}

void database::deriveKeys(const scl2::bytearray& secret)
{
    if (m_crypto.kdf != kdf_pbkdf2_sha256)
        throw xkeydb_exception(xkeydb_error::UnsupportedFormat,
            "xkeydb: unsupported key derivation function");

    // Without a cipher there is no encryption key, only the one for the tag.
    const size_t klen = cipher_key_size(m_config.file.cipher);
    if (m_config.file.cipher != cipher_algo::none && klen == 0)
        throw xkeydb_exception(xkeydb_error::UnsupportedFormat, "xkeydb: unsupported cipher");

    const scl2::bytearray salt = to_bytes(m_crypto.salt, kdf_salt_size);
    scl2::bytearray k = pbkdf2_sha256(secret, salt, m_crypto.kdf_iterations, klen + kdf_mac_key_size);

    // The encryption key and the authentication key are separate, and the secret itself is
    // not kept: only what is needed to write and check the next payload. Assigning over
    // the old keys erases them first, see secure_bytearray's move assignment.
    m_encKey = scl2::secure_bytearray(k.data(), klen);
    m_macKey = scl2::secure_bytearray(k.data() + klen, kdf_mac_key_size);
    k.wipe();
}

bool database::verifySecret(const scl2::bytearray& secret)
{
    const scl2::bytearray file = readWholeFile();

    crypto_block blk{};
    std::memcpy(&blk, file.data() + header_size, crypto_block_size);

    const scl2::bytearray header_bytes = file.subarr(0, header_size);
    const scl2::bytearray payload = file.subarr(header_size + crypto_block_size, m_header.payload_size);

    m_crypto = blk;
    deriveKeys(secret);

    // Verifying the tag does not require decrypting anything.
    return constant_time_equal(computeIntegrity(header_bytes, &blk, payload),
                               to_bytes(blk.tag, sizeof(blk.tag)));
}

scl2::bytearray database::computeIntegrity(const scl2::bytearray& header_bytes,
                                            const crypto_block* blk,
                                            const scl2::bytearray& payload) const
{
    scl2::bytearray message = header_bytes;
    if (blk) message.append(reinterpret_cast<const std::byte*>(blk), offsetof(crypto_block, tag));
    message.append(payload);

    switch (m_config.file.integrity) {
    case integrity_algo::crc32:       return scl2::crc32::hash(message);
    case integrity_algo::sha256:      return scl2::sha256::hash(message);
    case integrity_algo::hmac_sha256: return scl2::hmac<scl2::sha256>::compute(message, m_macKey);
    case integrity_algo::none:
    default:
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb: no integrity check is configured");
    }
}

// ══ Lifecycle ════════════════════════════════════════════════════════

void database::open(inst_config inst)
{
    if (m_state == state::open) {
        if (m_config.inst == inst) return; // the same call twice
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb::open: already open with a different instance config; use reopen()");
    }

    if (m_state != state::closed || m_status != file_status::ok)
        throw xkeydb_exception(status_to_error(m_status),
            std::string("xkeydb::open: ") + describe(m_status));

    if (needsSecret() && !m_unlocked)
        throw xkeydb_exception(xkeydb_error::Encrypted,
            "xkeydb::open: the database needs a secret; call unlock() first");

    // Checked here rather than in the middle of decoding, so a missing algorithm is
    // reported before the file is even read.
    requireCompression(m_config.file.compress, "open");

    const scl2::bytearray file = readWholeFile();

    crypto_block blk{};
    digest_block dig{};
    size_t offset = header_size;

    if (integrity_is_keyed(m_config.file.integrity)) {
        std::memcpy(&blk, file.data() + offset, crypto_block_size);
        offset += crypto_block_size;
    }
    else if (integrity_is_unkeyed(m_config.file.integrity)) {
        std::memcpy(&dig, file.data() + offset, digest_block_size);
        offset += digest_block_size;
    }

    const scl2::bytearray header_bytes = file.subarr(0, header_size);
    const scl2::bytearray payload = file.subarr(offset, m_header.payload_size);

    if (m_config.file.integrity != integrity_algo::none) {
        const bool keyed = integrity_is_keyed(m_config.file.integrity);
        const scl2::bytearray expected = computeIntegrity(header_bytes, keyed ? &blk : nullptr, payload);
        const scl2::bytearray stored = keyed ? to_bytes(blk.tag, sizeof(blk.tag))
                                             : to_bytes(dig.digest, expected.size());

        if (!constant_time_equal(expected, stored)) {
            if (keyed)
                throw xkeydb_exception(xkeydb_error::EncryptFailure,
                    "xkeydb::open: wrong secret, or the file was modified");
            throw xkeydb_exception(xkeydb_error::Corrupted,
                "xkeydb::open: the checksum does not match; the file changed on disk");
        }
    }

    if (integrity_is_keyed(m_config.file.integrity)) m_crypto = blk;

    scl2::bytearray raw = decode(m_header, blk, payload);

    _load(raw);

    m_config.inst = inst;
    m_state = state::open;
    m_loaded = true;
    m_dirty = false;
    m_lastError = xkeydb_error::None;
}

void database::close()
{
    if (m_state != state::open) return; // idempotent, and never creates a file

    // With NoImplicitSave the changes stay in memory and no longer reach the file on
    // their own; the caller has taken over that decision.
    if (writable() && m_dirty && !testInst(inst_config::NoImplicitSave)) save();

    m_state = state::closed;
    m_config.inst = inst_config::None;
}

void database::discard()
{
    if (m_state == state::vacant) return; // nothing is loaded, so there is nothing to undo
    if (!m_dirty) return;                 // nothing was changed

    // Reloading is what open() does, and it clears the dirty flag on the way.
    const inst_config inst = m_config.inst;
    m_state = state::closed;
    open(inst);
}

void database::reopen(inst_config inst)
{
    if (m_state == state::open && m_config.inst == inst) return;

    // Only the read-only bit differs: that is a property of this handle, not of the data,
    // so nothing has to be written out or read back.
    if (m_state == state::open
        && (static_cast<uint32_t>(m_config.inst ^ inst)
            & ~static_cast<uint32_t>(inst_config::ReadOnly)) == 0) {
        m_config.inst = inst;
        return;
    }

    // close() would not write, and open() would then reload over the unsaved changes.
    if (m_dirty && testInst(inst_config::NoImplicitSave))
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb::reopen: there are unsaved changes and NoImplicitSave is set; "
            "call save() or discard() first");

    close();
    open(inst);
}

void database::setCompression(compression_id id)
{
    requireOpenWritable("setCompression");

    if (!compress_id_known(id))
        throw xkeydb_exception(xkeydb_error::DisabledFeature,
            "xkeydb::setCompression: unsupported algorithm");
    requireCompression(id, "setCompression");
    if (m_config.file.compress == id) return;

    m_config.file.compress = id;
    m_dirty = true;
    if (!testInst(inst_config::NoImplicitSave)) save();
}

void database::setIntegrity(integrity_algo algo)
{
    requireOpenWritable("setIntegrity");

    if (!integrity_known(algo))
        throw xkeydb_exception(xkeydb_error::DisabledFeature,
            "xkeydb::setIntegrity: unsupported algorithm");
    if (algo == integrity_algo::hmac_sha256)
        throw xkeydb_exception(xkeydb_error::InvalidOperation,
            "xkeydb::setIntegrity: a keyed check needs a secret; use lock(secret, cipher_algo::none)");
    if (m_config.file.integrity == algo) return;

    m_config.file.integrity = algo;
    m_dirty = true;
    if (!testInst(inst_config::NoImplicitSave)) save();
}

// ══ Reading and writing ══════════════════════════════════════════════

scl2::bytearray database::readWholeFile() const
{
    try {
        return scl2::readFile(m_path);
    }
    catch (const std::exception&) {
        throw xkeydb_exception(xkeydb_error::FileReadError, "xkeydb: cannot read " + m_path.string());
    }
}

scl2::bytearray database::encode(const scl2::bytearray& raw, crypto_block& blk) const
{
    scl2::bytearray out = raw;

    if (!m_config.file.compress.isNone()) {
        const compression_provider* provider = providerFor(m_config.file.compress);
        if (!provider)
            throw xkeydb_exception(xkeydb_error::UnsupportedFormat,
                "xkeydb: no provider is registered for compression id "
                + std::to_string(m_config.file.compress.value()));

        out = provider->compress(out);
    }

    if (m_config.file.cipher != cipher_algo::none) {
        if (!m_unlocked)
            throw xkeydb_exception(xkeydb_error::Encrypted, "xkeydb: no secret is set");

        out = cipher_apply(true, m_config.file.cipher, out, m_encKey, to_bytes(blk.iv, kdf_iv_size));
    }

    return out;
}

scl2::bytearray database::decode(const xkeydb_header& hdr, const crypto_block& blk,
                                 const scl2::bytearray& payload) const
{
    scl2::bytearray out = payload;

    if (hdr.cipher != cipher_algo::none)
        out = cipher_apply(false, hdr.cipher, out, m_encKey, to_bytes(blk.iv, kdf_iv_size));

    if (!hdr.compress.isNone()) {
        const compression_provider* provider = providerFor(hdr.compress);
        if (!provider)
            throw xkeydb_exception(xkeydb_error::UnsupportedFormat,
                "xkeydb: no provider is registered for compression id "
                + std::to_string(hdr.compress.value()));

        out = provider->decompress(out);
    }

    if (out.size() != hdr.raw_size)
        throw xkeydb_exception(xkeydb_error::InvalidFormat,
            "xkeydb: the payload size does not match the header");

    return out;
}

scl2::bytearray database::buildImage(xkeydb_header& header_out, crypto_block& crypto_out) const
{
    const scl2::bytearray raw = _dump();

    const bool keyed = integrity_is_keyed(m_config.file.integrity);
    const bool unkeyed = integrity_is_unkeyed(m_config.file.integrity);

    crypto_block blk = m_crypto;
    if (keyed) {
        blk.kdf = kdf_pbkdf2_sha256;
        blk.reserved = 0;
        if (blk.kdf_iterations == 0) blk.kdf_iterations = default_kdf_iterations;

        if (m_config.file.cipher != cipher_algo::none) {
            // A fresh iv for every save, so two saves of the same data do not produce the
            // same ciphertext.
            fill_random(blk.iv, kdf_iv_size);
        }
        else {
            std::memset(blk.iv, 0, sizeof(blk.iv)); // authenticated, but not encrypted
        }

        std::memset(blk.tag, 0, sizeof(blk.tag));
    }

    const scl2::bytearray payload = encode(raw, blk);

    xkeydb_header hdr{};
    hdr.magic = header_magic;
    hdr.version = current_format_version;
    hdr.cipher = m_config.file.cipher;
    hdr.compress = m_config.file.compress;
    hdr.integrity = m_config.file.integrity;
    hdr.dbid = m_header.dbid;
    hdr.write_count = m_header.write_count + 1;
    hdr.key_count = m_data.size();
    hdr.payload_size = payload.size();
    hdr.raw_size = raw.size();

    scl2::bytearray image;
    image.append(hdr);

    // Everything the check covers is final by now, so nothing has to be patched afterwards.
    const scl2::bytearray header_bytes = image.subarr(0, header_size);

    if (keyed) {
        const scl2::bytearray tag = computeIntegrity(header_bytes, &blk, payload);
        std::memcpy(blk.tag, tag.data(), sizeof(blk.tag));
        image.append(blk);
    }
    else if (unkeyed) {
        digest_block dig{};
        const scl2::bytearray digest = computeIntegrity(header_bytes, nullptr, payload);
        std::memcpy(dig.digest, digest.data(), digest.size());
        image.append(dig);
    }

    image.append(payload);

    header_out = hdr;
    crypto_out = blk;
    return image;
}

void database::writeImage(const scl2::bytearray& image) const
{
    fs::path tmp = m_path;
    tmp += ".tmp";

    try {
        // Flush before replacing: handing the bytes to the operating system is not enough,
        // a power loss could still leave the new name pointing at data that never landed.
        scl2::writeFile(tmp, image, true);
    }
    catch (const std::exception&) {
        throw xkeydb_exception(xkeydb_error::FileWriteError, "xkeydb: cannot write " + tmp.string());
    }

    // Replace in one step, so a reader never sees a half-written database. The temporary
    // file has to sit next to the target for this to be a rename rather than a copy.
#ifdef OS_WINDOWS
    if (!MoveFileExW(tmp.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        throw xkeydb_exception(xkeydb_error::FileWriteError, "xkeydb: cannot replace " + m_path.string());
    }
#else
    std::error_code ec;
    fs::rename(tmp, m_path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        throw xkeydb_exception(xkeydb_error::FileWriteError, "xkeydb: cannot replace " + m_path.string());
    }
#endif
}

void database::writeOut()
{
    xkeydb_header hdr{};
    crypto_block blk{};
    const scl2::bytearray image = buildImage(hdr, blk);

    writeImage(image);

    m_header = hdr;
    m_crypto = blk;
    m_dirty = false;
    m_loaded = true;
    m_status = file_status::ok;
}

void database::save()
{
    requireOpenWritable("save");

    if (!m_dirty) return; // there is nothing to write
    writeOut();
}

void database::forceSave()
{
    requireOpenWritable("forceSave");

    writeOut();
}

// ══ Data section ═════════════════════════════════════════════════════

scl2::bytearray database::_dump() const
{
    scl2::bytearray out;

    for (const auto& [key, value] : m_data) {
        out.append<uint64_t>(key.size());
        out.append(key);

        const scl2::bytearray value_bytes = value.dump();
        out.append<uint64_t>(value_bytes.size());
        out.append(value_bytes);
    }

    return out;
}

void database::_load(scl2::bytearray& data)
{
    m_data.clear();

    for (uint64_t i = 0; i < m_header.key_count; ++i) {
        const scl2::bytearray key = take_bytes(data, take_u64(data));
        const scl2::bytearray value_bytes = take_bytes(data, take_u64(data));

        m_data[key] = scl2::variant::load(value_bytes);
    }

    if (data.remaining() != 0)
        throw xkeydb_exception(xkeydb_error::InvalidFormat, "xkeydb: trailing bytes in the data section");
}

// ══ Data ═════════════════════════════════════════════════════════════

bool database::hasKey(const scl2::bytearray& key) const
{
    return m_data.find(key) != m_data.end();
}

bool database::hasKey(std::string_view key) const
{
    return hasKey(key_from(key));
}

bool database::get(const scl2::bytearray& key, scl2::variant& out) const
{
    const auto it = m_data.find(key);
    if (it == m_data.end()) return false;

    out = it->second;
    return true;
}

bool database::get(std::string_view key, scl2::variant& out) const
{
    return get(key_from(key), out);
}

scl2::variant database::value(const scl2::bytearray& key) const
{
    const auto it = m_data.find(key);
    return it == m_data.end() ? scl2::variant() : it->second;
}

scl2::variant database::value(std::string_view key) const
{
    return value(key_from(key));
}

xkeydb_error database::addKey(const scl2::bytearray& key, const scl2::variant& value)
{
    if (m_state != state::open) return xkeydb_error::InvalidOperation;
    if (testInst(inst_config::ReadOnly)) return xkeydb_error::IsReadOnly;
    if (m_data.find(key) != m_data.end()) return xkeydb_error::AlreadyExists;

    m_data[key] = value;
    m_dirty = true;
    return xkeydb_error::None;
}

xkeydb_error database::addKey(std::string_view key, const scl2::variant& value)
{
    return addKey(key_from(key), value);
}

xkeydb_error database::setValue(const scl2::bytearray& key, const scl2::variant& value)
{
    if (m_state != state::open) return xkeydb_error::InvalidOperation;
    if (testInst(inst_config::ReadOnly)) return xkeydb_error::IsReadOnly;

    // Nothing changed, so there is nothing to write out later either.
    const auto it = m_data.find(key);
    if (it != m_data.end() && it->second == value) return xkeydb_error::None;

    m_data[key] = value;
    m_dirty = true;
    return xkeydb_error::None;
}

xkeydb_error database::setValue(std::string_view key, const scl2::variant& value)
{
    return setValue(key_from(key), value);
}

xkeydb_error database::eraseKey(const scl2::bytearray& key)
{
    if (m_state != state::open) return xkeydb_error::InvalidOperation;
    if (testInst(inst_config::ReadOnly)) return xkeydb_error::IsReadOnly;

    if (m_data.erase(key) != 0) m_dirty = true;
    return xkeydb_error::None;
}

xkeydb_error database::eraseKey(std::string_view key)
{
    return eraseKey(key_from(key));
}

xkeydb_error database::clear()
{
    if (m_state != state::open) return xkeydb_error::InvalidOperation;
    if (testInst(inst_config::ReadOnly)) return xkeydb_error::IsReadOnly;

    if (!m_data.empty()) {
        m_data.clear();
        m_dirty = true;
    }
    return xkeydb_error::None;
}

std::vector<scl2::bytearray> database::keys() const
{
    std::vector<scl2::bytearray> out;
    out.reserve(m_data.size());

    for (const auto& [key, value] : m_data) out.push_back(key);

    return out;
}

std::vector<scl2::bytearray> database::keys(std::string_view prefix) const
{
    if (prefix.empty()) return keys();

    // The map is ordered, so the range can be found without a full scan.
    const scl2::bytearray start = key_from(prefix);
    std::vector<scl2::bytearray> out;

    for (auto it = m_data.lower_bound(start); it != m_data.end(); ++it) {
        if (!starts_with(it->first, start)) break;
        out.push_back(it->first);
    }

    return out;
}

#ifdef SCL2_XKEYDB_HAS_ENTRIES
std::generator<std::pair<scl2::bytearray, scl2::variant>> database::entries() const
{
    for (const auto& [key, value] : m_data)
        co_yield std::pair<scl2::bytearray, scl2::variant>{ key, value };
}
#endif

xkeydb_error database::takeBackup(const fs::path& backup_path)
{
    // Nothing here touches the open database, so it stays exactly as it was.
    if (!m_loaded) return xkeydb_error::InvalidOperation;

    try {
        xkeydb_header hdr{};
        crypto_block blk{};
        scl2::writeFile(backup_path, buildImage(hdr, blk), true);
    }
    catch (const std::exception&) {
        return xkeydb_error::FileWriteError;
    }

    return xkeydb_error::None;
}

xkeydb_error database::saveAs(const fs::path& path, inst_config inst)
{
    if (m_state != state::open) return xkeydb_error::InvalidOperation;

    std::error_code ec;
    if (fs::exists(path, ec) && !ec) return xkeydb_error::AlreadyExists;

    xkeydb_header hdr{};
    crypto_block blk{};

    try {
        scl2::writeFile(path, buildImage(hdr, blk), true);
    }
    catch (const std::exception&) {
        return xkeydb_error::FileWriteError;
    }

    // The database lives at the new path from here on. The old file is left alone, and
    // this handle no longer has anything to do with it.
    m_path = path;
    m_header = hdr;
    m_crypto = blk;
    m_config.inst = inst;
    m_dirty = false;
    m_status = file_status::ok;
    return xkeydb_error::None;
}

xkeydb_error database::moveTo(const fs::path& path)
{
    if (m_state != state::open) return xkeydb_error::InvalidOperation;

    std::error_code ec;
    if (fs::exists(path, ec) && !ec) return xkeydb_error::AlreadyExists;

    xkeydb_header hdr{};
    crypto_block blk{};
    scl2::bytearray image;

    try {
        image = buildImage(hdr, blk);
        scl2::writeFile(path, image, true);
    }
    catch (const std::exception&) {
        return xkeydb_error::FileWriteError;
    }

    const fs::path old_path = m_path;
    fs::remove(old_path, ec);
    if (ec) {
        // Take the copy back out rather than leaving the database in two places.
        std::error_code ignored;
        fs::remove(path, ignored);
        return xkeydb_error::FileWriteError;
    }

    m_path = path;
    m_header = hdr;
    m_crypto = blk;
    m_dirty = false;
    m_status = file_status::ok;
    return xkeydb_error::None;
}

xkeydb_error database::destroy()
{
    std::error_code ec;

    if (fs::exists(m_path, ec) && !ec) {
        fs::remove(m_path, ec);
        if (ec) return xkeydb_error::FileWriteError;

        // A leftover from a save that never got to the rename.
        fs::path tmp = m_path;
        tmp += ".tmp";
        std::error_code ignored;
        fs::remove(tmp, ignored);
    }

    // Back to how the object was right after construction: nothing at the path, nothing
    // loaded, no keys held.
    m_status = file_status::missing;
    m_state = state::vacant;
    m_loaded = false;
    m_dirty = false;
    m_unlocked = false;
    m_data.clear();
    m_header = xkeydb_header{};
    m_crypto = crypto_block{};
    m_config = xkeydb_config{};
    m_lastError = xkeydb_error::None;
    m_encKey = scl2::secure_bytearray{};
    m_macKey = scl2::secure_bytearray{};

    return xkeydb_error::None;
}

} // namespace scl2::xkeydb