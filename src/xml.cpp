/*
    XML Library implementation file,
    As part of SharedCppLib2 project.
*/
#include "xml.hpp"

#include <functional>
#include <sstream>

namespace xml
{


qualified_name::qualified_name(const std::string &name)
{
    // check for ':'
    size_t colon_pos = name.find(':');
    if(colon_pos != std::string::npos) {
        prefix = name.substr(0, colon_pos);
        local_name = name.substr(colon_pos + 1);
    } else {
        local_name = name;
    }
}

// this is basically to_string (
qualified_name::operator std::string() const
{
    std::string result;
    if(prefix.has_value()) {
        result += prefix.value() + ":";
    }
    result += local_name;
    return result;
}

bool qualified_name::operator<(const qualified_name& other) const
{
    // prefix takes priority: empty prefix is smallest
    bool this_has_prefix = prefix.has_value();
    bool other_has_prefix = other.prefix.has_value();
    
    if(this_has_prefix != other_has_prefix) {
        return !this_has_prefix; // no prefix is smaller
    }
    
    // both have or both don't have prefix, compare prefix values
    if(this_has_prefix) {
        int prefix_cmp = prefix.value().compare(other.prefix.value());
        if(prefix_cmp != 0) {
            return prefix_cmp < 0;
        }
    }
    
    // prefix same, compare local_name
    return local_name < other.local_name;
}

qualified_name node::getName() const
{
    return name;
}

void node::setName(const qualified_name &name)
{
    this->name = name.local_name;
    this->xmlns = xnamespace{ name.prefix, "" }; // uri to be set later
}

std::string node::fullQualifiedName() const
{
    std::string result;
    if(xmlns.has_value() && !xmlns->isDefault()) {
        result += xmlns->prefix.value_or("") + ":";
    }
    result += name;
    return result;
}

std::string node::fullRestrictedName() const
{
    std::string result;
    if(xmlns.has_value() && !xmlns.value().isDefault()) {
        result += "{" + xmlns->uri + "}";
    }
    result += name;
    return result;
}

bool node::hasAttribute(const qualified_name &key) const
{
    return attributes.contains(key);
}

std::string node::getAttribute(const qualified_name &key, const std::string &default_value) const
{
    auto it = attributes.find(key);
    if(it != attributes.end()) {
        return it->second;
    }
    return default_value;
}

void node::setAttribute(const qualified_name &key, const std::string &value)
{
    attributes[key] = value;
}

void node::removeAttribute(const qualified_name &key)
{
    attributes.erase(key);
}

bool node::hasTextContent() const
{
    return text_content.has_value();
}

std::string node::getTextContent(const std::string &default_value) const
{
    if(text_content.has_value()) {
        return text_content.value();
    }
    return default_value;
}

void node::setTextContent(const std::string &text)
{
    text_content = text;
}

void node::removeTextContent()
{
    text_content.reset();
}

bool node::isSelfClosingSet() const
{
    return self_closing.has_value();
}

bool node::isSelfClosing() const
{
    return self_closing.value_or(
        (!text_content.has_value() || text_content->empty()) &&
        (!children.has_value() || children->empty())
    );
}

void node::setSelfClosing(bool self_closing)
{
    this->self_closing = self_closing;
}

void node::cancelSetSelfClosing()
{
    self_closing.reset();
}

void node::declareNamespace(const std::string &prefix, const std::string &uri)
{
    // ensure exists
    if(!decl_xmlns.has_value()) {
        decl_xmlns = std::vector<xnamespace>();
    }
    decl_xmlns->push_back({prefix, uri});
}

std::optional<std::vector<xnamespace>> node::getDeclaredNamespaces() const
{
    return decl_xmlns;
}

void node::undeclareNamespace(const std::string &prefix)
{
    if(!decl_xmlns.has_value()) return;
    auto &ns_list = decl_xmlns.value();
    ns_list.erase(
        std::remove_if(ns_list.begin(), ns_list.end(),
            [&](const xnamespace &ns) {
                return ns.prefix.has_value() && ns.prefix.value() == prefix;
            }),
        ns_list.end()
    );
}

void node::removeNamespace()
{
    xmlns.reset();
}

std::string node::deserialize(const std::string &node_text)
{
    // uh,
    reset(); // clears everything on this planet

    const std::string &working_text = node_text;
    // std::stringstream working_set(node_text);

    // find the tag structure <name>, possibly with attributes.
    // Also, if self-closing, actually get the full tag.
    // while(true) { // we may need to retry for a few times before we get a valid line, so use a loop
    //     // get a single line
    //     std::getline(working_set, working_text);
    //
    //     working_text = trimString(working_text);
    //     // check for starting '<' and ending '>'
    //
    //     // empty line or only spaces, ignore
    //     if(working_text.empty()) {
    //         continue; // get the next line
    //     }
    //
    //     // find the starting '<'
    //     if(!working_text.starts_with('<')) {
    //         throw parsing_error("Node text does not start with '<'");
    //     }
    //
    //     // find the ending '>'
    //     size_t end_pos = working_text.find('>');
    //     if(end_pos == std::string::npos) {
    //         // try append following lines, until we find a ending '>'
    //         while(true) {
    //             std::string next_line;
    //             if(std::getline(working_set, next_line)) {
    //                 working_text += " " + trimString(next_line);
    //                 end_pos = working_text.find('>');
    //                 if(end_pos != std::string::npos) {
    //                     break; // found it
    //                 }
    //             } else {
    //                 // no more lines, still not found
    //                 // then clearly I'm not able to parse this
    //                 throw parsing_error("Node text does not end with '>'");
    //             }
    //         }
    //     }
    //     break; // got a valid working_text containing a full tag
    // }

    // find tag start and end
    size_t tag_start = working_text.find('<');
    if(tag_start == std::string::npos) {
        throw parsing_error("Node text does not start with '<'");
    }

    size_t end_pos = working_text.find('>', tag_start);
    if(end_pos == std::string::npos) {
        throw parsing_error("Node text does not end with '>'");
    }

    // first, check if there IS a name
    size_t name_start = working_text.find_first_not_of(" \t\r\n", 1);
    if(name_start == std::string::npos || name_start >= end_pos) {
        throw parsing_error("Node name is missing");
    }
    size_t name_end = working_text.find_first_of(" \t\r\n/>", name_start);
    if(name_end == std::string::npos || name_end <= name_start) {
        throw parsing_error("Node name is missing");
    }

    // extract name (with possible namespace prefix at this point)
    std::string full_name = working_text.substr(name_start, name_end - name_start);

    // check for namespace prefix
    std::map<std::string, std::string> pending_namespaces; // prefix -> uri

    // parse attributes and possible namespace declarations
    size_t attr_pos = name_end;
    [&]() {
        std::stringstream attr_stream(working_text.substr(attr_pos, end_pos - attr_pos));
        while(true) {
            // skip spaces
            char ch;
            do {
                attr_stream.get(ch);
                if(attr_stream.eof() || ch == '>') {
                    return; // exit the lambda
                }
            } while(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');

            attr_stream.unget(); // put back the last read character

            // read attribute name
            std::string attr_name;
            std::getline(attr_stream, attr_name, '=');
            attr_name = trimString(attr_name);
            if(attr_name.empty()) {
                throw parsing_error("Attribute name is missing");
            }

            // read '='
            attr_stream.get(ch);
            if(ch != '=') {
                throw parsing_error("Expected '=' after attribute name");
            }

            // read attribute value
            char quote_char;
            attr_stream.get(quote_char);
            if(quote_char != '"' && quote_char != '\'') {
                throw parsing_error("Attribute value must start with a quote");
            }

            std::string attr_value;
            std::getline(attr_stream, attr_value, quote_char);

            // check if this is a namespace declaration
            if(attr_name == "xmlns") {
                // default namespace
                pending_namespaces[""] = attr_value;
            } else if(attr_name.starts_with("xmlns:")) {
                std::string ns_prefix = attr_name.substr(6);
                pending_namespaces[ns_prefix] = attr_value;
            } else {
                // normal attribute
                // attribute name may also have namespace prefix,
                // in that case it is automatically parsed by qualified_name,
                // which is now the key type of attributes map.
                attributes[qualified_name(attr_name)] = unescapeTextContent(attr_value);
            }
        }
        
    }(); // call it

    // resolve namespaces
    if(!pending_namespaces.empty()) {
        for(const auto &ns : pending_namespaces) {
            declareNamespace(ns.first, ns.second);
        }
    }

    // resolve node namespace
    {
        size_t colon_pos = full_name.find(':');
        if(colon_pos != std::string::npos) {
            // has namespace prefix
            std::string prefix = full_name.substr(0, colon_pos);
            std::string local_name = full_name.substr(colon_pos + 1);
            
            if(decl_xmlns.has_value()) {
                for(const auto &ns : decl_xmlns.value()) {
                    if(ns.prefix.has_value() && ns.prefix.value() == prefix) {
                        xmlns = ns;
                        break;
                    }
                }
            }
            // also check inherited namespaces from ancestors
            if(!xmlns.has_value() && inherited_xmlns.has_value()) {
                for(const auto &ns : inherited_xmlns.value()) {
                    if(ns.prefix.has_value() && ns.prefix.value() == prefix) {
                        xmlns = ns;
                        break;
                    }
                }
            }

            if(!xmlns.has_value()) {
                throw parsing_error("Namespace prefix '" + prefix + "' not declared");
            }

            name = local_name;
        } else {
            name = full_name;
        }
    }

    // check if self-closing
    if(working_text[end_pos - 1] == '/') {
        self_closing = true;
        // we are done here, but we need to remove the end tag
        // before returning the remaining text.

        std::string remaining_text = working_text.substr(end_pos + 1);
        remaining_text = trimString(remaining_text);
        return remaining_text;
    } else {
        self_closing = false;
    }

    // extract text content if any
    // note that in theory, working_text only contains the tag part,
    // so we need to append whatever remains in working_set to working_text.
    // std::
    std::string remaining_text = working_text.substr(end_pos + 1);

    remaining_text = trimString(remaining_text);
    if(!remaining_text.empty()) {
        while(true) { // we may need to look for multiple children or text content, before we find the end tag
            // check the type of the next content
            if(remaining_text.starts_with("</")) {
                // end tag. will be parsed after this if-else.
                break;
            } else if(remaining_text.starts_with('<')) {
                // then it's children.
                node child_node;
                // pass down namespace context: inherited + currently declared
                if(decl_xmlns.has_value() || inherited_xmlns.has_value()) {
                    child_node.inherited_xmlns = std::vector<xnamespace>();
                    if(inherited_xmlns.has_value()) {
                        child_node.inherited_xmlns->insert(child_node.inherited_xmlns->end(), inherited_xmlns->begin(), inherited_xmlns->end());
                    }
                    if(decl_xmlns.has_value()) {
                        child_node.inherited_xmlns->insert(child_node.inherited_xmlns->end(), decl_xmlns->begin(), decl_xmlns->end());
                    }
                }
                std::string rt = child_node.deserialize(remaining_text);
                
                // add to children
                if(!children.has_value()) {
                    children = std::vector<node>();
                }
                children->push_back(child_node);
                remaining_text = rt;
            } else {
                // text content
                size_t next_tag_pos = remaining_text.find('<');
                if(next_tag_pos == std::string::npos) {
                    throw parsing_error("End tag </" + full_name + "> not found");
                }

                std::string text = remaining_text.substr(0, next_tag_pos);
                text_content = unescapeTextContent(trimString(text));
                remaining_text = remaining_text.substr(next_tag_pos);
            }
        }

        // check for end tag
        std::string end_tag = "</" + full_name + ">";
        size_t end_tag_pos = remaining_text.find(end_tag);
        if(end_tag_pos == std::string::npos) {
            throw parsing_error("End tag " + end_tag + " not found");
        }

        // remove end tag from remaining text
        remaining_text = remaining_text.substr(remaining_text.find('>') + 1);
    }

    // If any more text is remaining, we simply return it.
    // In this case it may belongs to sibling nodes, which is not our concern here.
    return remaining_text;
}

std::string node::serialize(int alignDepth) const
{
    std::string result;
    
    // if any attributes or text content exists, ignore self_closing setting
    bool full_tag = (!attributes.empty() && hasTextContent() && (children.has_value() || !children->empty())) ? true : isSelfClosing();

    result = makeTab(alignDepth) + "<" + fullQualifiedName(); // by default explicitly use qualified name

    if(!full_tag) {
        // verified self-closing tag, so we are done
        result += " />";
        return result;
    }

    // attributes
    for(const auto &attr : attributes) {
        // this convertion from qualified_name adds the namespace prefix if any
        result += " " + std::string(attr.first) + "=\"" + escapeTextContent(attr.second) + "\"";
    }

    // namespace declarations
    // only emit namespaces declared on this node that are not already inherited
    auto is_declared_inherited = [&](const xnamespace& ns)->bool {
        if(!inherited_xmlns.has_value()) return false;
        for(const auto &inh : inherited_xmlns.value()) {
            // compare prefix presence/value and uri
            bool same_prefix = (!ns.prefix.has_value() && !inh.prefix.has_value()) ||
                               (ns.prefix.has_value() && inh.prefix.has_value() && ns.prefix.value() == inh.prefix.value());
            if(same_prefix && ns.uri == inh.uri) {
                return true;
            }
        }
        return false;
    };

    if(decl_xmlns.has_value()) {
        for(const auto &ns : decl_xmlns.value()) {
            if(is_declared_inherited(ns)) {
                continue; // already available from parent
            }
            if(!ns.prefix.has_value()) {
                result += " xmlns=\"" + escapeTextContent(ns.uri) + "\"";
            } else {
                result += " xmlns:" + ns.prefix.value() + "=\"" + escapeTextContent(ns.uri) + "\"";
            }
        }
    }

    // tag ending
    result += ">";

    // text content
    // Note: if both text content and children exist, text content is placed before children
    // Also, if the text is short enough, we keep it in the same line

    if(hasTextContent()) {
        std::string escaped_text = escapeTextContent(text_content.value());
        if(escaped_text.find('\n') == std::string::npos && escaped_text.length() <= 60) {
            // short text, keep in the same line
            result += escaped_text;
        } else {
            // long text or multi-line, put in new line with indentation
            result += "\n" + makeTab(alignDepth + 1) + escaped_text + "\n" + makeTab(alignDepth);
        }
    }

    // children
    if(children.has_value()) {
        result += "\n";
        for(const auto &child : children.value()) {
            result += child.serialize(alignDepth + 1) + "\n";
        }
        result += makeTab(alignDepth);
    }

    // end tag
    result += "</" + fullQualifiedName() + ">";

    // We may want to add some truncation policy to prevent making a single line too long.

    return result;
}

bool node::hasChildren() const
{
    return children.has_value() && !children->empty();
}

std::vector<node> &node::getChildNodes()
{
    return children.value();
}

const std::vector<node> &node::getChildNodes() const
{
    return children.value_or(std::vector<node>());
}

void node::addChildNode(node &&child)
{
    if(!children.has_value()) {
        children = std::vector<node>();
    }
    children->push_back(std::move(child));
}

void node::removeChildNode(size_t index)
{
    if(!children.has_value() || index >= children->size()) {
        throw parser_error("Child node index out of range");
    }
    children->erase(children->begin() + index);
}

void node::clear()
{
    text_content.reset();
    children.reset();
    attributes.clear();
    self_closing.reset();
    decl_xmlns.reset();
}

void node::reset()
{
    clear();
    name = "";
    xmlns.reset();
}

std::istream& operator>>(std::istream &is, document &doc)
{
    // read full stream into a string
    std::string xml_text((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    doc.deserialize(xml_text);
    return is;
}

std::ostream& operator<<(std::ostream &os, const document &doc)
{
    os << doc.serialize();
    return os;
}

// private helper functions
void search_and_replace(std::string &source, const std::string &from, const std::string &to)
{
    size_t pos = 0;
    while((pos = source.find(from, pos)) != std::string::npos) {
        source.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::string makeTab(int depth)
{
    return std::string(depth * tab_size, ' ');
}

std::string trimString(const std::string &str)
{
    std::string result = str;
    // trim leading spaces
    size_t start = result.find_first_not_of(" \t\n\r");;
    if(start != std::string::npos) {
        result = result.substr(start);
    } else {
        return ""; // all spaces
    }
    // trim trailing spaces
    size_t end = result.find_last_not_of(" \t\n\r");
    if(end != std::string::npos) {
        result = result.substr(0, end + 1);
    } else {
        return ""; // all spaces
    }
    return result;
}

std::string escapeTextContent(const std::string &text)
{
    std::string result = text;
    search_and_replace(result, "&", "&amp;");
    search_and_replace(result, "<", "&lt;");
    search_and_replace(result, ">", "&gt;");
    search_and_replace(result, "\"", "&quot;");
    search_and_replace(result, "'", "&apos;");
    return result;
}

std::string unescapeTextContent(const std::string &text)
{
    std::string result = text;
    search_and_replace(result, "&apos;", "'");
    search_and_replace(result, "&quot;", "\"");
    search_and_replace(result, "&gt;", ">");
    search_and_replace(result, "&lt;", "<");
    search_and_replace(result, "&amp;", "&");

    // decimal and hex character references
    size_t pos = 0;
    while((pos = result.find("&#", pos)) != std::string::npos) {
        try {
            size_t end_pos = result.find(';', pos);
            if(end_pos == std::string::npos) {
                break; // malformed, no ending ';'
            }
            std::string char_ref = result.substr(pos + 2, end_pos - (pos + 2));
            char32_t char_code = 0;
            if(!char_ref.empty() && char_ref[0] == 'x') {
                // hex
                char_code = static_cast<char32_t>(std::stoul(char_ref.substr(1), nullptr, 16));
            } else {
                // decimal
                char_code = static_cast<char32_t>(std::stoul(char_ref, nullptr, 10));
            }
            // convert char_code to UTF-8
            std::string utf8_char;
            if(char_code <= 0x7F) {
                utf8_char += static_cast<char>(char_code);
            } else if(char_code <= 0x7FF) {
                utf8_char += static_cast<char>(0xC0 | ((char_code >> 6) & 0x1F));
                utf8_char += static_cast<char>(0x80 | (char_code & 0x3F));
            } else if(char_code <= 0xFFFF) {
                utf8_char += static_cast<char>(0xE0 | ((char_code >> 12) & 0x0F));
                utf8_char += static_cast<char>(0x80 | ((char_code >> 6) & 0x3F));
                utf8_char += static_cast<char>(0x80 | (char_code & 0x3F));
            } else if(char_code <= 0x10FFFF) {
                utf8_char += static_cast<char>(0xF0 | ((char_code >> 18) & 0x07));
                utf8_char += static_cast<char>(0x80 | ((char_code >> 12) & 0x3F));
                utf8_char += static_cast<char>(0x80 | ((char_code >> 6) & 0x3F));
                utf8_char += static_cast<char>(0x80 | (char_code & 0x3F));
            } else {
                // invalid code point
                pos = end_pos + 1;
                continue;
            }
            result.replace(pos, end_pos - pos + 1, utf8_char);
            pos += utf8_char.length();
        } catch(...) {
            // If conversion fails, skip this character reference
            pos += 2;
        }
    }
    return result;
}

node &document::getRootNode()
{
    return root_node;
}

void document::setRootNode(const node &root)
{
    this->root_node = root;
}

std::optional<std::string> document::getXmlDeclaration() const
{
    return xml_declaration;
}

void document::setXmlDeclaration(const std::string &declaration)
{
    this->xml_declaration = declaration;
}

std::optional<std::string> document::getDoctypeDeclaration() const
{
    return doctype_declaration;
}

void document::setDoctypeDeclaration(const std::string &doctype)
{
    this->doctype_declaration = doctype;
}

void document::deserialize(const std::string &xml_text)
{
    reset();
    // simply deserialize the prolog, then just give the rest of the content to root node

    // For simplicity, we only look for xml declaration and doctype in the prolog
    size_t pos = 0;
    // xml declaration
    if(xml_text.compare(0, 5, "<?xml") == 0) {
        size_t end_pos = xml_text.find("?>", pos);
        if(end_pos != std::string::npos) {
            xml_declaration = xml_text.substr(0, end_pos + 2);
            pos = end_pos + 2;
        }
    }
    if(xml_text.compare(pos, 9, "<!DOCTYPE") == 0) {
        size_t end_pos = xml_text.find('>', pos);
        if(end_pos != std::string::npos) {
            doctype_declaration = xml_text.substr(pos, end_pos - pos + 1);
            pos = end_pos + 1;
        }
    }

    root_node.deserialize(xml_text.substr(pos));
}

std::string document::serialize() const
{
    // we simply serialize the root node, with optional xml declaration and doctype
    std::string result;

    // prolog
    if(xml_declaration.has_value()) {
        result += xml_declaration.value() + "\n";
    }

    if(doctype_declaration.has_value()) {
        result += doctype_declaration.value() + "\n";
    }

    // root node
    result += root_node.serialize();

    // and we're done
    return result;
}

std::vector<xnamespace> document::collectAllNamespaces() const
{
    std::vector<xnamespace> namespaces;

    // helper lambda to traverse nodes
    std::function<void(const node&)> traverse;
    traverse = [&](const node &n) {
        // collect declared namespaces
        if(n.getDeclaredNamespaces().has_value()) {
            for(const auto &ns : n.getDeclaredNamespaces().value()) {
                // check for duplicates
                auto it = std::find_if(namespaces.begin(), namespaces.end(),
                    [&](const xnamespace &existing_ns) {
                        return existing_ns.prefix == ns.prefix && existing_ns.uri == ns.uri;
                    });
                if(it == namespaces.end()) {
                    namespaces.push_back(ns);
                }
            }
        }

        // traverse children
        if(n.hasChildren()) {
            for(const auto &child : n.getChildNodes()) {
                traverse(child);
            }
        }
    };

    // start traversal from root node
    traverse(root_node);

    return namespaces;
}

void document::clear()
{
    root_node.clear();
    xml_declaration.reset();
    doctype_declaration.reset();
}

void document::reset()
{
    root_node.reset();
    xml_declaration.reset();
    doctype_declaration.reset();
}

bool xnamespace::isDefault() const
{
    return !prefix.has_value();
}

} // namespace xml
