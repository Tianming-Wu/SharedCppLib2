/*
    Ordered Map for SharedCppLib2
    Tianming Wu <https://github.com/Tianming-Wu> 2026.10.6

    A combination of map and linked list, which preserves the order inside a tree, without
    losing the insanely fast lookup speed of a map. Does not rely on standard library containers.

    Unlike std::map's "ordered" property, which is in practice sorted pairs based on the key's
    comparison operator, this implementation allows you to control the order by yourself, similar
    to a std::vector, but with O(log n) lookup speed. (In fact more like a linked list, since insertion
    is much faster than a vector)

    The iteration is by default your custom order, thus the insertion order by default. You can also
    customize the iteration order by inserting elements or removing elements.
    The std::map's legacy key order is still available as `sorted_iterator`, so you do not need to sort
    the elements yourself when you need key-based ordering.
    
    Also, reference stability is guaranteed. Inserting or removing elements will not invalidate
    references to other elements.

    It is implemented as a modified red-black tree, with additional linking information.
*/

#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace scl2 {

template<typename KeyType, typename ValueType, typename Compare = std::less<KeyType>>
class ordered_map {

public:
    using key_type        = KeyType;
    using mapped_type     = ValueType;
    using value_type      = std::pair<KeyType, ValueType>; // non-const key, unlike std::map
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare     = Compare;

private:
    // The order list needs one node without a value pair (the sentinel), so the two list links
    // live in a base class and only the real elements carry the data.
    struct link {
        link* prev = nullptr;
        link* next = nullptr;
    };

    struct node : link {
        value_type data;


        // red-black tree color, true for red, false for black
        bool red = true;

        // red-black tree pointers
        node* left = nullptr;
        node* right = nullptr;
        node* parent = nullptr;

        template<typename K, typename V>
        node(K&& k, V&& v) : data(std::forward<K>(k), std::forward<V>(v)) {}
    };

    link sentinel_;              // circular order list: .next is the first element, .prev the last
    node* root = nullptr;        // root of the red-black tree
    std::size_t size_ = 0;
    [[no_unique_address]] Compare comp_{};

public:
    template<bool IsConst> class ordered_iterator;
    template<bool IsConst> class key_iterator;

    using iterator              = ordered_iterator<false>;
    using const_iterator        = ordered_iterator<true>;
    using sorted_iterator       = key_iterator<false>;
    using sorted_const_iterator = key_iterator<true>;

    // ---------------------------------------------------------------------------------
    // Iterators. ordered_iterator walks the linked list, i.e. the insertion order;
    // key_iterator walks the tree in key order, which is what std::map iterates in.
    // ---------------------------------------------------------------------------------

    template<bool IsConst>
    class ordered_iterator {
        friend class ordered_map;
        template<bool> friend class ordered_iterator;

        using link_ptr = std::conditional_t<IsConst, const link*, link*>;
        using node_ptr = std::conditional_t<IsConst, const node*, node*>;

        link_ptr cur_ = nullptr; // pointing at the sentinel means end()

        explicit ordered_iterator(link_ptr cur) noexcept : cur_(cur) {}

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using iterator_concept  = std::bidirectional_iterator_tag;
        using value_type        = std::pair<KeyType, ValueType>;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<IsConst, const value_type*, value_type*>;
        using reference         = std::conditional_t<IsConst, const value_type&, value_type&>;

        ordered_iterator() = default;

        template<bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        ordered_iterator(const ordered_iterator<OtherConst>& other) noexcept : cur_(other.cur_) {}

        reference operator*() const { return static_cast<node_ptr>(cur_)->data; }
        pointer operator->() const { return std::addressof(static_cast<node_ptr>(cur_)->data); }

        ordered_iterator& operator++() { cur_ = cur_->next; return *this; }
        ordered_iterator operator++(int) { ordered_iterator t = *this; ++(*this); return t; }
        ordered_iterator& operator--() { cur_ = cur_->prev; return *this; }
        ordered_iterator operator--(int) { ordered_iterator t = *this; --(*this); return t; }

        template<bool OtherConst>
        bool operator==(const ordered_iterator<OtherConst>& other) const noexcept { return cur_ == other.cur_; }
        template<bool OtherConst>
        bool operator!=(const ordered_iterator<OtherConst>& other) const noexcept { return cur_ != other.cur_; }
    };

    template<bool IsConst>
    class key_iterator {
        friend class ordered_map;
        template<bool> friend class key_iterator;

        using node_ptr = std::conditional_t<IsConst, const node*, node*>;

