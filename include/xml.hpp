/*
    XML parser library module for SharedCppLib2, standalone (no module dependency).
    Tianming Wu <https://github.com/Tianming-Wu> 2026.1.24

    Follows the W3C XML 1.0 (Fifth Edition) specification:
    - https://www.w3.org/TR/xml/

    The description of xml format and the documentation of this module
    can be found in doc/xml.md.

    The module is not designed to work using virtual function table,
    you should directly use your derived classes if you want to extend
    its functionality.

    Supported features:
    - Element, Text, Comment and ProcessingInstruction node types
    - Attributes with proper escaping
    - Self-closing tags
    - Entity references (&lt;, &gt;, &amp;, &quot;, &apos;)
    - Numeric character references (&#123;, &#xABC;)
    - CDATA sections
    - Processing Instructions
    - XML Namespaces (prefix:local notation, xmlns declarations)
    - XML Declaration and DOCTYPE preservation
    - Whitespace preservation control (xml:space)

    Not yet supported:
    - DTD parsing/validation

    [SCL_STANDALONE_MODULE]
    version: 0.2.1
    cpp_generation: cxx23
*/
#pragma once

#include <string>
#include <string_view>
#include <map>
#include <optional>
#include <vector>
#include <generator>
#include <stdexcept>

#include <type_traits>

// XML parsing and serialization utilities
namespace xml
{

// Parsing strategy flags is only reserved for future use
enum class ParsingStrategy : uint32_t {
    None = 0,
    StrictParsing       = 1 << 0, // enforce strict XML compliance
    LenientParsing      = 0 << 0, // allow some common XML malformations (default)
    AutoUpdateOnEdit    = 1 << 1, // automatically update internal structure on node edits
    PreserveWhitespace  = 1 << 2, // preserve insignificant whitespace
};
inline constexpr ParsingStrategy operator|(ParsingStrategy a, ParsingStrategy b) noexcept {
    using T = std::underlying_type_t<ParsingStrategy>;
    return static_cast<ParsingStrategy>(static_cast<T>(a) | static_cast<T>(b));
}
inline constexpr ParsingStrategy operator^(ParsingStrategy a, ParsingStrategy b) noexcept {
    using T = std::underlying_type_t<ParsingStrategy>;
    return static_cast<ParsingStrategy>(static_cast<T>(a) ^ static_cast<T>(b));
}
inline constexpr ParsingStrategy operator&(ParsingStrategy a, ParsingStrategy b) noexcept {
    using T = std::underlying_type_t<ParsingStrategy>;
    return static_cast<ParsingStrategy>(static_cast<T>(a) & static_cast<T>(b));
}
inline constexpr ParsingStrategy operator~(ParsingStrategy a) noexcept {
    using T = std::underlying_type_t<ParsingStrategy>;
    return static_cast<ParsingStrategy>(~static_cast<T>(a));
}

// We also need a strategy for tabbing/aligning during serialization
// now use a constant value
constexpr int tab_size = 4; // number of spaces per tab

// this name is to avoid conflict with C++ "namespace" keyword
// XML namespace
struct xnamespace {
    std::optional<std::string> prefix;
    std::string uri;

    bool isDefault() const;
};

class qualified_name {
public:
    qualified_name() = default;
    qualified_name(const char* name);
    qualified_name(std::string_view name);
    qualified_name(const std::string& name);

    std::optional<std::string> prefix;
    std::string local_name;

    operator std::string() const;

    // comparison operator for use as std::map key
    // prefix (empty first) takes priority, then local_name
    bool operator<(const qualified_name& other) const;
};

// parsing_error is thrown when there is a syntax error during XML parsing
class parsing_error : public std::runtime_error {
public: explicit parsing_error(const std::string& message) : std::runtime_error("XML Parsing Error: " + message) {}
};
// parser_error is thrown when some of builtin login did not work as intended
class parser_error : public std::runtime_error {
public: explicit parser_error(const std::string& message) : std::runtime_error("XML Parser Error: " + message) {}
};

// xml single node
class node
{
    friend class document;
public:
    /// @brief Node type discriminator.
    enum class Type : uint8_t { Element, Text, Comment, ProcessingInstruction };

    node() = default;
    ~node() = default;

    node(const node&) = default;
    node& operator=(const node&) = default;
    node(node&&) = default;
    node& operator=(node&&) = default;

    /// @brief Create a comment node.
    static node create_comment(const std::string& text);
    /// @brief Create a text node.
    static node create_text(const std::string& text);
    /// @brief Create a processing instruction node.
    static node create_processing_instruction(const std::string& target, const std::string& data);

    Type type() const { return type_; }
    bool is_element() const { return type_ == Type::Element; }
    bool is_text() const { return type_ == Type::Text; }
    bool is_comment() const { return type_ == Type::Comment; }
    bool is_processing_instruction() const { return type_ == Type::ProcessingInstruction; }

    /// @brief Get PI target (only valid for ProcessingInstruction nodes).
    std::string pi_target() const { return pi_target_; }
    /// @brief Get PI data (only valid for ProcessingInstruction nodes).
    std::string pi_data() const { return pi_data_; }

