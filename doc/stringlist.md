# stringlist
+ Name: StringList
+ Namespace: `std`
+ Document Version: `3.22.5`


## Description
Stringlist is a class written to add more useful features to standard library (not really).

This class was inspired by [**QtProject**](https://www.qt.io) which provides powerful `QStringList` class.

### How to use:
Check [`split`](#split), [`join`](#join-1) first.


## Examples:
Stringlist is powerful on argument parsing. You can write this:
```cpp
#include <SharedCppLib2/stringlist.hpp>

int main(int argc, char** argv) {
    std::stringlist args(argc, argv);

    std::cout << args.join() << std::endl;

    return 0;
};
```

## Functions

### join (1)
Sig: `string join(const string &i = " ") const;`

Join the list into a complete string.

---
### join (2)
Sig: `string join(size_t begin, size_t size = -1, const string &i = " ") const;`

Same as [`join`](#join-1), and supports selecting range.

---
### xjoin
Sig: `string xjoin(const string &i = " ", const char binding = '\"') const;`

Same as [`join`](#join-1), and add binding character if the specific list item contains the seperator `i`. This is pretty useful if you are compiling command args.

For example:
```cpp
std::stringlist sl({"Message", "This is a sentence."});
std::cout << sl.join(" ") << std::endl;
std::cout << sl.xjoin(" ") << std::endl;
```
Output:
```
Message This is a sentence.
Message "This is a sentence."
```

---
### dbgjoin
Sig: `string dbgjoin(string i = "'") const;`

This function make it easy to see the actural boundary of each elements.

---
### append (1)
Sig: `void append(const stringlist &l);`

Wrapper of `std::vector<std::string>::append(std::vector<std::string>&)`.

---
### append (2)
Sig: `void append(const string &s);`

Wrapper of `std::vector<std::string>::append(_T&)`.

---
### subarr
Sig: `std::stringlist subarr(size_t pos, size_t len = 0) const;`

This is basically the same idea as `substr`. It allows you to slice the list.

---
### from
Sig: `void from(int size, char** content, int begin = 0, int end = -1);`

Allows you to read data from a raw char list, like argv.

You can do like this: `std::stringlist sl = std::stringlist::from(argc, argv);`, but it's better to use the [initializer version]().

---
### vat
Sig: `string vat(size_t p, const string &v = "") const;`

Return the string at position `p`, or `v` if the index `p` doesn't exist in the list.

---
### remove_empty
Sig: `void remove_empty();`

This function removes any empty strings in the list.

---
### unique
Sig: `stringlist unique();`

This function removes any duplicated strings in the list.

---
### find
Sig: `size_t find(const std::string &s, size_t start = 0) const;`

Find a specific string in the content. Returns `std::stringlist::npos` if not found.

---
### find_last
Sig: `size_t find_last(const std::string &s) const;`

Almost the same as [`find`](#find), but search from back to front.

---
### find_inside
Sig: `point find_inside(const std::string &s, size_t start = 0, size_t start_inside = 0) const;`

Find `s` in each string contained in the list until find one.

point is defined as `typedef std::pair<size_t,size_t> point;`, so get the content by `first` and `second`.

---
### contains
Sig: `bool contains(const std::string &s) const;`

---
### exec_foreach
Sig: `void exec_foreach(function<void(size_t,string&)> f);`

Execute `f()` for each of the element.

define the function as:
```cpp
void f(size_t id, std::string &content)
```
or use lambda:
```cpp
#include <functional>

std::function<void(size_t,std::string&)> f = [](size_t id, std::string &content) {
    // do something
};
```

---
### from_initializer
Sig: `static stringlist from_initializer(initializer_list<string> build);`

---
### split (1)
Sig: `static stringlist split(const string &s, const string &delim);`

**Tip**: it's suggested to use [`remove_empty`](#remove_empty) after this, because it is possible that multiple delimters together lead to empty strings. For example `split("123  123", " ")` will end up as `{"123", "", "123"}`.

---
### split (2)
Sig: `static stringlist split(const string &s, const stringlist &delims);`

Like [`split (1)`](#split-1), but supports multiple delimeters. The delimeters are `parallel`, meaning that any one of them can act as a delimeter.

Usage example:
```cpp
std::stringlist sl = std::stringlist::split(input_string, {"\n","\t","\r"," "});
```
**Tip**: it's suggested to use [`remove_empty`](#remove_empty) after this, because it is possible that multiple delimters together lead to empty strings. For example `split("123  123", " ")` will end up as `{"123", "", "123"}`.

---
### xsplit
Sig: ` static stringlist xsplit(const string &s, const string &delim, const string &begin_bind, string end_bind = "", bool remove_binding = true);`

Split the string, but support binding characters that prevent the content between them to be split.

> [!NOTE]
> Binding 'characters' means that only single-character binding is supported. This is a string working as a list of characters.

If `end_bind` is left empty, it will be the same as `begin_bind`.


---
### exsplit
Sig: `static stringlist exsplit(const string &s, const string &delim, const string &begin_bind, string end_bind = "", bool remove_binding = false, bool strict = false);`

Almost the same as [`xsplit`](#xsplit), while it supports binding characters to be found inside the string.

---
### pack
Sig: `string pack() const;`

---
### unpack
Sig: `static stringlist unpack(const std::string &s);`

---
### all_size
Sig: `size_t all_size();`

---
### (others)
Most of the `std::vector<std::string>::` functions are available if not mentioned above.


## Initializers

`explicit stringlist(int size, char** content, int begin = 0, int end = -1);`

Parse raw char list with NULL(`\0`) termating character. An example of it is `argv`.

---

`stringlist(initializer_list<string> build);`

Allows you to use initializer list.