        node_ptr cur_ = nullptr;             // nullptr means sorted_end()
        const ordered_map* owner_ = nullptr; // only needed to step back from sorted_end()

        key_iterator(node_ptr cur, const ordered_map* owner) noexcept : cur_(cur), owner_(owner) {}

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using iterator_concept  = std::bidirectional_iterator_tag;
        using value_type        = std::pair<KeyType, ValueType>;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<IsConst, const value_type*, value_type*>;
        using reference         = std::conditional_t<IsConst, const value_type&, value_type&>;

        key_iterator() = default;

        template<bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        key_iterator(const key_iterator<OtherConst>& other) noexcept
            : cur_(other.cur_), owner_(other.owner_) {}

        reference operator*() const { return cur_->data; }
        pointer operator->() const { return std::addressof(cur_->data); }

        key_iterator& operator++() { cur_ = ordered_map::successor_of(cur_); return *this; }
        key_iterator operator++(int) { key_iterator t = *this; ++(*this); return t; }
        key_iterator& operator--() {
            cur_ = cur_ ? ordered_map::predecessor_of(cur_)
                        : ordered_map::maximum_of(owner_ ? owner_->root : nullptr);
            return *this;
        }
        key_iterator operator--(int) { key_iterator t = *this; --(*this); return t; }

        template<bool OtherConst>
        bool operator==(const key_iterator<OtherConst>& other) const noexcept { return cur_ == other.cur_; }
        template<bool OtherConst>
        bool operator!=(const key_iterator<OtherConst>& other) const noexcept { return cur_ != other.cur_; }
    };

    // ---------------------------------------------------------------------------------
    // Construction / destruction / assignment
    // ---------------------------------------------------------------------------------

    ordered_map() noexcept { init_sentinel(); }

    explicit ordered_map(const Compare& comp) : comp_(comp) { init_sentinel(); }

    ordered_map(std::initializer_list<value_type> init) {
        init_sentinel();
        for (const value_type& kv : init) insert(kv.first, kv.second);
    }

    ~ordered_map() { clear(); }

    ordered_map(const ordered_map& other) {
        init_sentinel();
        comp_ = other.comp_;
        copy_from(other);
    }

    ordered_map(ordered_map&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>) {
        init_sentinel();
        steal_from(other);
    }

    ordered_map& operator=(const ordered_map& other) {
        if (this != &other) { ordered_map tmp(other); swap(tmp); }
        return *this;
    }

    ordered_map& operator=(ordered_map&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>) {
        if (this != &other) { clear(); steal_from(other); }
        return *this;
    }

    ordered_map& operator=(std::initializer_list<value_type> init) {
        clear();
        for (const value_type& kv : init) insert(kv.first, kv.second);
        return *this;
    }

    // O(1): the nodes stay where they are, only their owner changes.
    void swap(ordered_map& other) noexcept {
        if (this == &other) return;
        std::swap(root, other.root);
        std::swap(size_, other.size_);
        std::swap(comp_, other.comp_);
        std::swap(sentinel_, other.sentinel_);
        fix_list_ends();
        other.fix_list_ends();
    }

    key_compare key_comp() const { return comp_; }

    // ---------------------------------------------------------------------------------
    // Iteration: begin/end is the insertion order, sorted_begin/sorted_end the key order
    // ---------------------------------------------------------------------------------

    iterator begin() noexcept { return iterator(sentinel_.next); }
    iterator end() noexcept { return iterator(&sentinel_); }
    const_iterator begin() const noexcept { return const_iterator(sentinel_.next); }
    const_iterator end() const noexcept { return const_iterator(&sentinel_); }
    const_iterator cbegin() const noexcept { return const_iterator(sentinel_.next); }
    const_iterator cend() const noexcept { return const_iterator(&sentinel_); }

    sorted_iterator sorted_begin() noexcept { return sorted_iterator(minimum_of(root), this); }
    sorted_iterator sorted_end() noexcept { return sorted_iterator(nullptr, this); }
    sorted_const_iterator sorted_begin() const noexcept { return sorted_const_iterator(minimum_of(root), this); }
    sorted_const_iterator sorted_end() const noexcept { return sorted_const_iterator(nullptr, this); }
    sorted_const_iterator sorted_cbegin() const noexcept { return sorted_const_iterator(minimum_of(root), this); }
    sorted_const_iterator sorted_cend() const noexcept { return sorted_const_iterator(nullptr, this); }

    // ---------------------------------------------------------------------------------
    // Capacity
    // ---------------------------------------------------------------------------------

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    // ---------------------------------------------------------------------------------
    // Lookup
    // ---------------------------------------------------------------------------------

