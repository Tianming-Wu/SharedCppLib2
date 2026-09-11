/*
    XML2 — implementation (see xml2.hpp)

    [SCL_STANDALONE_MODULE]
    version: 0.1.0
    cpp_generation: cxx17 - cxx23
*/
#include "xml2.hpp"

#include <cctype>
#include <unordered_map>
#include <utility>

namespace scl2::xml2 {
namespace {

// ── 字符分类 ────────────────────────────────────────────────────────────

bool is_name_start(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == ':';
}

bool is_name_char(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':'
        || c == '-' || c == '.';
}

bool is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_all_ws(std::string_view s)
{
    for (const char c : s)
        if (!is_ws(c)) return false;
    return true;
}

// ── 实体解码 / 转义 ────────────────────────────────────────────────────

void append_utf8(std::string& out, unsigned long cp)
{
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

std::string decode_entities(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') { out += s[i]; continue; }

        const std::size_t semi = s.find(';', i + 1);
        if (semi == std::string_view::npos) { out += '&'; continue; }

        const std::string_view ent = s.substr(i + 1, semi - i - 1);
        bool handled = true;
        if (ent == "amp")       out += '&';
        else if (ent == "lt")   out += '<';
        else if (ent == "gt")   out += '>';
        else if (ent == "quot") out += '"';
        else if (ent == "apos") out += '\'';
        else if (!ent.empty() && ent[0] == '#') {
            unsigned long code = 0;
            bool ok = true;
            if (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) {
                for (std::size_t k = 2; k < ent.size(); ++k) {
                    const char c = ent[k];
                    if (!std::isxdigit(static_cast<unsigned char>(c))) { ok = false; break; }
                    code = code * 16 + static_cast<unsigned long>(
                        std::isdigit(static_cast<unsigned char>(c))
                            ? c - '0'
                            : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
                }
            } else {
                for (std::size_t k = 1; k < ent.size(); ++k) {
                    const char c = ent[k];
                    if (!std::isdigit(static_cast<unsigned char>(c))) { ok = false; break; }
                    code = code * 10 + static_cast<unsigned long>(c - '0');
                }
            }
            if (!ok) handled = false;
            else append_utf8(out, code);
        }
        else handled = false;

        if (!handled) { out += '&'; continue; }
        i = semi;
    }
    return out;
}

void escape_into(std::string& out, std::string_view s, bool for_attr)
{
    for (const char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += for_attr ? "&quot;" : "\""; break;
            case '\'': out += for_attr ? "&apos;" : "'";  break;
            default:   out += c;        break;
        }
    }
}

std::string escape_text(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    escape_into(out, s, false);
    return out;
}

std::string escape_attr(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    escape_into(out, s, true);
    return out;
}

// ── 解析结果 ────────────────────────────────────────────────────────────

struct parse_result {
    std::string prolog;
    std::string epilog;
    bool has_root = false;
    node root;
};

// ── 递归下降解析器（保真：所有原文片段都保存）────────────────────────────

class parser {
public:
    parser(std::string_view text, fidelity f) : m_s(text), m_f(f) {}

    parse_result run()
    {
        parse_result r;

        // prolog：XML 声明 / DOCTYPE / 注释 / PI / 空白
        // raw 模式逐字节保留；semantic 模式只保留 XML 声明本身
        while (!eof()) {
            if (peek() != '<') {
                if (m_f == fidelity::raw) r.prolog += m_s[m_pos];
                ++m_pos;
                continue;
            }
            if (starts_with("<?") || starts_with("<!--") || starts_with("<!")) {
                const std::size_t start = m_pos;
                const bool keep = (m_f == fidelity::raw) || starts_with("<?xml");
                skip_misc();
                if (keep) r.prolog.append(m_s.substr(start, m_pos - start));
                continue;
            }
            break;
        }

        if (!eof() && peek() == '<') {
            r.root = parse_element();
            r.has_root = true;
        }

        // epilog：根元素之后的内容（semantic 模式下丢弃）
        if (m_f == fidelity::raw) r.epilog.assign(m_s.substr(m_pos));
        m_pos = m_s.size();
        return r;
    }

private:
    std::string_view m_s;
    std::size_t m_pos = 0;
    fidelity m_f = fidelity::raw;

    bool eof() const { return m_pos >= m_s.size(); }
    char peek() const { return m_pos < m_s.size() ? m_s[m_pos] : '\0'; }

