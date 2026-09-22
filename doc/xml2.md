# XML2

`xml2` is a **lossless-capable** XML parser and serializer: you can read a file, change
one value, and write it back **without reformatting the rest of the file** — comments,
blank lines, indentation, attribute order, quote style and entity spelling all survive.

It is the intended replacement for the older `xml` module. The two live side by side for
now (`xml2.hpp` / `xml2.cpp`).

* header: `xml2.hpp` &nbsp;·&nbsp; source: `xml2.cpp`
* standalone module (`[SCL_STANDALONE_MODULE]`), standard library only, C++17 and newer
* namespace: `scl2::xml2`

---

## Quick start

```cpp
#include <xml2.hpp>

namespace x = scl2::xml2;

const std::string text = R"(<library>
    <book id="1"   kind='paper'>
        <title>Modern C++</title>
    </book>
</library>)";

x::document doc = x::document::parse(text);

doc.get("library/book[1]/@id");      // "1"
doc.get("library/book[1]/title");    // "Modern C++"

doc.set("library/book[1]/@id", "2"); // change one value

std::string out = doc.serialize();   // identical to `text`, except id="2"
```

The whole point is the last line: everything except the edited attribute is byte-for-byte
the original text.

---

## Why not `std::map`-style trees

The old `xml` module (and most naive XML libraries) store attributes in a `std::map`,
text in a single `optional<string>`, and re-indent on output. That design makes lossless
editing impossible:

| stored as | what is lost |
|---|---|
| `std::map<name, value>` | attribute **order**, and any whitespace around attributes |
| one `text` string per element | **mixed content** (`<p>a<b>c</b>d</p>`) cannot be represented |
| decoded text only | `&amp;` vs `&#38;` vs CDATA spelling is unrecoverable |
| re-indentation on write | comments' position, blank lines, user's wrapping |

`xml2` instead keeps children as an **ordered, complete decomposition of the source**:
elements, text (including pure whitespace), comments and processing instructions are all
children, in document order. Serialization is then just recursive concatenation, which is
lossless by construction.

---

## Reading

### `document`

| member | description |
|---|---|
| `static document parse(std::string text, fidelity f = fidelity::raw)` | parse |
| `serialize()` | produce text (lossless for untouched parts) |
| `root()` | the root `node` |
| `prolog()` / `epilog()` | raw text before / after the root element (declaration, DOCTYPE, comments, trailing newlines) |
| `select` / `find` / `get` / `set` | path queries (see below) |

### `node`

| member | description |
|---|---|
| `is_element()` `is_text()` `is_comment()` `is_pi()` | node kind |
| `name()` | element name, including any prefix (`x:item` → `"x:item"`) |
| `value()` | text: for a text node its own text; for an element, its first direct text child |
| `attr(key, def)` / `has_attr(key)` | decoded attribute value |
| `attributes()` | `const std::vector<attribute>*` — source order, each entry has `name`, `value`, `raw_value` |
| `children()` | `std::vector<node>*` — every child, in document order |
| `child(name)` / `child_at(name, i)` | first / i-th (0-based) child element with that name |
| `select(path)` / `find(path)` | path query relative to this node |

Note that `value()` gives the **first** text child; for mixed content iterate `children()`
and check `is_text()` / `is_element()`.

---

## Path queries

Paths are a practical subset of XPath — enough for configuration and document work,
without the full grammar.

### Syntax

| syntax | meaning |
|---|---|
| `a/b` | child steps |
| `//a` | any descendant at any depth |
| `a//b` | `b` anywhere below `a` |
| `*` | any element name |
| `a[2]` | the **2nd** matching element (1-based) |
| `a[@id]` | has an `id` attribute |
| `a[@id=v]` | `id` attribute equals `v` |
| `a[@id*=v]` | `id` attribute **contains** `v` (fuzzy match) |
| `a/@id` | take an attribute value instead of the element |
| `a:b` | prefix matched **literally** (no namespace URI resolution) |

### Rules

* The first step may name the root element (`library/book`) **or** start at its children
  (`book`) — both work.
* `//` must come directly before an element name.
* `@attr` is only allowed at the end. It produces an *attribute match*: `value()` returns
  the attribute value, `set_value()` writes it back.
* `[n]` is 1-based and counts **within each parent** (XPath semantics):
  `library/book[1]` returns the first `book` of *every* `library`.
* Filters and indexes can be chained: `book[@id][2]`, `book[@id*=a][1]`.
* Results are returned in document order.
* `select()` returns **all** matches; `find()`, `get()` and `set()` only use the **first**.

### Examples

Using this document:

```xml
<library>
    <book id="1" kind="paper"><title>Modern C++</title></book>
    <book id="2"><title>Compressed</title></book>
    <empty/>
</library>
```

