#include "zlib.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace scl2 {
namespace {

using std::string;

// ─────────────────────────── Adler-32 ────────────────────────────────

struct adler_state {
    uint32_t a = 1, b = 0;
    void update(const uint8_t* p, size_t n) {
        while (n > 0) {
            size_t take = n > 5552 ? 5552 : n;
            n -= take;
            for (size_t k = 0; k < take; ++k) { a += p[k]; b += a; }
            a %= 65521; b %= 65521;
            p += take;
        }
    }
    uint32_t value() const { return (b << 16) | a; }
};

// ─────────────────────── DEFLATE 长度 / 距离表 ───────────────────────

const uint16_t kLenBase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,
                                43,51,59,67,83,99,115,131,163,195,227,258 };
const uint8_t kLenExtra[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,
                                4,4,4,4,5,5,5,5,0 };
const uint16_t kDistBase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
                                 257,385,513,769,1025,1537,2049,3073,4097,
                                 6145,8193,12289,16385,24577 };
const uint8_t kDistExtra[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,
                                 9,9,10,10,11,11,12,12,13,13 };

int len_index(int len) {
    int best = 0;
    for (int i = 0; i < 29; ++i) if (kLenBase[i] <= len) best = i;
    return best;
}
int dist_index(int d) {
    int best = 0;
    for (int i = 0; i < 30; ++i) if (kDistBase[i] <= d) best = i;
    return best;
}

// ─────────────────────── 位输出（压缩侧） ────────────────────────────
// DEFLATE 位流整体 LSB-first；huffman 码「先发最高位」，发射前须反转。

uint32_t rev_bits(uint32_t x, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; ++i) { r = (r << 1) | (x & 1u); x >>= 1; }
    return r;
}

struct bit_writer {
    string bytes;
    uint32_t cur = 0;
    int n = 0;

    void put(int val, int count) {
        cur |= (static_cast<uint32_t>(val) & ((1u << count) - 1)) << n;
        n += count;
        while (n >= 8) { bytes.push_back(static_cast<char>(cur & 0xFFu)); cur >>= 8; n -= 8; }
    }
    void raw_byte(uint8_t v) { bytes.push_back(static_cast<char>(v)); }
    void align_byte() {
        const int rem = n & 7;
        if (rem) { bytes.push_back(static_cast<char>(cur & 0xFFu)); cur >>= 8; n -= rem; }
    }
    size_t length() const { return bytes.size() + (n ? 1 : 0); }
};

// fixed-Huffman litlen 码发射（RFC1951 3.2.6）
void put_litlen(bit_writer& w, int sym) {
    if (sym < 144)      w.put(static_cast<int>(rev_bits(0x30u + sym, 8)), 8);
    else if (sym < 256) w.put(static_cast<int>(rev_bits(0x190u + (sym - 144), 9)), 9);
    else if (sym == 256)w.put(0, 7);
    else if (sym < 280) w.put(static_cast<int>(rev_bits(sym - 256u, 7)), 7);
    else                w.put(static_cast<int>(rev_bits(0xC0u + (sym - 280), 8)), 8);
}

// ─────────────────────── 简单 LZ77 匹配器 ────────────────────────────

constexpr size_t kWindow = 32768;
constexpr size_t kMaxLen = 258;
constexpr size_t kMaxTries = 512;
constexpr size_t kGoodLen = 64;

size_t find_match(const uint8_t* d, size_t pos, size_t end, size_t& dist) {
    const size_t max_len = std::min<size_t>(kMaxLen, end - pos);
    if (max_len < 3) return 0;
    const size_t lim = pos > kWindow ? pos - kWindow : 0;
    size_t scan = pos, best = 0, tries = 0;
    while (scan > lim && tries < kMaxTries) {
        --scan; ++tries;
        if (d[scan] != d[pos]) continue;
        if (d[scan + 1] != d[pos + 1] || d[scan + 2] != d[pos + 2]) continue;
        size_t l = 3;
        while (l < max_len && d[scan + l] == d[pos + l]) ++l;
        if (l > best) {
            best = l; dist = pos - scan;
            if (best >= kGoodLen || best >= max_len) break;
        }
    }
    return best >= 3 ? best : 0;
}

// ─────────────────── 单个 deflate 块编码 ─────────────────────────────

