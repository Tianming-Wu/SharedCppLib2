/*
    XML parser library module for SharedCppLib2.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.1.24

    The description of xml format and the documentation of this module
    can be found at the end of this file.
*/

#pragma once

#include <string>
#include <map>
#include <optional>
#include <vector>
#include <stdexcept>

#include "basics.hpp"

// forward declaration
namespace std {
    template<typename CharT> class basic_stringlist;
    using stringlist = basic_stringlist<char>; using wstringlist = basic_stringlist<wchar_t>;
    class bytearray;
}

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
Define_Enum_BitOperators(ParsingStrategy)

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
public:
    node() = default;
    ~node() = default;

    enable_copy_move(node) // use default copy/move for now

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

    std::string deserialize(const std::string& node_text); // returns remaining text after this node
    std::string serialize(int alignDepth = 0) const;

    bool hasChildren() const;
    std::vector<node>& getChildNodes();
    const std::vector<node>& getChildNodes() const;

    void addChildNode(node&& child);
    void removeChildNode(size_t index);

    // clear all content and set them to default.
    // Note that this does not clear name or namespace info.
    void clear();

    // reset to default state
    // Note that this actually make the object invalid,
    // so you better not serialize it before setting name and content again,
    // if it is in a document.
    void reset();

private:
    std::string name;
    std::optional<std::string> text_content;
    std::optional<std::vector<node>> children;
    std::map<qualified_name, std::string> attributes;
    std::optional<bool> self_closing; // if not set, auto-detect based on content

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

    enable_copy_move(document) // use default copy/move for now

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

private:
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