    iterator find(const KeyType& key) noexcept {
        node* n = find_impl(key);
        return n ? iterator(n) : end();
    }

    const_iterator find(const KeyType& key) const noexcept {
        const node* n = find_impl(key);
        return n ? const_iterator(n) : end();
    }

    bool contains(const KeyType& key) const noexcept { return find_impl(key) != nullptr; }
    size_type count(const KeyType& key) const noexcept { return find_impl(key) ? 1 : 0; }

    ValueType& at(const KeyType& key) {
        node* n = find_impl(key);
        if (!n) throw std::out_of_range("scl2::ordered_map::at(): key not found");
        return n->data.second;
    }

    const ValueType& at(const KeyType& key) const {
        const node* n = find_impl(key);
        if (!n) throw std::out_of_range("scl2::ordered_map::at(): key not found");
        return n->data.second;
    }

    // Inserts a value-initialized element when the key is missing (like std::map).
    ValueType& operator[](const KeyType& key) {
        node* n = find_impl(key);
        if (!n) n = insert_node(key, ValueType{}).first;
        return n->data.second;
    }

    // std::map has no const operator[]; this one cannot insert, so it throws instead.
    const ValueType& operator[](const KeyType& key) const {
        const node* n = find_impl(key);
        if (!n) throw std::out_of_range("scl2::ordered_map::operator[]: key not found");
        return n->data.second;
    }

    // ---------------------------------------------------------------------------------
    // Modifiers. None of them moves the elements that are already there.
    // ---------------------------------------------------------------------------------

    // Returns true when a new element was appended; an existing value is left alone.
    template<typename K, typename V>
    bool insert(K&& key, V&& value) {
        return insert_node(std::forward<K>(key), std::forward<V>(value)).second;
    }

    // Same as insert, kept because std::map has both names.
    template<typename K, typename V>
    bool emplace(K&& key, V&& value) {
        return insert_node(std::forward<K>(key), std::forward<V>(value)).second;
    }

    // Appends when the key is new, overwrites in place when it is not (position unchanged).
    template<typename K, typename V>
    bool insert_or_assign(K&& key, V&& value) {
        node* n = find_impl(key);
        if (n) { n->data.second = std::forward<V>(value); return false; }
        insert_node(std::forward<K>(key), std::forward<V>(value));
        return true;
    }

    bool erase(const KeyType& key) {
        node* n = find_impl(key);
        if (!n) return false;
        erase_node(n);
        return true;
    }

    // Erases the element the iterator points at and returns the next one in insertion order.
    iterator erase(iterator pos) {
        link* nxt = pos.cur_->next;
        erase_node(static_cast<node*>(pos.cur_));
        return iterator(nxt);
    }

    void clear() noexcept {
        destroy_all();
        root = nullptr;
        size_ = 0;
        init_sentinel();
    }

    // ---------------------------------------------------------------------------------
    // Diagnostics, for tests: O(n). Checks the list wiring, the tree's search order and
    // the red-black properties (root black, no red child of a red node, equal black height).
    // ---------------------------------------------------------------------------------

    bool check_invariants() const {
        std::size_t n = 0;
        for (const link* l = sentinel_.next; l != &sentinel_; l = l->next) {
            if (l->next->prev != l) return false;
            if (l->prev->next != l) return false;
            if (++n > size_) return false; // the list runs into itself
        }
        if (n != size_) return false;

        if (!root) return size_ == 0;
        if (size_ == 0) return false;
        if (root->parent || root->red) return false;

        std::size_t m = 0;
        const node* prev = nullptr;
        for (const node* x = minimum_of(root); x; x = successor_of(x)) {
            if (prev && !comp_(prev->data.first, x->data.first)) return false;
            prev = x;
            if (++m > size_) return false;
        }
        if (m != size_) return false;

        int black_height = 0;
        return check_subtree(root, nullptr, 0, black_height);
    }

private:
    // ---------------------------------------------------------------------------------
    // Order list. Circular and sentinel based, so no operation needs a null check.
    // ---------------------------------------------------------------------------------

    void init_sentinel() noexcept { sentinel_.prev = sentinel_.next = &sentinel_; }

    void fix_list_ends() noexcept {
        if (size_ == 0) { init_sentinel(); return; }
        sentinel_.next->prev = &sentinel_;
        sentinel_.prev->next = &sentinel_;
    }

