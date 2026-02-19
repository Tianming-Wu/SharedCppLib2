/*
    Multindex module for SharedCppLib2, providing a multi-index container
    that allows efficient access to elements by multiple keys.

    The container is fully stl-compatible and supports custom index types and value types.

    This file is header-only and does not require separate compilation. Just include it where needed.
    although it uses basics.hpp, it does not require linking to basic module, as it only used some
    features in basics.hpp.

    Design:
    - Uses two-level mapping: index -> uid -> value
    - Multiple indices can map to the same value without duplicating it
    - Efficient O(log n) lookup and insertion (or O(2 log n) maybe)
*/

#pragma once

#include <map>
#include <set>
#include <stdexcept>
#include <optional>

#include "basics.hpp"

template <
    typename _Index_Type,
    typename _Value_Type,
    typename _Compare = std::less<_Index_Type>,
    typename _Allocator = std::allocator<std::pair<const _Index_Type, size_t>>,
    typename _Uid_Type = size_t
>
class multindex
{
public:
    typedef _Index_Type index_type;
    typedef _Value_Type value_type;
    typedef _Uid_Type uid_type;

    // constructors
    multindex() : _M_next_uid(0) {}

    ~multindex() = default;

    enable_copy_move(multindex)

    // Insert a value with given index
    // If the index already exists, does nothing and returns false
    // Returns true if insertion was successful
    bool insert(const index_type& index, const value_type& value) {
        // Check if index already exists
        if (_M_index_to_uid.find(index) != _M_index_to_uid.end()) {
            // If different value is being inserted for existing index, we can choose to update the value or ignore. Here we ignore.
            return false; // ignore for now.
        }

        // Allocate new UID and store value
        uid_type uid = _M_next_uid++;
        _M_uid_to_value[uid] = value;
        _M_index_to_uid[index] = uid;
        _M_uid_to_indices[uid].insert(index);

        return true;
    }

    // Bind an additional index to an existing value found by another index
    // Returns true if binding was successful, false if new_index already exists or old_index not found
    bool bind(const index_type& existing_index, const index_type& new_index) {
        // Check if new index already exists
        if (_M_index_to_uid.find(new_index) != _M_index_to_uid.end()) {
            return false;
        }

        // Find the UID for the existing index
        auto it = _M_index_to_uid.find(existing_index);
        if (it == _M_index_to_uid.end()) {
            return false;
        }

        uid_type uid = it->second;
        _M_index_to_uid[new_index] = uid;
        _M_uid_to_indices[uid].insert(new_index);

        return true;
    }

    // Find value by index, returns pointer to value or nullptr if not found
    value_type* find(const index_type& index) {
        auto it = _M_index_to_uid.find(index);
        if (it == _M_index_to_uid.end()) {
            return nullptr;
        }
        uid_type uid = it->second;
        auto vit = _M_uid_to_value.find(uid);
        if (vit == _M_uid_to_value.end()) {
            return nullptr;
        }
        return &(vit->second);
    }

    // Find value by index (const version)
    const value_type* find(const index_type& index) const {
        auto it = _M_index_to_uid.find(index);
        if (it == _M_index_to_uid.end()) {
            return nullptr;
        }
        uid_type uid = it->second;
        auto vit = _M_uid_to_value.find(uid);
        if (vit == _M_uid_to_value.end()) {
            return nullptr;
        }
        return &(vit->second);
    }

    // Check if index exists
    bool contains(const index_type& index) const {
        return _M_index_to_uid.find(index) != _M_index_to_uid.end();
    }

    // Get value by index with bounds checking
    value_type& at(const index_type& index) {
        auto it = _M_index_to_uid.find(index);
        if (it == _M_index_to_uid.end()) {
            throw std::out_of_range("multindex::at: index not found");
        }
        uid_type uid = it->second;
        return _M_uid_to_value.at(uid);
    }

    // Get value by index with bounds checking (const version)
    const value_type& at(const index_type& index) const {
        auto it = _M_index_to_uid.find(index);
        if (it == _M_index_to_uid.end()) {
            throw std::out_of_range("multindex::at: index not found");
        }
        uid_type uid = it->second;
        return _M_uid_to_value.at(uid);
    }