void emit_block(bit_writer& w, const uint8_t* d, size_t n, bool final) {
    // 尝试 fixed-Huffman 压缩
    bit_writer t;
    t.put(final ? 1 : 0, 1);
    t.put(1, 2);                                // BTYPE=01
    size_t i = 0;
    while (i < n) {
        size_t dist = 0;
        const size_t m = find_match(d, i, n, dist);
        if (m >= 3) {
            const int li = len_index(static_cast<int>(m));
            put_litlen(t, 257 + li);
            t.put(static_cast<int>(m - kLenBase[li]), kLenExtra[li]);
            const int di = dist_index(static_cast<int>(dist));
            t.put(static_cast<int>(rev_bits(static_cast<uint32_t>(di), 5)), 5);
            t.put(static_cast<int>(dist - kDistBase[di]), kDistExtra[di]);
            i += m;
        } else {
            put_litlen(t, d[i]);
            ++i;
        }
    }
    put_litlen(t, 256);                          // EOB

    if (t.length() < n + 5) {
        // 用固定码：把 t 的字节与残余位原样搬进 w（位流连续）
        for (size_t k = 0; k < t.bytes.size(); ++k)
            w.put(t.bytes[k] & 0xFFu, 8);
        w.put(static_cast<int>(t.cur & ((1u << t.n) - 1)), t.n);
    } else {
        // stored 块（须字节对齐；n 受 chunk 上限约束，恒 <=32768）
        w.align_byte();
        w.raw_byte(static_cast<uint8_t>(final ? 1 : 0));
        w.raw_byte(static_cast<uint8_t>(n & 0xFFu));
        w.raw_byte(static_cast<uint8_t>((n >> 8) & 0xFFu));
        w.raw_byte(static_cast<uint8_t>(~n & 0xFFu));
        w.raw_byte(static_cast<uint8_t>((~n >> 8) & 0xFFu));
        for (size_t k = 0; k < n; ++k) w.raw_byte(d[k]);
    }
}

// 自包含多块 deflate 编码器（静态与流式共用）
struct deflater {
    bit_writer w;
    string pending;
    bool any = false;

    // final=true 时把残留输入编码成最后一块（含空输入也产出一块）
    string push(const uint8_t* p, size_t n, bool final) {
        if (n) pending.append(reinterpret_cast<const char*>(p), n);
        const size_t CH = 32768;
        if (final) {
            size_t base = 0;
            while (pending.size() - base > CH) {
                emit_block(w, reinterpret_cast<const uint8_t*>(pending.data()) + base, CH, false);
                base += CH; any = true;
            }
            emit_block(w, reinterpret_cast<const uint8_t*>(pending.data()) + base,
                       pending.size() - base, true);
            any = true;
            pending.clear();
            // flush 残余位（<8 bit）：BFINAL 后补零到字节边界合法，
            // inflate 读到块结束符号即停止，会忽略这些填充位。
            if (w.n) {
                w.bytes.push_back(static_cast<char>(w.cur & 0xFFu));
                w.cur = 0; w.n = 0;
            }
        } else {
            while (pending.size() >= 2 * CH) {
                emit_block(w, reinterpret_cast<const uint8_t*>(pending.data()), CH, false);
                pending.erase(0, CH); any = true;
            }
        }
        string out;
        out.swap(w.bytes);
        return out;
    }
};

// ─────────────────────── 位输入（解压侧） ────────────────────────────

struct zlib_incomplete { };        // 输入耗尽、还需更多字节（内部信号）

struct bit_reader {
    const uint8_t* p;
    size_t len;
    size_t i = 0;
    uint32_t cur = 0;
    int n = 0;

    int get(int count) {
        while (n < count) {
            if (i >= len) throw zlib_incomplete{};
            cur |= static_cast<uint32_t>(p[i++]) << n;
            n += 8;
        }
        const int v = static_cast<int>(cur & ((1u << count) - 1));
        cur >>= count;
        n -= count;
        return v;
    }
    void align_byte() {
        const int rem = n & 7;
        if (rem) { cur >>= rem; n -= rem; }
    }
    void read_bytes(string& out, size_t count) {
        if (i + count > len) throw zlib_incomplete{};
        out.append(reinterpret_cast<const char*>(p + i), count);
        i += count;
    }
};

// 经典 canonical huffman 解码表（RFC1951 puff 算法）
struct huff_table {
    int counts[16] = { 0 };
    std::vector<int> syms;

    int decode(bit_reader& r) const {
        int code = 0, first = 0, index = 0;
        for (int l = 1; l <= 15; ++l) {
            code |= r.get(1);
            const int c = counts[l];
            if (code - first < c)
                return syms[static_cast<size_t>(index + (code - first))];
            index += c;
            first = (first + c) << 1;
            code <<= 1;
        }
        throw std::runtime_error("zlib: invalid huffman code");
    }
};

huff_table build_table(const std::vector<int>& lens) {
    huff_table h;
    for (int l = 1; l <= 15; ++l)
        for (size_t s = 0; s < lens.size(); ++s)
            if (lens[s] == l) { h.counts[l]++; h.syms.push_back(static_cast<int>(s)); }
    return h;
}