    bool starts_with(std::string_view sv) const
    {
        return m_s.substr(m_pos, sv.size()) == sv;
    }

    void skip_misc()
    {
        if (starts_with("<!--")) {
            const std::size_t p = m_s.find("-->", m_pos);
            m_pos = (p == std::string_view::npos) ? m_s.size() : p + 3;
            return;
        }
        if (starts_with("<?") && !starts_with("<?xml")) {
            const std::size_t p = m_s.find("?>", m_pos);
            m_pos = (p == std::string_view::npos) ? m_s.size() : p + 2;
            return;
        }
        if (starts_with("<?")) {
            const std::size_t p = m_s.find("?>", m_pos);
            m_pos = (p == std::string_view::npos) ? m_s.size() : p + 2;
            return;
        }
        // DOCTYPE（可含 [...] 内部子集）
        ++m_pos;   // '<'
        int depth = 0;
        while (!eof()) {
            const char c = m_s[m_pos];
            if (c == '[') ++depth;
            else if (c == ']') --depth;
            else if (c == '>' && depth <= 0) { ++m_pos; return; }
            ++m_pos;
        }
    }

    std::string read_ws()
    {
        const std::size_t s = m_pos;
        while (!eof() && is_ws(m_s[m_pos])) ++m_pos;
        return std::string(m_s.substr(s, m_pos - s));
    }

    std::string read_name()
    {
        if (eof() || !is_name_start(m_s[m_pos]))
            throw parsing_error("expected a name");
        const std::size_t s = m_pos;
        ++m_pos;
        while (!eof() && is_name_char(m_s[m_pos])) ++m_pos;
        return std::string(m_s.substr(s, m_pos - s));
    }

    std::string read_sep()
    {
        const std::size_t s = m_pos;
        while (!eof() && (is_ws(m_s[m_pos]) || m_s[m_pos] == '=')) ++m_pos;
        std::string sep(m_s.substr(s, m_pos - s));
        if (sep.find('=') == std::string::npos)
            throw parsing_error("expected '=' in attribute");
        return sep;
    }