    // Erase by index
    // If this was the last index pointing to the value, the value is also removed
    // Returns true if the index was found and removed
    bool erase(const index_type& index) {
        auto it = _M_index_to_uid.find(index);
        if (it == _M_index_to_uid.end()) {
            return false;
        }

        uid_type uid = it->second;
        _M_index_to_uid.erase(it);

        // Remove index from uid's index set
        auto& indices = _M_uid_to_indices[uid];
        indices.erase(index);

        // If no more indices point to this uid, remove the value
        if (indices.empty()) {
            _M_uid_to_indices.erase(uid);
            _M_uid_to_value.erase(uid);
        }

        return true;
    }

    // Get all indices that point to the same value as the given index
    std::set<index_type> get_all_indices(const index_type& index) const {
        auto it = _M_index_to_uid.find(index);
        if (it == _M_index_to_uid.end()) {
            return {};
        }
        uid_type uid = it->second;
        auto uit = _M_uid_to_indices.find(uid);
        if (uit == _M_uid_to_indices.end()) {
            return {};
        }
        return uit->second;
    }

    // Clear all data
    void clear() {
        _M_index_to_uid.clear();
        _M_uid_to_value.clear();
        _M_uid_to_indices.clear();
        _M_next_uid = 0;
    }

    // Get number of unique values stored
    size_t value_count() const {
        return _M_uid_to_value.size();
    }

    // Get number of indices
    size_t index_count() const {
        return _M_index_to_uid.size();
    }

    // Check if empty
    bool empty() const {
        return _M_index_to_uid.empty();
    }

    // Constructor from initializer list.
    // Format: {{{"index1", "index2"}, value1}, {{"index3"}, value2}, ... }
    // We feel free to THROW AN EXCEPTION if user gave any ILLEAGAL inputs.

    multindex(std::initializer_list<std::pair<std::initializer_list<index_type>, value_type>> init)
        : _M_next_uid(0)
    {
        for (const auto& item : init) {
            const auto& indices = item.first;
            const auto& value = item.second;

            if (indices.size() == 0) {
                continue; // skip empty index set
            }

            // Insert the first index with the value
            auto it = indices.begin();
            if (!insert(*it, value)) {
                throw std::invalid_argument("Duplicate index in initializer list");
            }

            // Bind the rest of the indices to the same value
            for (++it; it != indices.end(); ++it) {
                if (!bind(*(indices.begin()), *it)) {
                    throw std::invalid_argument("Duplicate index in initializer list");
                }
            }
        }
    }

    // reference access operator, for convenience. Behaves like map's operator[] but with multi-index support.
    // If index doesn't exist, a new value is default-constructed and returned.

    value_type& operator[](const index_type& index) {
        auto it = _M_index_to_uid.find(index);
        if (it != _M_index_to_uid.end()) {
            uid_type uid = it->second;
            return _M_uid_to_value[uid];
        } else {
            // Insert new default value and return reference to it
            uid_type uid = _M_next_uid++;
            _M_uid_to_value[uid] = value_type(); // default construct value
            _M_index_to_uid[index] = uid;
            _M_uid_to_indices[uid].insert(index);
            return _M_uid_to_value[uid];
        }
    }

private:
    // Next available UID
    uid_type _M_next_uid;

    // Index to UID mapping (multiple indices can map to same UID)
    std::map<index_type, uid_type, _Compare, _Allocator> _M_index_to_uid;

    // UID to value mapping (one UID maps to one value)
    // _Compare and _Allocator are for index_type, so we should not use them here.
    std::map<uid_type, value_type> _M_uid_to_value;

    // UID to indices mapping (for tracking which indices point to each value)
    // The outer map uses uid_type as key (uses default comparator for uid_type)
    // The inner set stores index_type and uses _Compare
    std::map<uid_type, std::set<index_type, _Compare>> _M_uid_to_indices;
};