```cpp
doc.select("library/book").size();          // 2
doc.select("library/book[2]").size();       // 1
doc.get("library/book[1]/@id");             // "1"
doc.get("library/book[@id=2]/title");       // "Compressed"
doc.get("library/book[@kind=paper]/@id");   // "1"
doc.get("library/book[@kind*=pap]/@id");    // "1"   (contains)
doc.get("//title[1]");                      // "Modern C++"  (any depth)
doc.select("library/*").size();             // 2     (book, book — * skips text/comments)
doc.select("library/book[@id]").size();     // 2
doc.get("library/nope", "fallback");        // "fallback" (no match → default)
```

Iterate every match:

```cpp
for (const auto& m : doc.select("library/book")) {
    std::string id = m.target->attr("id");
}
```

### Writing through a path

```cpp
doc.set("library/book[1]/@id", "9");   // rewrites only that attribute value
doc.set("library/book[1]/title", "New");
```

`set()` returns `false` when nothing matched. If the target element has no direct text
child, a new text node is appended.

### Structural edits

```cpp
x::document d = x::document::parse("<root>\n    <a/>\n    <c/>\n</root>");

d.find("root/c").insert_before(make_element("b"));   // insert <b/> before <c/>
d.find("root/a").insert_after(make_element("b"));    // insert after <a/>
d.find("root/c").remove();                           // drop <c/> and its indentation
d.find("root/@id").remove();                         // drop that attribute
```

* `insert_before` / `insert_after` / `remove` need the match to come from `select()` or
  `find()` (they use the recorded parent). The root element has no parent and no siblings,
  so those calls return `false` for it — but `@attr` removal still works on the root.
* Indentation is **inferred locally**; the document is never re-laid out:

  | parent layout | result |
  |---|---|
  | multi-line (`<root>\n    <a/>\n</root>`) | the new element reuses its siblings' indentation |
  | compact (`<root><a/></root>`) | stays compact — nothing gets unfolded |
  | mixed content (`<p>a<b/>c</p>`) | no whitespace is invented |

* Removing an element also removes the whitespace that led to it, so no blank line is left
  behind; if the parent ends up without element children, the leftover whitespace is
  cleared too.
* `append_child` is exactly “insert at the end”.

---

## Fidelity

`parse()` takes a fidelity mode:

| mode | what is kept | typical use |
|---|---|---|
| `fidelity::raw` (default) | original text of every tag, attribute value and text node, plus prolog/epilog | edit a file and write it back with everything else untouched |
| `fidelity::semantic` | values and structure only: no raw tags, whitespace-only text dropped, prolog reduced to the XML declaration, epilog dropped | read data out of large files, or build output from scratch |

`semantic` still keeps attribute **order**, comments, processing instructions and every
value — it just throws away the layout. `serialize()` then regenerates compact output
instead of copying original text:

```cpp
// <?xml version="1.0"?>\n<root  a="1"  b='2'>\n    <c>  hi  </c>\n</root>\n
const std::string text = load("big.xml");

x::document big = x::document::parse(text, x::fidelity::semantic);
big.get("root/@b");                    // "2"
big.serialize();                       // <?xml version="1.0"?><root a="1" b="2"><c>  hi  </c></root>
```

So the trade is memory and formatting against fidelity — use `raw` unless you only need
the data.

## What a modification regenerates

Because children are a complete decomposition of the source, serialization copies raw
text for everything that was not touched:

| operation | effect on output |
|---|---|
| (no modification) | **byte-identical** to the input |
| `set_attr` on an existing attribute | only that attribute value is rewritten; other attributes keep their whitespace, order and quote style |
| `set_attr` for a new attribute | appended at the end, reusing the existing gap whitespace |
| `set_text` | only that text node is rewritten (entities escaped) |
| `append_child` / `insert_child` | inserted with the indentation inferred from its siblings (multi-line parents); a compact parent stays compact |
| `remove_child` / `remove_attr` | removed together with its own leading whitespace; everything else is untouched |

An element is regenerated **only** when it is marked dirty (its attributes changed); its
children and its end tag are still copied verbatim.

---

## Limits / roadmap

* Indentation inference is local only — new nodes follow their siblings' style, but
  existing nodes are never re-laid out (there is no pretty-printer).
* No predicate logic (`and`/`or`), no `text()`, no positional `last()`.
* No namespace URI resolution — prefixes are matched literally.
* No DTD / entity definition processing; unknown entities are left as-is.

---

## Standalone usage

Inside a SharedCppLib2 build it is registered as the `xml2` target:

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(app SharedCppLib2::xml2)
```

Or copy `xml2.hpp` and `xml2.cpp` into your project — they only need the standard library:

```cmake
add_executable(app main.cpp xml2.cpp)
```

The module is marked `cpp_generation: cxx17 - cxx23`, so it also builds with older
standards (unlike the rest of SharedCppLib2, which targets C++23).