    node parse_element()
    {
        node n;
        element e;
        const std::size_t start = m_pos;   // '<'
        ++m_pos;

        e.name = read_name();

        for (;;) {
            const std::string ws = read_ws();
            if (eof())
                throw parsing_error("unexpected end inside start tag");

            const char c = m_s[m_pos];
            if (c == '>') {
                e.name_tail = ws;
                ++m_pos;
                break;
            }
            if (c == '/') {
                if (m_pos + 1 < m_s.size() && m_s[m_pos + 1] == '>') {
                    e.name_tail = ws;
                    e.self_closed = true;
                    m_pos += 2;
                    break;
                }
                throw parsing_error("unexpected '/' inside start tag");
            }
            if (!is_name_start(c))
                throw parsing_error("invalid attribute name");

            attribute a;
            a.lead = ws;
            a.name = read_name();
            a.sep = read_sep();

            if (eof())
                throw parsing_error("unterminated attribute value");
            a.quote = m_s[m_pos];
            if (a.quote != '"' && a.quote != '\'')
                throw parsing_error("attribute value must be quoted");
            ++m_pos;

            const std::size_t vs = m_pos;
            const std::size_t ve = m_s.find(a.quote, m_pos);
            if (ve == std::string_view::npos)
                throw parsing_error("unterminated attribute value");
            a.raw_value.assign(m_s.substr(vs, ve - vs));
            a.value = decode_entities(a.raw_value);
            m_pos = ve + 1;

            if (m_f != fidelity::raw) {
                // semantic：不保留属性原文，序列化时统一用 name="value" 生成
                a.lead = " ";
                a.sep = "=";
                a.quote = '"';
                a.raw_value.clear();
                a.value_dirty = true;
            }

            e.attrs.push_back(std::move(a));
        }

        if (m_f == fidelity::raw) {
            e.raw_open.assign(m_s.substr(start, m_pos - start));
        } else {
            e.dirty = true;          // 没有原文可供照抄，序列化时重新生成
            e.name_tail.clear();
        }

        if (!e.self_closed) {
            for (;;) {
                if (eof())
                    throw parsing_error("missing </" + e.name + ">");

                if (m_s[m_pos] != '<') {
                    // 文本（含纯空白）
                    const std::size_t ts = m_pos;
                    while (!eof() && m_s[m_pos] != '<') ++m_pos;
                    const std::string_view raw_text = m_s.substr(ts, m_pos - ts);

                    node t;
                    text_data td;
                    if (m_f == fidelity::raw) {
                        td.raw.assign(raw_text);
                        td.value = decode_entities(raw_text);
                    } else {
                        // semantic：纯空白属于布局，丢弃；其余只保留解码后的值
                        if (is_all_ws(raw_text)) continue;
                        td.dirty = true;
                        td.value = decode_entities(raw_text);
                        td.new_value = td.value;
                    }
                    t.content = std::move(td);
                    e.children.push_back(std::move(t));
                    continue;
                }

                if (starts_with("<!--")) {
                    const std::size_t cs = m_pos;
                    const std::size_t p = m_s.find("-->", m_pos);
                    if (p == std::string_view::npos)
                        throw parsing_error("unterminated comment");
                    m_pos = p + 3;
                    node c;
                    comment_data cd;
                    cd.raw.assign(m_s.substr(cs, m_pos - cs));
                    c.content = std::move(cd);
                    e.children.push_back(std::move(c));
                    continue;
                }

                if (starts_with("<![CDATA[")) {
                    const std::size_t cs = m_pos;
                    const std::size_t p = m_s.find("]]>", m_pos);
                    if (p == std::string_view::npos)
                        throw parsing_error("unterminated CDATA section");
                    m_pos = p + 3;
                    const std::string_view content = m_s.substr(cs + 9, p - (cs + 9));

                    node t;
                    text_data td;
                    if (m_f == fidelity::raw) {
                        td.cdata = true;
                        td.raw.assign(m_s.substr(cs, m_pos - cs));
                        td.value.assign(content);
                    } else {
                        // semantic：不保留 CDATA 包装，值等价
                        td.dirty = true;
                        td.value.assign(content);
                        td.new_value = td.value;
                    }
                    t.content = std::move(td);
                    e.children.push_back(std::move(t));
                    continue;
                }

                if (starts_with("<?")) {
                    const std::size_t cs = m_pos;
                    const std::size_t p = m_s.find("?>", m_pos);
                    if (p == std::string_view::npos)
                        throw parsing_error("unterminated processing instruction");
                    m_pos = p + 2;
                    node x;
                    pi_data pd;
                    pd.raw.assign(m_s.substr(cs, m_pos - cs));
                    x.content = std::move(pd);
                    e.children.push_back(std::move(x));
                    continue;
                }

                if (starts_with("</")) {
                    const std::size_t cs = m_pos;
                    const std::size_t p = m_s.find('>', m_pos);
                    if (p == std::string_view::npos)
                        throw parsing_error("unterminated end tag");
                    m_pos = p + 1;
                    e.raw_close.assign(m_s.substr(cs, m_pos - cs));
                    break;
                }

                e.children.push_back(parse_element());
            }
        }

        n.content = std::move(e);
        return n;
    }
};

// ── 序列化 ──────────────────────────────────────────────────────────────

std::string build_open_tag(const element& e)
{
    std::string s;
    s.reserve(e.raw_open.size() + 16);
    s += '<';
    s += e.name;
    for (const attribute& a : e.attrs) {
        s += a.lead;
        s += a.name;
        s += a.sep;
        s += a.quote;
        if (a.value_dirty) s += escape_attr(a.value);
        else               s += a.raw_value;
        s += a.quote;
    }
    s += e.name_tail;
    s += e.self_closed ? "/>" : ">";
    return s;
}

void emit(const node& n, std::string& out)
{
    if (const element* e = std::get_if<element>(&n.content)) {
        out += e->dirty ? build_open_tag(*e) : e->raw_open;
        for (const node& c : e->children) emit(c, out);
        if (!e->self_closed) {
            if (!e->raw_close.empty()) out += e->raw_close;
            else out += "</" + e->name + ">";
        }
        return;
    }
    if (const text_data* t = std::get_if<text_data>(&n.content)) {
        out += t->dirty ? escape_text(t->new_value) : t->raw;
        return;
    }
    if (const comment_data* c = std::get_if<comment_data>(&n.content)) {
        out += c->raw;
        return;
    }
    if (const pi_data* p = std::get_if<pi_data>(&n.content)) {
        out += p->raw;
        return;
    }
}

} // namespace

// ── 公共接口 ────────────────────────────────────────────────────────────

document document::parse(std::string text, fidelity f)
{
    parser p(text, f);
    parse_result r = p.run();

    document doc;
    doc.mode_ = f;
    doc.prolog_ = std::move(r.prolog);
    doc.epilog_ = std::move(r.epilog);
    doc.has_root_ = r.has_root;
    doc.root_ = std::move(r.root);
    return doc;
}

std::string document::serialize() const
{
    std::string out;
    out += prolog_;
    if (has_root_) emit(root_, out);
    out += epilog_;
    return out;
}

void node::set_attr(std::string_view key, std::string new_value)
{
    element* e = as_element();
    if (!e) return;

    for (attribute& a : e->attrs) {
        if (a.name == key) {
            a.value = std::move(new_value);
            a.value_dirty = true;
            e->dirty = true;
            return;
        }
    }

    // 追加新属性：沿用原有缝隙空白，尽量贴合原风格
    attribute a;
    a.name.assign(key);
    a.sep = "=";
    a.quote = '"';
    a.value = std::move(new_value);
    a.raw_value = escape_attr(a.value);
    if (e->attrs.empty()) {
        a.lead = e->name_tail.empty() ? std::string(" ") : e->name_tail;
        e->name_tail.clear();
    } else {
        a.lead = " ";
    }
    e->attrs.push_back(std::move(a));
    e->dirty = true;
}

void node::set_text(std::string new_value)
{
    if (text_data* t = as_text()) {
        t->new_value = std::move(new_value);
        t->dirty = true;
    }
}

namespace {

// 某元素在 children 中的前导纯空白（没有则返回空）
std::string element_lead(const element& e, std::size_t elem_pos)
{
    if (elem_pos == 0) return {};
    const text_data* t = e.children[elem_pos - 1].as_text();
    if (!t || t->cdata || !is_all_ws(t->raw)) return {};
    return t->raw;
}

// 构造一个原文与值相同的文本节点（用于插入空白）
node make_text_node(std::string text)
{
    node n;
    text_data t;
    t.value = text;
    t.raw = std::move(text);
    n.content = std::move(t);
    return n;
}

const std::size_t npos_index = static_cast<std::size_t>(-1);

// 命中节点在父元素 children 中是第几个元素（0-based）
std::size_t element_index_of(const node& parent, const node* child)
{
    const element* pe = parent.as_element();
    if (!pe) return npos_index;
    std::size_t idx = 0;
    for (const node& c : pe->children) {
        if (&c == child) return c.is_element() ? idx : npos_index;
        if (c.is_element()) ++idx;
    }
    return npos_index;
}

} // namespace

void node::append_child(node child)
{
    element* e = as_element();
    if (!e) return;
    std::size_t count = 0;
    for (const node& c : e->children)
        if (c.is_element()) ++count;
    insert_child(count, std::move(child));      // 追加 = 在末尾插入
}

bool node::insert_child(std::size_t element_index, node child)
{
    element* e = as_element();
    if (!e) return false;

    // 定位插入点：第 element_index 个元素在 children 中的下标（超出则末尾）
    std::size_t pos = e->children.size();
    {
        std::size_t seen = 0;
        for (std::size_t i = 0; i < e->children.size(); ++i) {
            if (!e->children[i].is_element()) continue;
            if (seen == element_index) { pos = i; break; }
            ++seen;
        }
    }

    // 参考缩进：从相邻元素的前导空白推断（多行结构才有）
    auto reference_lead = [&]() -> std::string {
        for (std::size_t i = pos; i < e->children.size(); ++i)
            if (e->children[i].is_element()) {
                std::string ws = element_lead(*e, i);
                if (!ws.empty()) return ws;
            }
        for (std::size_t i = pos; i-- > 0; )
            if (e->children[i].is_element()) {
                std::string ws = element_lead(*e, i);
                if (!ws.empty()) return ws;
            }
        return {};
    };

    std::vector<node> add;

    if (pos == e->children.size()) {
        // 追加：父的尾随空白移到新节点之后，避免多出一个空行
        const std::string lead = reference_lead();
        std::string tail;
        if (pos > 0) {
            const text_data* t = e->children[pos - 1].as_text();
            if (t && !t->cdata && is_all_ws(t->raw)) {
                tail = t->raw;
                e->children.pop_back();
                --pos;              // 关键：摘掉尾随空白后必须同步插入点
            }
        }
        if (!lead.empty()) add.push_back(make_text_node(lead));
        add.push_back(std::move(child));
        if (!tail.empty()) add.push_back(make_text_node(tail));
    }
    else if (pos > 0) {
        const text_data* t = e->children[pos - 1].as_text();
        if (t && !t->cdata && is_all_ws(t->raw)) {
            // 中间插入：插入点前的空白自成为新节点的前导，给后面的元素复制一份
            add.push_back(std::move(child));
            add.push_back(make_text_node(t->raw));
        }
    }

    if (add.empty()) {
        // 插入点前没有空白（紧凑结构或开头）：补一份参考缩进，并保证后面元素仍有前导
        const std::string lead = reference_lead();
        if (!lead.empty()) add.push_back(make_text_node(lead));
        add.push_back(std::move(child));
        if (!lead.empty() && pos < e->children.size()) add.push_back(make_text_node(lead));
    }

    // 在 pos 处插入 add（node 不可拷贝，只能逐项搬移）
    std::vector<node> merged;
    merged.reserve(e->children.size() + add.size());
    for (std::size_t i = 0; i < pos; ++i)
        merged.push_back(std::move(e->children[i]));
    for (node& n : add)
        merged.push_back(std::move(n));
    for (std::size_t i = pos; i < e->children.size(); ++i)
        merged.push_back(std::move(e->children[i]));
    e->children = std::move(merged);
    return true;
}

bool node::remove_child(std::size_t element_index)
{
    element* e = as_element();
    if (!e) return false;

    std::size_t pos = e->children.size();
    {
        std::size_t seen = 0;
        for (std::size_t i = 0; i < e->children.size(); ++i) {
            if (!e->children[i].is_element()) continue;
            if (seen == element_index) { pos = i; break; }
            ++seen;
        }
    }
    if (pos >= e->children.size()) return false;

    // 连同前导空白一起删除
    std::size_t from = pos;
    if (pos > 0) {
        const text_data* t = e->children[pos - 1].as_text();
        if (t && !t->cdata && is_all_ws(t->raw)) from = pos - 1;
    }
    e->children.erase(e->children.begin() + from, e->children.begin() + pos + 1);

    // 已经没有任何元素子节点：遗留的纯空白一并清掉
    bool any_element = false;
    for (const node& c : e->children)
        if (c.is_element()) { any_element = true; break; }
    if (!any_element) {
        std::vector<node> kept;
        kept.reserve(e->children.size());
        for (node& c : e->children) {
            const text_data* t = c.as_text();
            if (t && !t->cdata && is_all_ws(t->raw)) continue;
            kept.push_back(std::move(c));
        }
        e->children = std::move(kept);
    }
    return true;
}

bool node::remove_attr(std::string_view key)
{
    element* e = as_element();
    if (!e) return false;
    for (std::size_t i = 0; i < e->attrs.size(); ++i) {
        if (e->attrs[i].name != key) continue;
        e->attrs.erase(e->attrs.begin() + i);   // 前导空白随属性一起消失
        if (e->attrs.empty()) e->name_tail.clear();
        e->dirty = true;
        return true;
    }
    return false;
}

// ── 路径查询 ─────────────────────────────────────────────────────────────
namespace {

struct path_step {
    bool        descendant = false;   // 前面是 "//"：任意深度
    bool        wildcard = false;     // "*"
    std::string name;
    bool        has_filter = false;
    std::string filter_attr;
    int         filter_op = 0;        // 0 = 存在, 1 = 等于, 2 = 包含
    std::string filter_value;
    bool        has_index = false;
    std::size_t index = 0;            // 0-based
};

struct parsed_path {
    std::vector<path_step> steps;
    bool        has_attr_tail = false;
    std::string attr_tail;
};

std::string_view trim_view(std::string_view s)
{
    while (!s.empty() && is_ws(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_ws(s.back()))  s.remove_suffix(1);
    return s;
}

std::string read_path_name(std::string_view s, std::size_t& i)
{
    const std::size_t start = i;
    while (i < s.size()) {
        const char c = s[i];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':'
            || c == '-' || c == '.')
            ++i;
        else
            break;
    }
    if (i == start) throw parsing_error("expected a name in path");
    return std::string(s.substr(start, i - start));
}

void parse_predicate(path_step& st, std::string_view body)
{
    body = trim_view(body);
    if (body.empty()) throw parsing_error("empty predicate in path");

    if (std::isdigit(static_cast<unsigned char>(body[0]))) {
        std::size_t n = 0;
        for (const char c : body) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                throw parsing_error("bad index predicate in path");
            n = n * 10 + static_cast<std::size_t>(c - '0');
        }
        if (n == 0) throw parsing_error("index predicate is 1-based");
        st.has_index = true;
        st.index = n - 1;
        return;
    }

