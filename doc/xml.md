# xml - XML Parser/Serializer Library

+ Name: xml
+ Namespace: `xml`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|------|-------|
| Namespace | `SharedCppLib2` |
| Library | `xml` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::xml)
```

```cpp
#include <SharedCppLib2/xml.hpp>
```

## Description

`xml` provides a general-purpose XML parser and serializer for SharedCppLib2, conforming to the W3C XML 1.0 (Fifth Edition) specification. It is a standalone module with no internal module dependencies.

## XML Overview

XML (eXtensible Markup Language) is a markup language designed to store and transport data. It is widely used as the foundation for many file formats like XHTML, SVG, RSS, SOAP, etc.

### Document Structure

An XML document consists of three main parts:

1. **PROLOG** (optional):
   Located at the beginning of the file, contains:
   - XML Declaration: `<?xml version="1.0" encoding="UTF-8"?>`
   - Document Type Declaration (DTD): `<!DOCTYPE root SYSTEM "file.dtd">`
   - Processing Instructions: `<?target data?>`
   - Comments: `<!-- This is a comment -->`
   - Whitespace and blank lines

2. **ROOT ELEMENT**:
   Every well-formed XML document must have exactly one root element that contains all other content:
   ```xml
   <root>
       <!-- All other elements go here -->
   </root>
   ```

3. **EPILOG** (optional):
   Processing instructions and comments that appear after the root element.

### Node Types

This library supports four node types:

| Type | Description | Serialization |
|------|-------------|---------------|
| `Element` | Named element with attributes and children | `<name>...</name>` or `<name/>` |
| `Text` | Plain text content | Raw text (escaped) |
| `Comment` | Non-processed commentary | `<!-- text -->` |
| `ProcessingInstruction` | Application-targeted data | `<?target data?>` |

Each node in the XML tree represents an element with:
- **Name** (tag name): The identifier of the element
- **Attributes**: Key-value pairs providing metadata. Syntax: `<element key="value" foo="bar">`
- **Text content**: Plain text between opening and closing tags: `<element>Text content</element>`
- **Child nodes**: Nested elements inside the element
- **Empty elements**: Self-closing tags with no content: `<element key="value"/>`

### Serialization Behavior

In this library, when serializing:
- If a node has NO text content AND NO child nodes → automatically use self-closing tag
  You can still manually set it to use opening and closing tags if desired.
- If a node has text content OR child nodes → use opening and closing tags

### Content Models

XML elements can follow these patterns:
- **Empty content**: `<element/>`
- **Element-only content**: `<element><child/></element>`
- **Text-only content** (PCDATA): `<element>text only</element>`
- **Mixed content**: `<element>text <child/>more text</element>`

### Attributes vs Elements

**Attributes** are suitable for:
- Simple, single-value metadata
- Properties that are not hierarchical
- IDs, references, type information

**Elements** are suitable for:
- Complex, structured data
- Hierarchical relationships
- Content that might need attributes or children itself

## Character Handling

### Predefined Entity References

Must be escaped in element content:

| Entity | Character | Description |
|--------|-----------|-------------|
| `&lt;` | `<` | less-than |
| `&gt;` | `>` | greater-than |
| `&amp;` | `&` | ampersand |
| `&quot;` | `"` | double quote (mainly in attributes) |
| `&apos;` | `'` | apostrophe (mainly in attributes) |

### CDATA Sections

Literal text blocks where entity references are NOT interpreted:
```xml
<![CDATA[This is <literal> & uninterpreted content]]>
```

Useful for embedding code, scripts, or content with many special characters.
Restrictions: Cannot contain `]]>` sequence.

### Character References

Numeric references to any Unicode character:
- Decimal: `&#65;` (represents 'A')
- Hexadecimal: `&#x41;` (represents 'A')

### Whitespace Handling

- Whitespace between tags can be significant or insignificant
- `xml:space="preserve"` attribute explicitly preserves whitespace
- `xml:space="default"` restores default normalization behavior
- The `xml:space` attribute is inherited by child elements

## Namespaces

Namespaces provide a mechanism to avoid element name conflicts:
- Syntax: `<prefix:element xmlns:prefix="http://example.com/ns">`
- Default namespace: `<element xmlns="http://example.com/ns">`
- Scope: Namespace declarations apply to the element and its descendants
- Qualified names: `prefix:localname` where prefix is defined by xmlns declaration

## Comments

```
<!-- Comment text -->
```
- Cannot appear before the XML declaration
- Cannot appear inside tags
- Cannot contain `--` or end with `-`
- Content is not processed by XML parser

## Processing Instructions

```
<?target data?>
```
- Used to pass information to the application
- Format: `<?name data?>`
- Example: `<?xml-stylesheet type="text/xsl" href="style.xsl"?>`

## Parsing Strategy

1. **Lexical Analysis**: Tokenize input stream
2. **Syntax Validation**: Check well-formedness (not validity against schema)
3. **Tree Construction**: Build in-memory DOM (Document Object Model) structure
4. **Error Handling**: Report parsing errors

## Serialization Strategy

1. Traverse the in-memory tree
2. Generate properly formatted XML output
3. Escape special characters appropriately
4. Pretty-print with configurable indentation

## Feature Status

### Supported
- [x] Basic parsing and serialization
- [x] Element, Text, Comment, ProcessingInstruction node types
- [x] Attributes and text content
- [x] Child nodes (hierarchical structure)
- [x] Self-closing tags
- [x] Entity references (`&lt;`, `&gt;`, `&amp;`, `&quot;`, `&apos;`)
- [x] Numeric character references (`&#123;`, `&#xABC;`)
- [x] CDATA sections
- [x] Processing instructions
- [x] Comments
- [x] XML Namespaces (prefix:local notation, xmlns declarations)
- [x] XML Declaration and DOCTYPE preservation
- [x] Whitespace preservation control (`xml:space`)

### Consider for Future Implementation
- DTD/Schema validation (if needed)
- XPath-like query capabilities
- Custom entity definitions

### Not Supporting (Out of Scope)
- Full DTD validation
- Schema (XSD) validation
- Full XSLT processing

## Design Considerations

- **Performance**: Uses `std::map` for attributes, `std::vector` for children
- **Memory**: Node-based tree structure with optional members for sparse data
- **Encoding**: UTF-8 as primary encoding
- **API**: Balances ease-of-use with flexibility
- **Error Recovery**: Supports both strict and lenient parsing modes

## How to Use

```cpp
#include <SharedCppLib2/xml.hpp>

xml::document doc;

// Read from file:
std::ifstream ifs("file.xml");
ifs >> doc;
ifs.close();

xml::node& root = doc.getRootNode();
std::string attr_value = root.getAttribute("key", "default");
root.setAttribute("new_key", "new_value");

// Write to file:
std::ofstream ofs("output.xml");
ofs << doc;
ofs.close();
```