    qualified_name getName() const;
    void setName(const qualified_name& name);

    std::string fullQualifiedName() const; // with namespace prefix if any
    std::string fullRestrictedName() const; // with namespace uri if any

    bool hasAttribute(const qualified_name& key) const;
    std::string getAttribute(const qualified_name& key, const std::string& default_value = "") const;
    void setAttribute(const qualified_name& key, const std::string& value);
    void removeAttribute(const qualified_name& key);

    bool hasTextContent() const;
    std::string getTextContent(const std::string& default_value = "") const;
    void setTextContent(const std::string& text);
    void removeTextContent();

    bool isSelfClosingSet() const; // whether self-closing style is manually set
    bool isSelfClosing() const;
    /// @brief force self-closing style. If contains any text or child nodes, this will be ignored during serialization.
    void setSelfClosing(bool self_closing);
    void cancelSetSelfClosing(); // clear manual setting

    void declareNamespace(const std::string& prefix, const std::string& uri);
    std::optional<std::vector<xnamespace>> getDeclaredNamespaces() const; // get namespace prefix and uri if declared
    void undeclareNamespace(const std::string& prefix);
    void removeNamespace(); // remove all namespace

    /// @brief Parse XML from string. Returns remaining text after this node.
    /// @param preserve_whitespace If true, do not trim leading/trailing whitespace in text content.
    std::string deserialize(const std::string& node_text, bool preserve_whitespace = false);
    std::string serialize(int alignDepth = 0) const;

    bool hasChildren() const;
    std::vector<node>& getChildNodes();
    const std::vector<node>& getChildNodes() const;

    /// @brief Yield only Element-typed children (skip Text and Comment).
    std::generator<const node&> elements() const {
        if (children.has_value()) {
            for (const auto& child : children.value()) {
                if (child.is_element()) co_yield child;
            }
        }
    }

    std::generator<const node&> comments() const {
        if (children.has_value()) {
            for (const auto& child : children.value()) {
                if (child.is_comment()) co_yield child;
            }
        }
    }

    /// @brief Yield only ProcessingInstruction-typed children.
    std::generator<const node&> processing_instructions() const {
        if (children.has_value()) {
            for (const auto& child : children.value()) {
                if (child.is_processing_instruction()) co_yield child;
            }
        }
    }

    void addChildNode(node&& child);
    void removeChildNode(size_t index);
    void resetChildNodes();

    // clear all content and set them to default.
    // Note that this does not clear name or namespace info.
    void clear();

    // reset to default state
    // Note that this actually make the object invalid,
    // so you better not serialize it before setting name and content again,
    // if it is in a document.
    void reset();

protected:
    Type type_ = Type::Element;

    std::string name;
    std::optional<std::string> text_content;
    std::optional<std::vector<node>> children;
    std::map<qualified_name, std::string> attributes;
    std::optional<bool> self_closing; // if not set, auto-detect based on content

    std::string pi_target_; // processing instruction target
    std::string pi_data_;   // processing instruction data

    std::optional<xnamespace> xmlns; // namespace that the node belongs to
    std::optional<std::vector<xnamespace>> decl_xmlns; // declared namespaces in this node
    std::optional<std::vector<xnamespace>> inherited_xmlns; // inherited namespaces from parent nodes
};

// xml declaration
// this can only be parsed/deserialized from a string,
// and serialize is not supported.
// struct xml_decl
// {
//     std::string version = "1.0";
//     std::optional<std::string> encoding; // e.g., "UTF-8"
//     std::optional<bool> standalone;
// };

// xml document
class document
{

public:
    document() = default;
    ~document() = default;

    document(const document&) = default;
    document& operator=(const document&) = default;
    document(document&&) = default;
    document& operator=(document&&) = default;

    node& getRootNode();
    void setRootNode(const node& root); // Warn: replaces the entire document if root node is changed

    std::optional<std::string> getXmlDeclaration() const;
    void setXmlDeclaration(const std::string& declaration);

    std::optional<std::string> getDoctypeDeclaration() const;
    void setDoctypeDeclaration(const std::string& doctype);

    // XML parsing and serialization
    void deserialize(const std::string& xml_text);
    std::string serialize() const;

    // Make some of the functions using the same name as node,
    // and link to root_node's functions.
    // For simplicity, use inline forwarding functions.
    inline bool hasChildren() const { return root_node.hasChildren(); }
    inline std::vector<node>& getChildNodes() { return root_node.getChildNodes(); }
    inline const std::vector<node>& getChildNodes() const { return root_node.getChildNodes(); }

    // Collect information from the tree
    std::vector<xnamespace> collectAllNamespaces() const;

    void clear(); // clear entire document to default state
    void reset(); // reset entire document to invalid state

protected:
    node root_node;

    // prolog components
    std::optional<std::string> xml_declaration;
    std::optional<std::string> doctype_declaration;
};

// operators
std::istream& operator>>(std::istream& is, document& doc);
std::ostream& operator<<(std::ostream& os, const document& doc);


// helper functions
std::string makeTab(int depth);
std::string trimString(const std::string& str);
std::string escapeTextContent(const std::string& text);
std::string unescapeTextContent(const std::string& text);

} // namespace xml