    void link_back(node* n) noexcept {
        n->prev = sentinel_.prev;
        n->next = &sentinel_;
        sentinel_.prev->next = n;
        sentinel_.prev = n;
    }

    void unlink(node* n) noexcept {
        n->prev->next = n->next;
        n->next->prev = n->prev;
        n->prev = n->next = nullptr;
    }

    // Tree helpers, templates so they serve both node* and const node*.
    template<typename N> static N minimum_of(N x) noexcept {
        if (!x) return x;
        while (x->left) x = x->left;
        return x;
    }

    template<typename N> static N maximum_of(N x) noexcept {
        if (!x) return x;
        while (x->right) x = x->right;
        return x;
    }

    template<typename N> static N successor_of(N x) noexcept {
        if (x->right) return minimum_of(x->right);
        N p = x->parent;
        while (p && x == p->right) { x = p; p = p->parent; }
        return p;
    }

    template<typename N> static N predecessor_of(N x) noexcept {
        if (x->left) return maximum_of(x->left);
        N p = x->parent;
        while (p && x == p->left) { x = p; p = p->parent; }
        return p;
    }

    // The rotations only fix the tree; the order list is deliberately left alone.
    void rotate_left(node* x) noexcept {
        node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;

        y->parent = x->parent;
        if (!x->parent) root = y;
        else if (x == x->parent->left) x->parent->left = y;
        else x->parent->right = y;

        y->left = x;
        x->parent = y;
    }

    void rotate_right(node* y) noexcept {
        node* x = y->left;
        y->left = x->right;
        if (x->right) x->right->parent = y;

        x->parent = y->parent;
        if (!y->parent) root = x;
        else if (y == y->parent->left) y->parent->left = x;
        else y->parent->right = x;

        x->right = y;
        y->parent = x;
    }

    void insert_fixup(node* z) noexcept {
        while (z->parent && z->parent->red) {
            node* p = z->parent;
            node* g = p->parent; // exists: a red node is never the root

            if (p == g->left) {
                node* u = g->right; // uncle
                if (u && u->red) {
                    p->red = false;
                    u->red = false;
                    g->red = true;
                    z = g;
                } else {
                    if (z == p->right) { z = p; rotate_left(z); p = z->parent; }
                    p->red = false;
                    g->red = true;
                    rotate_right(g);
                }
            } else {
                node* u = g->left; // uncle
                if (u && u->red) {
                    p->red = false;
                    u->red = false;
                    g->red = true;
                    z = g;
                } else {
                    if (z == p->left) { z = p; rotate_right(z); p = z->parent; }
                    p->red = false;
                    g->red = true;
                    rotate_left(g);
                }
            }
        }
        if (root) root->red = false;
    }

    // CLRS's RB-DELETE-FIXUP. `x` is what took the removed node's place and may be null, in
    // which case `parent` is its parent (a null x has none). x carries one extra black until
    // the loop has pushed it up to the root.
    void delete_fixup(node* x, node* parent) noexcept {
        while (x != root && (!x || !x->red)) {
            if (x == parent->left) {
                node* w = parent->right; // sibling; not null while one black is missing
                if (w->red) {
                    w->red = false;
                    parent->red = true;
                    rotate_left(parent);
                    w = parent->right;
                }
                if ((!w->left || !w->left->red) && (!w->right || !w->right->red)) {
                    w->red = true;
                    x = parent;
                    parent = x->parent;
                } else {
                    if (!w->right || !w->right->red) {
                        if (w->left) w->left->red = false;
                        w->red = true;
                        rotate_right(w);
                        w = parent->right;
                    }
                    w->red = parent->red;
                    parent->red = false;
                    if (w->right) w->right->red = false;
                    rotate_left(parent);
                    x = root;
                    parent = nullptr;
                }
            } else {
                node* w = parent->left;
                if (w->red) {
                    w->red = false;
                    parent->red = true;
                    rotate_right(parent);
                    w = parent->left;
                }
                if ((!w->left || !w->left->red) && (!w->right || !w->right->red)) {
                    w->red = true;
                    x = parent;
                    parent = x->parent;
                } else {
                    if (!w->left || !w->left->red) {
                        if (w->right) w->right->red = false;
                        w->red = true;
                        rotate_left(w);
                        w = parent->left;
                    }
                    w->red = parent->red;
                    parent->red = false;
                    if (w->left) w->left->red = false;
                    rotate_right(parent);
                    x = root;
                    parent = nullptr;
                }
            }
        }
        if (x) x->red = false;
    }