    if (body[0] != '@') throw parsing_error("unsupported predicate in path");

    std::size_t i = 1;
    st.has_filter = true;
    st.filter_attr = read_path_name(body, i);
    if (i >= body.size()) return;                       // [@attr]：存在即可

    if (body.substr(i, 2) == "*=") { st.filter_op = 2; i += 2; }
    else if (body[i] == '=')         { st.filter_op = 1; i += 1; }
    else throw parsing_error("unsupported predicate operator in path");

    std::string_view v = trim_view(body.substr(i));
    if (v.size() >= 2
        && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\'')))
        v = v.substr(1, v.size() - 2);
    st.filter_value.assign(v);
}

parsed_path parse_path(std::string_view p)
{
    parsed_path out;
    std::size_t i = 0;
    bool pending_desc = false;

    if (p.size() >= 2 && p[0] == '/' && p[1] == '/') { pending_desc = true; i = 2; }
    else if (!p.empty() && p[0] == '/') { i = 1; }

    while (i < p.size()) {
        if (p[i] == '@') {                              // 结尾取属性
            ++i;
            out.has_attr_tail = true;
            out.attr_tail = read_path_name(p, i);
            break;
        }

        path_step st;
        st.descendant = pending_desc;
        pending_desc = false;

        if (p[i] == '*') { st.wildcard = true; ++i; }
        else             { st.name = read_path_name(p, i); }

        while (i < p.size() && p[i] == '[') {
            ++i;
            const std::size_t s = i;
            int depth = 1;
            while (i < p.size()) {
                if (p[i] == '[') ++depth;
                else if (p[i] == ']') { --depth; if (depth == 0) break; }
                ++i;
            }
            if (i >= p.size()) throw parsing_error("unterminated predicate in path");
            parse_predicate(st, p.substr(s, i - s));
            ++i;                                        // ']'
        }

        out.steps.push_back(std::move(st));

        if (i >= p.size()) break;
        if (p[i] == '/') {
            ++i;
            if (i < p.size() && p[i] == '/') { ++i; pending_desc = true; }
            continue;
        }
        throw parsing_error("unexpected character in path");
    }
    return out;
}