const huff_table& fixed_lit() {
    static huff_table t = [] {
        std::vector<int> lens(288, 0);
        for (int s = 0; s <= 143; ++s) lens[s] = 8;
        for (int s = 144; s <= 255; ++s) lens[s] = 9;
        for (int s = 256; s <= 279; ++s) lens[s] = 7;
        for (int s = 280; s <= 287; ++s) lens[s] = 8;
        return build_table(lens);
    }();
    return t;
}
const huff_table& fixed_dist() {
    static huff_table t = [] {
        std::vector<int> lens(32, 0);
        for (int s = 0; s < 32; ++s) lens[s] = 5;
        return build_table(lens);
    }();
    return t;
}

// ─────────────────── 原始 DEFLATE 解码 ───────────────────────────────

string inflate_raw(bit_reader& in) {
    string out;
    const huff_table& flit = fixed_lit();
    const huff_table& fdist = fixed_dist();
    for (;;) {
        const int bfinal = in.get(1);
        const int btype = in.get(2);
        if (btype == 0) {
            in.align_byte();
            const int len = in.get(16);
            const int nlen = in.get(16);
            if (static_cast<uint16_t>(len) != static_cast<uint16_t>(~nlen))
                throw std::runtime_error("zlib: stored block length mismatch");
            in.read_bytes(out, static_cast<size_t>(len));
        } else {
            const huff_table* lit = &flit;
            const huff_table* dist = &fdist;
            huff_table dyn_lit, dyn_dist;
            if (btype == 2) {
                // 动态哈夫曼块头（RFC1951 3.2.7）
                const int hlit = in.get(5) + 257;
                const int hdist = in.get(5) + 1;
                const int hclen = in.get(4) + 4;
                static const int order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,
                                               12,3,13,2,14,1,15 };
                std::vector<int> clens(19, 0);
                for (int i = 0; i < hclen; ++i) clens[order[i]] = in.get(3);
                const huff_table clen_table = build_table(clens);

                std::vector<int> lens(static_cast<size_t>(hlit + hdist), 0);
                size_t idx = 0;
                const size_t total = lens.size();
                while (idx < total) {
                    const int s = clen_table.decode(in);
                    if (s < 16) { lens[idx++] = s; }
                    else if (s == 16) {
                        if (idx == 0) throw std::runtime_error("zlib: bad repeat");
                        const int prev = lens[idx - 1];
                        int rep = 3 + in.get(2);
                        while (rep-- && idx < total) lens[idx++] = prev;
                    } else if (s == 17) {
                        int rep = 3 + in.get(3);
                        while (rep-- && idx < total) lens[idx++] = 0;
                    } else {
                        int rep = 11 + in.get(7);
                        while (rep-- && idx < total) lens[idx++] = 0;
                    }
                }
                std::vector<int> lit_lens(lens.begin(), lens.begin() + hlit);
                std::vector<int> dist_lens(lens.begin() + hlit, lens.end());
                dyn_lit = build_table(lit_lens);
                dyn_dist = build_table(dist_lens);
                lit = &dyn_lit;
                dist = &dyn_dist;
            } else if (btype != 1) {
                throw std::runtime_error("zlib: invalid block type");
            }
            // 符号解码
            for (;;) {
                const int sym = lit->decode(in);
                if (sym == 256) break;
                if (sym < 256) { out.push_back(static_cast<char>(sym)); continue; }
                const int li = sym - 257;
                if (li < 0 || li >= 29) throw std::runtime_error("zlib: bad length code");
                const int length = kLenBase[li] + in.get(kLenExtra[li]);
                const int dsym = dist->decode(in);
                if (dsym < 0 || dsym >= 30) throw std::runtime_error("zlib: bad distance code");
                const int distance = kDistBase[dsym] + in.get(kDistExtra[dsym]);
                if (distance > static_cast<int>(out.size()))
                    throw std::runtime_error("zlib: distance too far");
                const size_t start = out.size() - static_cast<size_t>(distance);
                for (int k = 0; k < length; ++k)     // 重叠拷贝逐字节即正确
                    out.push_back(out[start + static_cast<size_t>(k)]);
            }
        }
        if (bfinal) break;
    }
    return out;
}

// ─────────────────── zlib 容器（RFC1950） ────────────────────────────