    // Replaces the subtree rooted at u with the one rooted at v (v may be null).
    void transplant(node* u, node* v) noexcept {
        if (!u->parent) root = v;
        else if (u == u->parent->left) u->parent->left = v;
        else u->parent->right = v;
        if (v) v->parent = u->parent;
    }

    void erase_node(node* z) noexcept {
        unlink(z);

        node* y = z;              // the node that really leaves the tree
        node* x = nullptr;        // what takes its place, possibly null
        node* x_parent = nullptr; // x's parent; needed because x may be null
        bool y_red = y->red;

        if (!z->left) {
            x = z->right;
            x_parent = z->parent;
            transplant(z, z->right);
        } else if (!z->right) {
            x = z->left;
            x_parent = z->parent;
            transplant(z, z->left);
        } else {
            y = minimum_of(z->right);
            y_red = y->red;
            x = y->right;
            if (y->parent == z) {
                x_parent = y;
            } else {
                x_parent = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->red = z->red;
        }

        delete z;
        --size_;

        if (!y_red) delete_fixup(x, x_parent);
    }

    template<typename K, typename V>
    std::pair<node*, bool> insert_node(K&& key, V&& value) {
        node* parent = nullptr;
        node* cur = root;
        bool go_left = false;

        while (cur) {
            parent = cur;
            if (comp_(key, cur->data.first))      { go_left = true;  cur = cur->left; }
            else if (comp_(cur->data.first, key)) { go_left = false; cur = cur->right; }
            else return { cur, false }; // the key is already there
        }

        node* n = new node(std::forward<K>(key), std::forward<V>(value));
        n->parent = parent;
        if (!parent) root = n;
        else if (go_left) parent->left = n;
        else parent->right = n;

        link_back(n);
        ++size_;
        insert_fixup(n);
        return { n, true };
    }

    const node* find_impl(const KeyType& key) const noexcept {
        const node* cur = root;
        while (cur) {
            if (comp_(key, cur->data.first))      cur = cur->left;
            else if (comp_(cur->data.first, key)) cur = cur->right;
            else return cur;
        }
        return nullptr;
    }

    node* find_impl(const KeyType& key) noexcept {
        return const_cast<node*>(std::as_const(*this).find_impl(key));
    }

    // ---------------------------------------------------------------------------------
    // Copy / move / destruction helpers
    // ---------------------------------------------------------------------------------

    // Walks the source in list order, so the copy keeps the insertion order as well.
    void copy_from(const ordered_map& other) {
        try {
            for (const link* l = other.sentinel_.next; l != &other.sentinel_; l = l->next) {
                const node* n = static_cast<const node*>(l);
                insert_node(n->data.first, n->data.second);
            }
        } catch (...) {
            clear();
            throw;
        }
    }

    void steal_from(ordered_map& other) noexcept {
        root  = other.root;
        size_ = other.size_;
        comp_ = std::move(other.comp_);

        if (size_) {
            sentinel_.next = other.sentinel_.next;
            sentinel_.prev = other.sentinel_.prev;
            sentinel_.next->prev = &sentinel_;
            sentinel_.prev->next = &sentinel_;
        }

        other.root = nullptr;
        other.size_ = 0;
        other.init_sentinel();
    }

    // Iterative on purpose, so a deep tree cannot blow the stack.
    void destroy_all() noexcept {
        node* cur = root;
        while (cur) {
            if (cur->left) { cur = cur->left; continue; }
            if (cur->right) { cur = cur->right; continue; }

            node* parent = cur->parent;
            if (parent) {
                if (parent->left == cur) parent->left = nullptr;
                else parent->right = nullptr;
            }
            delete cur;
            cur = parent;
        }
    }

    bool check_subtree(const node* x, const node* parent, int depth, int& black_height) const {
        if (!x) { black_height = 1; return true; }
        if (depth > 512) return false; // far past any real tree: probably a cycle
        if (x->parent != parent) return false;
        if (x->red && ((x->left && x->left->red) || (x->right && x->right->red))) return false;

        int left_bh = 0;
        int right_bh = 0;
        if (!check_subtree(x->left, x, depth + 1, left_bh)) return false;
        if (!check_subtree(x->right, x, depth + 1, right_bh)) return false;
        if (left_bh != right_bh) return false;

        black_height = left_bh + (x->red ? 0 : 1);
        return true;
    }
};



template<typename KeyType, typename ValueType, typename Compare>
inline void swap(ordered_map<KeyType, ValueType, Compare>& a,
                 ordered_map<KeyType, ValueType, Compare>& b) noexcept
{
    a.swap(b);
}

} // namespace scl2