using hit_pair = std::pair<node*, node*>;   // (命中节点, 其父节点)

void collect_children(node& parent, const path_step& st, std::vector<hit_pair>& out)
{
    std::vector<node>* ch = parent.children();
    if (!ch) return;
    for (node& c : *ch) {
        const element* e = c.as_element();
        if (!e) continue;
        if (st.wildcard || e->name == st.name) out.emplace_back(&c, &parent);
    }
}

void collect_descendants(node& parent, const path_step& st, std::vector<hit_pair>& out)
{
    std::vector<node>* ch = parent.children();
    if (!ch) return;
    for (node& c : *ch) {
        const element* e = c.as_element();
        if (!e) continue;
        if (st.wildcard || e->name == st.name) out.emplace_back(&c, &parent);
        collect_descendants(c, st, out);
    }
}

bool passes_filter(const node& n, const path_step& st)
{
    if (!st.has_filter) return true;
    const element* e = n.as_element();
    if (!e) return false;
    for (const attribute& a : e->attrs) {
        if (a.name != st.filter_attr) continue;
        if (st.filter_op == 0) return true;                       // [@attr]
        if (st.filter_op == 1) return a.value == st.filter_value; // [@attr=v]
        return a.value.find(st.filter_value) != std::string::npos; // [@attr*=v]
    }
    return false;
}