string decode_zlib(const uint8_t* p, size_t n) {
    if (n < 6) throw std::runtime_error("zlib: stream too short");
    const uint8_t cmf = p[0], flg = p[1];
    if ((cmf & 0x0F) != 8) throw std::runtime_error("zlib: not deflate (CMF)");
    if ((((static_cast<uint16_t>(cmf) << 8) | flg) % 31) != 0)
        throw std::runtime_error("zlib: bad header checksum");
    size_t start = 2;
    if (flg & 0x20) {                              // preset dictionary
        if (n < start + 4) throw std::runtime_error("zlib: stream too short");
        start += 4;
    }
    if (n < start + 4) throw std::runtime_error("zlib: stream too short");
    const size_t data_end = n - 4;
    bit_reader in{ p + start, data_end - start };
    string out = inflate_raw(in);
    const uint32_t want = (static_cast<uint32_t>(p[data_end]) << 24)
                        | (static_cast<uint32_t>(p[data_end + 1]) << 16)
                        | (static_cast<uint32_t>(p[data_end + 2]) << 8)
                        | static_cast<uint32_t>(p[data_end + 3]);
    adler_state ad;
    ad.update(reinterpret_cast<const uint8_t*>(out.data()), out.size());
    if (ad.value() != want) throw std::runtime_error("zlib: adler32 mismatch");
    return out;
}

// 尝试完整解码；不足时 complete=false（供流式缓冲累积）
string try_decode_zlib(const string& buf, bool& complete) {
    try {
        string r = decode_zlib(reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
        complete = true;
        return r;
    } catch (const zlib_incomplete&) {
        complete = false;
        return {};
    }
}

} // namespace

// ═══════════════════ zlib provider 实现 ═══════════════════

struct zlib::stream_type::Impl {
    codec_dir dir;
    deflater def;                  // 压缩方向
    adler_state ad;
    string head = "\x78\x9c";      // zlib 头（首个输出字节前发出）
    bool header_done = false;
    bool ended = false;
    string inbuf;                  // 解压方向：累积输入
    bool dec_done = false;         // 解压方向：已完成整段解码
};

zlib::stream_type::stream_type(codec_dir dir) : m_(new Impl) { m_->dir = dir; }
zlib::stream_type::~stream_type() { delete m_; }

scl2::bytearray zlib::stream_type::update(const scl2::bytearray& data)
{
    if (m_->ended) throw std::runtime_error("zlib: stream already ended");
    if (m_->dir == codec_dir::Compress) {
        m_->ad.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
        string o = m_->def.push(reinterpret_cast<const uint8_t*>(data.data()),
                                data.size(), false);
        if (!o.empty() && !m_->header_done) {
            o.insert(0, m_->head);
            m_->header_done = true;
        }
        return scl2::bytearray(o);
    } else {
        m_->inbuf.append(reinterpret_cast<const char*>(data.data()), data.size());
        if (m_->dec_done) throw std::runtime_error("zlib: stream already finished");
        bool complete = false;
        string r = try_decode_zlib(m_->inbuf, complete);
        if (complete) { m_->inbuf.clear(); m_->dec_done = true; return scl2::bytearray(r); }
        return {};
    }
}

scl2::bytearray zlib::stream_type::end()
{
    if (m_->ended) return {};
    m_->ended = true;
    if (m_->dir == codec_dir::Compress) {
        string o = m_->def.push(nullptr, 0, true);
        if (!m_->header_done) {
            o.insert(0, m_->head);
            m_->header_done = true;
        }
        const uint32_t v = m_->ad.value();
        o.push_back(static_cast<char>(v >> 24));
        o.push_back(static_cast<char>((v >> 16) & 0xFFu));
        o.push_back(static_cast<char>((v >> 8) & 0xFFu));
        o.push_back(static_cast<char>(v & 0xFFu));
        return scl2::bytearray(o);
    } else {
        if (m_->dec_done) return {};
        bool complete = false;
        string r = try_decode_zlib(m_->inbuf, complete);
        if (!complete) throw std::runtime_error("zlib: truncated stream");
        m_->inbuf.clear();
        m_->dec_done = true;
        return scl2::bytearray(r);
    }
}

scl2::bytearray zlib::compress(const scl2::bytearray& data)
{
    deflater def;
    string o = def.push(reinterpret_cast<const uint8_t*>(data.data()), data.size(), true);
    string out = "\x78\x9c";
    out += o;
    adler_state ad;
    ad.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    const uint32_t v = ad.value();
    out.push_back(static_cast<char>(v >> 24));
    out.push_back(static_cast<char>((v >> 16) & 0xFFu));
    out.push_back(static_cast<char>((v >> 8) & 0xFFu));
    out.push_back(static_cast<char>(v & 0xFFu));
    return scl2::bytearray(out);
}

scl2::bytearray zlib::decompress(const scl2::bytearray& data)
{
    string r;
    try {
        r = decode_zlib(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    } catch (const zlib_incomplete&) {
        throw std::runtime_error("zlib: truncated stream");
    }
    return scl2::bytearray(r);
}

} // namespace scl2
