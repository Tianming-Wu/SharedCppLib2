/*
    Undo / redo stacks for SharedCppLib2.

    undostack<T> keeps whole states by value. Push a state and the stack remembers it; undo walks
    a cursor back through the states already pushed.

    commandstack keeps actions instead. Each action runs itself forwards and backwards and
    captures whatever it needs, usually a reference to the object it edits, so nothing has to be
    copyable and one stack can drive several targets. A macro groups several pushes into one step,
    and a merge id folds a push into the step below it.

    One undo is one step on both.

    Usage Example:
        scl2::undostack<std::string> text;
        text.push("hello");
        text.push("hello world");
        text.undo();                    // true
        text.current();                 // "hello"

        scl2::commandstack ur;
        ur.push("rotate +90",
                [&scene, id]{ scene.rotate(id, +90); },
                [&scene, id]{ scene.rotate(id, -90); });
        ur.undo();                      // scene.rotate(id, -90)

    See doc/undostack.md for the rest.

    [SCL_STANDALONE_MODULE]
    version: 1.0.0
    cpp_generation: cxx17 - cxx23
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace scl2 {

// ---------------------------------------------------------------------------------------------
// undostack - whole states, by value
// ---------------------------------------------------------------------------------------------

// A history of whole states, kept by value. T only has to be copy constructible.
template<typename T>
class undostack {
    static_assert(std::is_copy_constructible_v<T>,
        "undostack<T>: T is stored by value, so it must be copy constructible.");
public:
    using size_type = std::size_t;

    undostack() = default;

    // Records a state and makes it current. Anything that was undone is dropped: a redo chain
    // only lives until the next push.
    void push(const T& state) { push_impl(state); }
    void push(T&& state) { push_impl(std::move(state)); }

    // Steps the cursor one state back. False once the oldest state is current.
    bool undo() {
        if (m_index == 0) return false;
        --m_index;
        return true;
    }

    // Steps the cursor one state forward. False once the newest state is current.
    bool redo() {
        if (m_index >= m_stack.size()) return false;
        ++m_index;
        return true;
    }

    // The state the cursor is on. Throws when nothing is current: the stack is empty, or undo
    // has walked back past the first state.
    const T& current() const {
        if (m_index == 0)
            throw std::logic_error("undostack: no current state");
        return m_stack[m_index - 1];
    }

    // The state at a position in the history, 0 being the oldest.
    const T& at(size_type position) const {
        if (position >= m_stack.size())
            throw std::out_of_range("undostack: no state at that history position");
        return m_stack[position];
    }

    void clear() {
        m_stack.clear();
        m_index = 0;
    }

    bool empty() const { return m_stack.empty(); }

    // States held, and how many of them are currently applied. The applied count is the
    // meaningful one: it is 0 when everything has been undone, size() when nothing has.
    size_type size() const { return m_stack.size(); }
    size_type index() const { return m_index; }

    bool can_undo() const { return m_index > 0; }
    bool can_redo() const { return m_index < m_stack.size(); }

    // Oldest states are dropped once the history passes the limit, so the stack cannot grow
    // without bound. 0 means no limit, which is the default.
    void set_limit(size_type limit) {
        m_limit = limit;
        trim();
    }
    size_type limit() const { return m_limit; }

private:
    template<typename U>
    void push_impl(U&& state) {
        while (m_stack.size() > m_index)
            m_stack.pop_back(); // drop the redo chain
        m_stack.push_back(std::forward<U>(state));
        ++m_index;
        trim();
    }

    void trim() {
        if (m_limit == 0) return;
        while (m_stack.size() > m_limit) {
            m_stack.pop_front();
            if (m_index > 0) --m_index;
        }
    }

    std::deque<T> m_stack;
    size_type m_index = 0; // states applied, always within [0, size]
    size_type m_limit = 0; // 0: unlimited
};

// ---------------------------------------------------------------------------------------------
// commandstack - actions, run forwards and backwards
// ---------------------------------------------------------------------------------------------

// Which way an action is being run. Handed to an action that handles both directions itself.
enum class undo_dir : std::uint8_t { Undo, Redo };

class commandstack {
public:
    using size_type = std::size_t;
    using action = std::function<void(undo_dir)>;

    // Merge id that never merges.
    static constexpr int no_merge = -1;

    commandstack() = default;
    explicit commandstack(size_type limit) { set_limit(limit); }

    // ---------------------------------------------------------------------------- pushing

    // One action, told which way it is running.
    void push(std::string_view label, action apply) {
        push_impl(label, no_merge, std::move(apply));
    }

    // Two actions, one per direction.
    void push(std::string_view label, std::function<void()> redo, std::function<void()> undo) {
        push_impl(label, no_merge, pair_action(std::move(redo), std::move(undo)));
    }

    // The same two forms, folding into the step below when that step carries the same merge id.
    // Folding replaces that step, so the action has to redo and undo the whole folded run.
    void push(std::string_view label, int merge_id, action apply) {
        push_impl(label, merge_id, std::move(apply));
    }
    void push(std::string_view label, int merge_id, std::function<void()> redo, std::function<void()> undo) {
        push_impl(label, merge_id, pair_action(std::move(redo), std::move(undo)));
    }

    // --------------------------------------------------------------------------- stepping

    // Runs the action of the step below the cursor. False when there is none.
    bool undo() {
        require_no_macro("undo");
        if (m_index == 0) return false;
        m_steps[m_index - 1].run_undo();
        --m_index;
        return true;
    }

    // Runs the action of the step on the cursor. False when there is none.
    bool redo() {
        require_no_macro("redo");
        if (m_index >= m_steps.size()) return false;
        m_steps[m_index].run_redo();
        ++m_index;
        return true;
    }

    bool can_undo() const { return m_open.empty() && m_index > 0; }
    bool can_redo() const { return m_open.empty() && m_index < m_steps.size(); }

    // ------------------------------------------------------------------------------ query

    // Steps held, and how many of them are applied. A macro is one step however many actions
    // went into it.
    size_type size() const { return m_steps.size(); }
    size_type index() const { return m_index; }
    bool empty() const { return m_steps.empty(); }

    // What the next undo would take back, and what the next redo would put back. Empty when
    // there is nothing to do.
    std::string_view undo_label() const {
        return m_index > 0 ? std::string_view(m_steps[m_index - 1].label) : std::string_view{};
    }
    std::string_view redo_label() const {
        return m_index < m_steps.size() ? std::string_view(m_steps[m_index].label) : std::string_view{};
    }

    // ------------------------------------------------------------------------------ macro

    // Groups every push made until macro_end into one step. Macros may nest, and the outermost
    // one is the step. Nothing can be undone or redone while one is open.
    void macro_begin(std::string_view label) {
        if (m_open.empty() && m_index < m_steps.size())
            discard_redo_chain();
        entry* parent = resolve_open();
        auto& container = parent ? parent->children : m_steps;
        entry node;
        node.label.assign(label.begin(), label.end());
        container.push_back(std::move(node));
        m_open.push_back(container.size() - 1);
    }

    // Closes the innermost open macro. A macro that recorded nothing is dropped.
    void macro_end() {
        if (m_open.empty())
            throw std::logic_error("commandstack: macro_end without a matching macro_begin");

        entry* parent = nullptr;
        if (m_open.size() > 1) {
            parent = &m_steps[m_open.front()];
            for (size_type i = 1; i + 1 < m_open.size(); ++i)
                parent = &parent->children[m_open[i]];
        }
        auto& container = parent ? parent->children : m_steps;
        const bool top_level = (m_open.size() == 1);
        const size_type position = m_open.back();
        m_open.pop_back();

        if (container[position].children.empty()) {
            container.pop_back(); // nothing was recorded, so there is no step to keep
        } else if (top_level) {
            ++m_index;
            trim();
        }
    }

    // ------------------------------------------------------------------------ clean state

    // Marks the current position as clean, which is what a window asks before it lets itself
    // be closed: is_clean() is "there are no unsaved edits". The mark is dropped once the step
    // it points at is discarded by a new push or by the limit.
    void set_clean() {
        m_clean_valid = true;
        m_clean_index = m_index;
    }
    bool is_clean() const { return m_clean_valid && m_index == m_clean_index; }

    // --------------------------------------------------------------------------- house

    // Oldest steps are dropped once the history passes the limit. 0 means no limit, the default.
    void set_limit(size_type limit) {
        m_limit = limit;
        trim();
    }
    size_type limit() const { return m_limit; }

    void clear() {
        m_steps.clear();
        m_open.clear();
        m_index = 0;
        m_clean_valid = false;
    }

private:
    // A step. Either an action, or a macro holding the actions it grouped.
    struct entry {
        std::string label;
        int merge_id = no_merge;
        action apply;
        std::vector<entry> children;

        bool is_macro() const { return !children.empty(); }

        void run_redo() {
            if (is_macro())
                for (entry& child : children) child.run_redo();
            else if (apply)
                apply(undo_dir::Redo);
        }

        void run_undo() {
            if (is_macro())
                for (auto child = children.rbegin(); child != children.rend(); ++child)
                    child->run_undo();
            else if (apply)
                apply(undo_dir::Undo);
        }
    };

    static action pair_action(std::function<void()> redo, std::function<void()> undo) {
        if (!redo || !undo)
            throw std::invalid_argument("commandstack: push needs both a redo and an undo action");
        return [redo = std::move(redo), undo = std::move(undo)](undo_dir direction) {
            if (direction == undo_dir::Redo) redo();
            else undo();
        };
    }

    void push_impl(std::string_view label, int merge_id, action apply) {
        if (!apply)
            throw std::invalid_argument("commandstack: push needs a non-empty action");

        // Folding is only for the step directly below the cursor, and never across a macro.
        if (m_open.empty() && merge_id >= 0 && m_index == m_steps.size() && !m_steps.empty()) {
            entry& top = m_steps.back();
            if (!top.is_macro() && top.merge_id == merge_id) {
                top.label.assign(label.begin(), label.end());
                top.apply = std::move(apply);
                top.apply(undo_dir::Redo);
                return;
            }
        }

        if (m_open.empty() && m_index < m_steps.size())
            discard_redo_chain();

        entry& step = append(label, merge_id, std::move(apply));
        if (m_open.empty()) ++m_index;
        step.apply(undo_dir::Redo);
        if (m_open.empty()) trim();
    }

    // Appends a step, into the innermost open macro when there is one.
    entry& append(std::string_view label, int merge_id, action apply) {
        entry* parent = resolve_open();
        auto& container = parent ? parent->children : m_steps;
        entry step;
        step.label.assign(label.begin(), label.end());
        step.merge_id = merge_id;
        step.apply = std::move(apply);
        container.push_back(std::move(step));
        return container.back();
    }

    // The innermost open macro, or null when none is open. Paths of positions are kept instead
    // of pointers because pushing into a container moves what lives in it.
    entry* resolve_open() {
        if (m_open.empty()) return nullptr;
        entry* node = &m_steps[m_open.front()];
        for (size_type i = 1; i < m_open.size(); ++i)
            node = &node->children[m_open[i]];
        return node;
    }

    void discard_redo_chain() {
        m_steps.resize(m_index);
        if (m_clean_valid && m_clean_index > m_index)
            m_clean_valid = false;
    }

    void trim() {
        if (!m_open.empty() || m_limit == 0 || m_steps.size() <= m_limit) return;
        const size_type dropped = m_steps.size() - m_limit;
        m_steps.erase(m_steps.begin(), m_steps.begin() + static_cast<std::ptrdiff_t>(dropped));
        m_index = (m_index > dropped) ? m_index - dropped : 0;
        if (m_clean_valid) {
            if (m_clean_index < dropped) m_clean_valid = false;
            else m_clean_index -= dropped;
        }
    }

    void require_no_macro(const char* what) const {
        if (!m_open.empty())
            throw std::logic_error(std::string("commandstack: ") + what + " inside an open macro");
    }

    std::vector<entry> m_steps;
    std::vector<size_type> m_open; // path to the innermost open macro
    size_type m_index = 0;         // steps applied, always within [0, size]
    size_type m_limit = 0;         // 0: unlimited
    size_type m_clean_index = 0;
    bool m_clean_valid = false;
};

} // namespace scl2