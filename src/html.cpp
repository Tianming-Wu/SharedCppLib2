#include "html.hpp"

namespace html {

header::header()
{
    name = "head"; // name should be "head"
    xml::node::setSelfClosing(false); // always non-self-closing
}

header::header(const xml::node &node)
    : xml::node(node)
{}

header::header(const xml::node &&node)
    : xml::node(std::move(node))
{}

void header::addMetaTag(const std::string &name, const std::string &content)
{
    xml::node metaNode;
    metaNode.setName("meta");
    metaNode.setAttribute("name", name);
    metaNode.setAttribute("content", content);
    addChildNode(std::move(metaNode));
}

void header::setFavicon(const url& link)
{
    xml::node linkNode;
    linkNode.setName("link");
    linkNode.setAttribute("rel", "icon");
    linkNode.setAttribute("href", link.toString());
    addChildNode(std::move(linkNode));
}



body::body()
{
    name = "body"; // name should be "body"
    xml::node::setSelfClosing(false); // always non-self-closing
}

body::body(const xml::node &node)
    : xml::node(node)
{}

body::body(const xml::node &&node)
    : xml::node(std::move(node))
{}




Image::Image(const url &src, const std::string &alt)
    : xml::node()
{
    name = "img"; // name should be "img"
    setAttribute("src", src.toString());
    setAttribute("alt", alt);
    xml::node::setSelfClosing(true); // always self-closing
}

void Image::setSize(int width, int height)
{
    setAttribute("width", std::to_string(width));
    setAttribute("height", std::to_string(height));
}

void Image::resetSize()
{
    removeAttribute("width");
    removeAttribute("height");
}

Script::Script(const url& src)
{
    name = "script"; // name should be "script"
    setAttribute("src", src.toString());
    xml::node::setSelfClosing(false); // always non-self-closing
}

Script::Script(const std::string &inline_script)
{
    name = "script"; // name should be "script"
    setTextContent(inline_script);
    xml::node::setSelfClosing(false); // always non-self-closing
}

document::document()
{
    // set html doctype prolog
    xml_doc.setDoctypeDeclaration("<!DOCTYPE html>");

    // set root node to <html>
    xml::node& root = xml_doc.getRootNode();
    root.setName("html");
    


}

// These functions does not relate to logic and are just for convenience,
// So I just fold them to save some lines.
header &document::getHtmlHeader() { return html_header; }
header &document::header() { return html_header; }
void document::addHtmlHeader(::html::header &&hdr) { html_header = std::move(hdr); }
body &document::getHtmlBody(){ return html_body; }
body &document::body() { return html_body; }
void document::addHtmlBody(::html::body &&bdy) { html_body = std::move(bdy); }

void document::deserialize(const std::string& html_text)
{
    reset();
    // simply deserialize the prolog, then just give the rest of the content to root node

    xml_doc.deserialize(html_text);

    // after xml document is deserialized, we need to extract header and body nodes
    xml::node& root = xml_doc.getRootNode();
    if(root.getName().local_name != "html") {
        throw parser_error("Root node is not <html>");
    }

    const auto& children = root.getChildNodes();
    for(const auto& child : children) {
        if(child.getName().local_name == "head") {
            html_header = child;
        } else if(child.getName().local_name == "body") {
            html_body = child;
        }
    }
}

std::string document::serialize() const
{
    // before serializing, we need to set header and body nodes to root node
    xml::node& root = xml_doc.getRootNode();
    root.resetChildNodes();
    root.addChildNode(std::move(xml::node(html_header)));
    root.addChildNode(std::move(xml::node(html_body)));

    return xml_doc.serialize();
}

void document::clear()
{
    xml_doc.clear();
    html_header.clear();
    html_body.clear();
}

void document::reset()
{
    xml_doc.reset();
    html_header.reset();
    html_body.reset();
}

// but luckily it is almost the same as xml document's operators

std::istream &operator>>(std::istream &is, document &doc)
{
    std::string xml_text((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    doc.deserialize(xml_text);
    return is;
}

std::ostream &operator<<(std::ostream &os, const document &doc)
{
    os << doc.serialize();
    return os;
}

} // namespace html