// 对一步的命中集合做过滤 + 下标筛选
std::vector<hit_pair> apply_step(const std::vector<hit_pair>& hits, const path_step& st)
{
    std::vector<hit_pair> out;
    if (st.has_index) {
        // [n] 按父节点分组计数（XPath 语义）
        std::unordered_map<const node*, std::size_t> counter;
        for (const hit_pair& h : hits) {
            if (!passes_filter(*h.first, st)) continue;
            const std::size_t k = counter[h.second]++;
            if (k == st.index) out.push_back(h);
        }
    } else {
        for (const hit_pair& h : hits)
            if (passes_filter(*h.first, st)) out.push_back(h);
    }
    return out;
}

selection select_from(node& root, std::string_view path)
{
    const parsed_path pp = parse_path(path);
    selection sel;

    std::vector<hit_pair> current;
    std::size_t next_step = 0;

    // 第一步：既允许以根元素名开头（library/book），也允许直接写子元素名（book）。
    if (!pp.steps.empty()) {
        const path_step& st0 = pp.steps[0];
        std::vector<hit_pair> hits;
        if (st0.descendant) {
            collect_descendants(root, st0, hits);
        } else {
            const element* re = root.as_element();
            if (re && (st0.wildcard || re->name == st0.name))
                hits.emplace_back(&root, &root);          // 路径名字就是根元素
            else
                collect_children(root, st0, hits);        // 否则从根的子元素找起
        }
        current = apply_step(hits, st0);
        next_step = 1;
    } else {
        current.emplace_back(&root, &root);                // 空路径 / 只有 @attr：作用于当前节点
    }

    for (std::size_t k = next_step; k < pp.steps.size(); ++k) {
        const path_step& st = pp.steps[k];
        std::vector<hit_pair> hits;
        for (const hit_pair& c : current) {
            if (st.descendant) collect_descendants(*c.first, st, hits);
            else               collect_children(*c.first, st, hits);
        }
        current = apply_step(hits, st);
    }

    for (const hit_pair& h : current) {
        match m;
        m.target = h.first;
        m.parent = h.second;
        if (pp.has_attr_tail) m.attribute = pp.attr_tail;
        sel.items().push_back(std::move(m));
    }
    return sel;
}

} // namespace