/*
    Library Theory Notice:

    XML OVERVIEW:
    XML (eXtensible Markup Language) is a markup language designed to store and transport data.
    It is widely used as the foundation for many file formats like XHTML, SVG, RSS, SOAP, etc.
    This module provides a general-purpose XML parser and serializer that can be extended
    to support XML-based file formats.

    DOCUMENT STRUCTURE:
    An XML document consists of three main parts:

    1. PROLOG (optional):
       Located at the beginning of the file, contains:
       - XML Declaration: <?xml version="1.0" encoding="UTF-8"?>
       - Document Type Declaration (DTD): <!DOCTYPE root SYSTEM "file.dtd">
       - Processing Instructions: <?target data?>
       - Comments: <!-- This is a comment -->
       - Whitespace and blank lines

    2. ROOT ELEMENT:
       Every well-formed XML document must have exactly one root element that contains
       all other content. For example:
           <root>
               <!-- All other elements go here -->
           </root>

    3. EPILOG (optional):
       Processing instructions and comments that appear after the root element.

    NODES AND ELEMENTS:
    Each node in the XML tree represents an element with:
    - Name (tag name): The identifier of the element
    - Attributes: Key-value pairs providing metadata. Syntax: <element key="value" foo="bar">
    - Text content: Plain text between opening and closing tags: <element>Text content</element>
    - Child nodes: Nested elements inside the element
    - Empty elements: Self-closing tags with no content: <element key="value"/>

    In this library, when serializing:
    - If a node has NO text content AND NO child nodes → automatically use self-closing tag
        You can still mannually set it to use opening and closing tags if desired.
        In the future html implementation, specific tags like <div></div> will always use opening/closing tags.
    - If a node has text content OR child nodes → use opening and closing tags

    CONTENT MODELS:
    XML elements can follow these patterns:
    - Empty content: <element/>
    - Element-only content: <element><child/></element>
    - Text-only content (PCDATA): <element>text only</element>
    - Mixed content: <element>text <child/>more text</element>

    ATTRIBUTES vs ELEMENTS:
    Attributes are suitable for:
    - Simple, single-value metadata
    - Properties that are not hierarchical
    - IDs, references, type information

    Elements are suitable for:
    - Complex, structured data
    - Hierarchical relationships
    - Content that might need attributes or children itself

    CHARACTER HANDLING:
    1. Predefined Entity References (must be escaped in element content):
       &lt;     <  (less-than)
       &gt;     >  (greater-than)
       &amp;    &  (ampersand)
       &quot;   "  (double quote, mainly in attributes)
       &apos;   '  (apostrophe, mainly in attributes)

    2. CDATA Sections:
       Literal text blocks where entity references are NOT interpreted:
           <![CDATA[This is <literal> & uninterpreted content]]>
       Useful for embedding code, scripts, or content with many special characters.
       Restrictions: Cannot contain "]]>" sequence

    3. Character References:
       Numeric references to any Unicode character:
       - Decimal: &#65; (represents 'A')
       - Hexadecimal: &#x41; (represents 'A')

    4. Whitespace Handling:
       - Whitespace between tags can be significant or insignificant
       - xml:space="preserve" attribute explicitly preserves whitespace
       - Most XML processors normalize whitespace in certain contexts

    NAMESPACES:
    Namespaces provide a mechanism to avoid element name conflicts:
    - Syntax: <prefix:element xmlns:prefix="http://example.com/ns">
    - Default namespace: <element xmlns="http://example.com/ns">
    - Scope: Namespace declarations apply to the element and its descendants
    - Qualified names: prefix:localname where prefix is defined by xmlns declaration

    COMMENTS:
    <!-- Comment text -->
    - Cannot appear before the XML declaration
    - Cannot appear inside tags
    - Cannot contain "--" or end with "-"
    - Cannot contain the string "--->"
    - Content is not processed by XML parser

    PROCESSING INSTRUCTIONS:
    <?target data?>
    - Used to pass information to the application
    - Format: <?name data?>
    - Example: <?xml-stylesheet type="text/xsl" href="style.xsl"?>

    PARSING STRATEGY FOR THIS LIBRARY:
    1. Lexical Analysis: Tokenize input stream
    2. Syntax Validation: Check well-formedness (not validity against schema)
    3. Tree Construction: Build in-memory DOM (Document Object Model) structure
    4. Error Handling: Report parsing errors with line/column information

    SERIALIZATION STRATEGY:
    1. Traverse the in-memory tree
    2. Generate properly formatted XML output
    3. Escape special characters appropriately
    4. Optionally pretty-print with indentation

    FEATURES TO SUPPORT:
    [Core Features]
    ✓ Basic parsing and serialization
    ✓ Attributes and text content
    ✓ Child nodes (hierarchical structure)
    ✓ Self-closing tags
    ✓ Comments
    ✓ Entity references (&lt;, &gt;, &amp;, &quot;, &apos;)

    [Extended Features - Consider for Implementation]
    - CDATA sections
    - Numeric character references (&#123;, &#xABC;)
    - Processing instructions
    - DTD/Schema validation (if needed)
    - Namespace support
    - XML declaration and encoding handling
    - Whitespace preservation control
    - XPath-like query capabilities
    - Pretty printing with indentation control

    [NOT Supporting (Out of Scope)]
    - Full DTD validation
    - Schema (XSD) validation
    - Full XSLT processing
    - Custom entity definitions

    DESIGN CONSIDERATIONS:
    - Performance: Choose appropriate data structures (std::map for attributes)
    - Memory: Consider node pooling for large documents
    - Encoding: Decide on UTF-8 as primary encoding vs multi-encoding support
    - API: Balance ease-of-use with flexibility
    - Error Recovery: Decide between strict parsing or lenient parsing

*/


/*
    How to use:

    #include <SharedCppLib2/xml.hpp>

    xml::document doc;

    // Read:
    std::ifstream ifs("file.xml");
    // parse xml from file
    ifs >> doc;
    ifs.close();

    xml::node& root = doc.getRootNode();
    std::string attr_value = root.getAttribute("key", "default");
    root.setAttribute("new_key", "new_value");


    // Write:
    std::ofstream ofs("output.xml");
    // serialize xml to file
    ofs << doc;
    ofs.close();

*/