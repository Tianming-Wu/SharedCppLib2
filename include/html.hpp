/*
    HTML parser and serializer module for SharedCppLib2.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.1.26

    Warning:
        When deserializing from text, the class actually ignores any node
        that is not <head> or <body> under the root <html> node.
        So if you have comments or other nodes there, they will be lost. 

    This module is derived from xml module.
    You don't need to link xml module separately when using html module.

    link target:
        SharedCppLib2::html
    link target (parent):
        SharedCppLib2::xml

*/
#pragma once

#include "xml.hpp"
#include "uri.hpp"

namespace html
{

using parser_error = xml::parser_error;
using parsing_error = xml::parsing_error;

using xml::qualified_name;

using uri = uri;
using url = url;

// Header Node of html module
class header : public xml::node
{
public:
    header();
    header(const xml::node& node);
    header(const xml::node&& node);

    ~header() = default;

    enable_copy_move(header) // use default copy/move for now

    void addMetaTag(const std::string& name, const std::string& content);
    
    // icon
    void setFavicon(const url& link);

    // ---- This part is to disable some xml::node functions ----

    // we do not want user to change the name of header node
    qualified_name getName() const = delete;
    void setName(const qualified_name& name) = delete;

    // explicitly disable self-closing related functions, as we always want header to be non-self-closing
    bool isSelfClosingSet() const = delete;
    bool isSelfClosing() const = delete;
    void setSelfClosing(bool self_closing) = delete;
    void cancelSetSelfClosing() = delete;
};

// Body Node of html module
class body : public xml::node
{
public:
    body();
    body(const xml::node& node);
    body(const xml::node&& node);
    
    ~body() = default;

    enable_copy_move(body) // use default copy/move for now
};


// Friendly insert types
class Image : protected xml::node
{
public:
    Image(const url& src, const std::string& alt = "");

    void setSize(int width, int height);
    void resetSize(); // remove size attributes

    ~Image() = default;

    enable_copy_move(Image) // use default copy/move for now
};


class Script : protected xml::node
{
public:
    Script(const url& src); // Load script from url
    Script(const std::string& inline_script); // Load inline script

    ~Script() = default;

    enable_copy_move(Script) // use default copy/move for now
};



// we do not derive from xml::document,
// so I don't have to delete many functions.
class document
{
public:
    document(); // inject html features first
    ~document() = default;

    // header management
    header& getHtmlHeader();
    header& header(); // alias
    // Note: this overwrites previous header if any
    void addHtmlHeader(::html::header &&hdr);

    body& getHtmlBody();
    body& body(); // alias
    // Note: this overwrites previous body if any
    void addHtmlBody(::html::body &&bdy);

    // parsing and serializing
    void deserialize(const std::string& html_text);
    std::string serialize() const;

    void clear(); // clear entire document to default state
    void reset(); // reset entire document to invalid state

protected:
    mutable xml::document xml_doc;

    ::html::header html_header;
    ::html::body html_body;
};


// sadly we need to implement operators separately
std::istream& operator>>(std::istream& is, document& doc);
std::ostream& operator<<(std::ostream& os, const document& doc);

} // namespace html


/*
    Developer Note:

    As for HTML standard, I want to provide full-featured html construction, meaning
    that it is possible to build a valid html file with all features using this library.

    However, the priority of this library is relatively low, so maybe that would not be
    possible in the near future.

    To achieve this goal, this library provides simplified syntax for common html features.

    I may have to also modify xml library for a better construction experience.

    Typical tags are:
    <head> </head> - meta tags, title, favicon, etc.
    <body> </body> - content of the page, including text, images, scripts, etc.

    <a> </a> - hyperlinks, which can be easily constructed by creating a node with name "a" and setting href attribute.
    class name: *Anchor
        - supported attributes: href, target, rel, etc.
    
    <img/> - images, which can be easily constructed by creating an Image object with src and alt attributes.
    class name: Image
        - self-closing
        - supported attributes: src, alt, width, height

    <script> </script> - scripts, which can be easily constructed by creating a Script object with src or inline script content.
    class name: Script
        - supported attributes: src
        - inline script content is supported by setting text content of the node.

*/