std::string match::value() const
{
    if (!target) return {};
    if (is_attribute()) return target->attr(attribute);
    if (const text_data* t = target->as_text()) return t->value;
    if (std::vector<node>* ch = target->children())
        for (const node& c : *ch)
            if (const text_data* t = c.as_text()) return t->value;
    return {};
}

bool match::set_value(std::string v)
{
    if (!target) return false;

    if (is_attribute()) {
        target->set_attr(attribute, std::move(v));
        return true;
    }
    if (text_data* t = target->as_text()) {
        t->new_value = std::move(v);
        t->dirty = true;
        return true;
    }
    if (std::vector<node>* ch = target->children()) {
        for (node& c : *ch) {
            if (text_data* t = c.as_text()) {
                t->new_value = std::move(v);
                t->dirty = true;
                return true;
            }
        }
        // 元素没有任何直接文本：新建一个文本子节点
        node n;
        text_data t;
        t.new_value = std::move(v);
        t.dirty = true;
        n.content = std::move(t);
        ch->push_back(std::move(n));
        return true;
    }
    return false;
}

bool match::insert_before(node new_node)
{
    if (!target || !parent || target == parent) return false;
    const std::size_t idx = element_index_of(*parent, target);
    if (idx == npos_index) return false;
    return parent->insert_child(idx, std::move(new_node));
}

bool match::insert_after(node new_node)
{
    if (!target || !parent || target == parent) return false;
    const std::size_t idx = element_index_of(*parent, target);
    if (idx == npos_index) return false;
    return parent->insert_child(idx + 1, std::move(new_node));
}

bool match::remove()
{
    if (!target) return false;
    // 属性删除作用于 target 自身，不需要父节点（根元素上的属性也能删）
    if (is_attribute()) return target->remove_attr(attribute);

    if (!parent || target == parent) return false;   // 根元素没有父节点，不可删除
    const std::size_t idx = element_index_of(*parent, target);
    if (idx == npos_index) return false;
    return parent->remove_child(idx);
}

selection node::select(std::string_view path)
{
    return select_from(*this, path);
}
match node::find(std::string_view path)
{
    selection s = select_from(*this, path);
    return s.empty() ? match{} : s[0];
}

selection document::select(std::string_view path)
{
    if (!has_root_) return {};
    return select_from(root_, path);
}

match document::find(std::string_view path)
{
    selection s = select(path);
    return s.empty() ? match{} : s[0];
}

std::string document::get(std::string_view path, std::string def)
{
    match m = find(path);
    return m.valid() ? m.value() : std::move(def);
}

bool document::set(std::string_view path, std::string value)
{
    match m = find(path);
    if (!m.valid()) return false;
    return m.set_value(std::move(value));
}

} // namespace scl2::xml2
