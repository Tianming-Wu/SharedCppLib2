/*
    XML2 — lossless-capable XML module (experimental rewrite of xml)

    Goals:
      - fidelity::raw      : 解析时保存原文片段；序列化时未修改的部分逐字节原样输出
                             （注释、空行、缩进、属性顺序、引号风格、实体/CDATA 拼写全保留）
      - fidelity::semantic : 只保留语义（值 + 结构）：不保存原文、丢弃纯空白布局，
                             序列化时用紧凑格式重新生成（省内存，适合只取值的大文件）
      - 属性保序存放（vector），文本作为 children 的成员 → 支持混合内容
      - 修改只影响被改动的单元；同一开始标签内的其他属性排版不受影响

    Design notes:
      - children 是原文顺序的**完整分解**：元素、文本（含纯空白）、注释、PI 全部作为子节点。
        因此“序列化 = 递归拼接”，天然保真，无需额外的 trivia 机制。
      - 每个 element 保存 raw_open / raw_close 原文；只有被标记 dirty 的元素才重新生成
        开始标签，且生成时使用解析期记录的空白（attr.lead / name_tail），因此改一个属性值
        不会影响同一标签里其他属性的排版。
      - 文本节点保存 raw（原文，保留实体拼写或 CDATA 包装）+ 解码后的 value。

    Not implemented yet:
      - 谓词逻辑运算（and/or）、text()/last()、命名空间 URI 解析、DTD/实体定义
      - 结构编辑的缩进推断是"局部"的：新节点沿用兄弟缩进，已有节点不会被重新排版

    [SCL_STANDALONE_MODULE]
    version: 0.1.0
    cpp_generation: cxx17 - cxx23
*/
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scl2::xml2 {

/// 解析时的保真级别。
enum class fidelity : std::uint8_t {
    /// 只保留语义（值 + 结构）：不保存元素/属性的原文形式与文本的实体拼写，
    /// 丢弃纯空白布局（prolog 仅保留 XML 声明、epilog 丢弃），序列化时紧凑生成。
    /// 注释与 PI 仍会保留（它们没有其他可提取的形态）。
    semantic = 0,
    /// 保真：保存原文片段，序列化时未修改部分逐字节原样输出（默认）。
    raw = 1,
};

class parsing_error : public std::runtime_error {
public:
    explicit parsing_error(const std::string& msg)
        : std::runtime_error("xml2 parsing error: " + msg) {}
};

// ── 属性：保序存放，保留原文空白与引号风格 ──────────────────────────────
struct attribute {
    std::string lead;        // 前导空白（含换行/缩进）
    std::string name;        // 原文名（可含命名空间前缀）
    std::string sep;         // 名字与值之间，如 " = "
    char        quote = '"'; // 原文引号
    std::string raw_value;   // 原值文本（保留实体拼写）
    std::string value;       // 解码后的值
    bool        value_dirty = false;   // 值被修改 → 该属性重新生成
};

struct node;
struct match;
class selection;

struct element {
    std::string name;                // 原文名（可含前缀）
    std::string name_tail;           // 最后一个属性（或名字）之后、结束符之前的空白
    std::vector<attribute> attrs;    // 保序
    bool        self_closed = false;
    std::string raw_open;            // 原文开始标签：'<' … '>'（含）
    std::string raw_close;           // 原文结束标签 "</…>"；自闭合时为空
    std::vector<node> children;      // 原文顺序的完整分解
    bool        dirty = false;       // 属性或结构被改 → 开始标签重新生成
};

struct text_data {
    std::string raw;          // 原文（含实体引用或 CDATA 包装）
    std::string value;        // 解码后的文本
    bool        cdata = false;
    bool        dirty = false;
    std::string new_value;    // dirty 时的新值（解码态，输出时转义）
};

struct comment_data { std::string raw; };   // "<!-- … -->" 原文
struct pi_data      { std::string raw; };   // "<? … ?>" 原文

struct node {
    std::variant<element, text_data, comment_data, pi_data> content;

    bool is_element() const { return std::holds_alternative<element>(content); }
    bool is_text()    const { return std::holds_alternative<text_data>(content); }
    bool is_comment() const { return std::holds_alternative<comment_data>(content); }
    bool is_pi()      const { return std::holds_alternative<pi_data>(content); }

    element*         as_element()       { return std::get_if<element>(&content); }
    const element*   as_element() const { return std::get_if<element>(&content); }
    text_data*       as_text()          { return std::get_if<text_data>(&content); }
    const text_data* as_text() const    { return std::get_if<text_data>(&content); }

    /// 元素名（非元素返回空）
    std::string_view name() const {
        const element* e = as_element();
        return e ? std::string_view(e->name) : std::string_view();
    }

    /// 文本值：文本节点返回自身；元素返回其第一个文本子节点（直接文本）
    std::string_view value() const {
        if (const text_data* t = as_text()) return t->value;
        if (const element* e = as_element()) {
            for (const node& c : e->children)
                if (const text_data* t = c.as_text()) return t->value;
        }
        return {};
    }

    const std::vector<attribute>* attributes() const {
        const element* e = as_element();
        return e ? &e->attrs : nullptr;
    }

    bool has_attr(std::string_view key) const {
        const element* e = as_element();
        if (!e) return false;
        for (const attribute& a : e->attrs)
            if (a.name == key) return true;
        return false;
    }

    std::string attr(std::string_view key, std::string def = {}) const {
        const element* e = as_element();
        if (!e) return def;
        for (const attribute& a : e->attrs)
            if (a.name == key) return a.value;
        return def;
    }

    std::vector<node>* children() {
        element* e = as_element();
        return e ? &e->children : nullptr;
    }
    const std::vector<node>* children() const {
        const element* e = as_element();
        return e ? &e->children : nullptr;
    }

    /// 第一个同名子元素（不含更深的层级）
    node* child(std::string_view n) {
        element* e = as_element();
        if (!e) return nullptr;
        for (node& c : e->children) {
            const element* ce = c.as_element();
            if (ce && ce->name == n) return &c;
        }
        return nullptr;
    }
    const node* child(std::string_view n) const {
        const element* e = as_element();
        if (!e) return nullptr;
        for (const node& c : e->children) {
            const element* ce = c.as_element();
            if (ce && ce->name == n) return &c;
        }
        return nullptr;
    }

    /// 第 n 个（0-based）同名子元素
    node* child_at(std::string_view n, std::size_t index) {
        element* e = as_element();
        if (!e) return nullptr;
        for (node& c : e->children) {
            const element* ce = c.as_element();
            if (ce && ce->name == n) { if (index-- == 0) return &c; }
        }
        return nullptr;
    }
    const node* child_at(std::string_view n, std::size_t index) const {
        const element* e = as_element();
        if (!e) return nullptr;
        for (const node& c : e->children) {
            const element* ce = c.as_element();
            if (ce && ce->name == n) { if (index-- == 0) return &c; }
        }
        return nullptr;
    }

    // ── 修改（只影响被改动的单元）──────────────────────────────────────

    /// 设置 / 追加属性。已存在则只重写该属性值，同一标签内其他属性排版不变。
    void set_attr(std::string_view key, std::string new_value);

    /// 设置文本（仅对文本节点有效）。
    void set_text(std::string new_value);

    /// 追加子节点。
    /// 父元素是多行结构时，自动沿用兄弟元素的缩进与结束标签前的空白；
    /// 紧凑单行结构保持紧凑（不会被展开）。
    void append_child(node n);

    /// 在第 n 个（0-based）子元素之前插入；n 超出元素个数时追加到末尾。
    /// 缩进规则同 append_child：多行结构沿用兄弟缩进，紧凑结构保持紧凑。
    bool insert_child(std::size_t element_index, node n);

    /// 删除第 n 个（0-based）子元素，连同它的**前导空白**；
    /// 若删完不再有元素子节点，遗留的纯空白也会一并清理。
    bool remove_child(std::size_t element_index);

    /// 删除属性（连同它的前导空白）。属性不存在时返回 false。
    bool remove_attr(std::string_view key);

    // ── 路径查询（语法说明见 doc/xml2.md）────────────────────────────

    /// 相对本节点查询，返回全部匹配（保持文档顺序）。
    selection select(std::string_view path);
    /// 相对本节点查询，只取第一个匹配。
    match find(std::string_view path);
};

// ── 路径查询结果 ─────────────────────────────────────────────────────────

/// 一次匹配的结果项。
/// 命中属性时 attribute 非空（target 为属性所属的元素）。
struct match {
    node*       target = nullptr;
    node*       parent = nullptr;   // 父节点；结构性编辑需要（由 select()/find() 填充）
    std::string attribute;          // 非空 = 命中属性

    bool valid() const { return target != nullptr; }
    bool is_attribute() const { return target != nullptr && !attribute.empty(); }
    /// target 就是根元素（没有父节点可供编辑）
    bool is_root() const { return target != nullptr && target == parent; }

    /// 取值：命中属性 → 属性值；命中文本节点 → 文本；命中元素 → 第一个直接文本
    std::string value() const;

    /// 就地写入（属性值 / 文本）。命中元素且无直接文本时，会新建一个文本子节点。
    bool set_value(std::string v);

    // ── 结构性编辑（需要 match 来自 select()/find() 才有 parent 信息）──

    /// 在命中节点之前插入兄弟节点。
    bool insert_before(node new_node);
    /// 在命中节点之后插入兄弟节点。
    bool insert_after(node new_node);
    /// 删除命中节点（命中属性时删除该属性）。
    bool remove();
};

/// 路径查询的结果集（保持文档顺序）。
class selection {
public:
    using container = std::vector<match>;

    std::size_t size() const { return m_items.size(); }
    bool empty() const { return m_items.empty(); }

    const match& operator[](std::size_t i) const { return m_items[i]; }
    const match& first() const {
        static const match empty_match{};
        return m_items.empty() ? empty_match : m_items.front();
    }

    /// 第一个匹配的值（无匹配则返回 def）
    std::string value(std::string def = {}) const {
        return m_items.empty() ? std::move(def) : m_items.front().value();
    }

    container::iterator       begin()       { return m_items.begin(); }
    container::iterator       end()         { return m_items.end(); }
    container::const_iterator begin() const { return m_items.begin(); }
    container::const_iterator end()   const { return m_items.end(); }

    container&       items()       { return m_items; }
    const container& items() const { return m_items; }

private:
    container m_items;
};

class document {
public:
    document() = default;

    /// 解析 XML 文本。fidelity::raw 时保存原文以支持保真序列化。
    static document parse(std::string text, fidelity f = fidelity::raw);

    /// 序列化：未修改的部分逐字节原样输出。
    std::string serialize() const;

    fidelity mode() const { return mode_; }

    bool has_root() const { return has_root_; }
    node& root() { return root_; }
    const node& root() const { return root_; }

    /// 根元素之前的原文（XML 声明 / DOCTYPE / 注释 / 空白）
    const std::string& prolog() const { return prolog_; }
    /// 根元素之后的原文（尾随空白、注释等）
    const std::string& epilog() const { return epilog_; }

    // ── 路径查询 / 便捷读写（语法说明见 doc/xml2.md）──────────────────

    /// 查询所有匹配（相对根元素）。
    selection select(std::string_view path);
    /// 第一个匹配；无匹配时 match::valid() 为 false。
    match find(std::string_view path);
    /// 第一个匹配的值；无匹配返回 def。
    std::string get(std::string_view path, std::string def = {});
    /// 写入第一个匹配（属性值或文本），成功返回 true。
    bool set(std::string_view path, std::string value);

private:
    fidelity mode_ = fidelity::raw;
    bool has_root_ = false;
    std::string prolog_;
    std::string epilog_;
    node root_;
};

} // namespace scl2::xml2
