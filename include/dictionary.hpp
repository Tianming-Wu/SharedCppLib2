/*
    Dictionary module for SharedCppLib2.

    What's the difference between dictionary and map?

    Well, this so-called dictionary is used to build a map.
    You can make a dictionary (a pre-map) without the values,
    and you can then generate a map with the values using a
    single command.

    It features a built-in cached map that can be generated and
    updated incrementally with the dictionary's keys, so you can
    easily use it as a cache for expensive key-value pair generation.

    It is based on a vector, but does not bahave like a vector. It
    does not allow duplicate keys.

*/

#pragma once
#include <vector>
#include <map>
#include <type_traits>
#include <functional>

#include "basics.hpp"

namespace scl2 {


template<typename _Key_Type, typename _Value_Type, typename _Compare = std::less<_Key_Type>, typename _Allocator = std::allocator<std::pair<const _Key_Type, _Value_Type>>>
requires requires(std::map<_Key_Type, _Value_Type, _Compare, _Allocator> m) {
    typename _Key_Type; typename _Value_Type;
    { m[_Key_Type{}] } -> std::convertible_to<_Value_Type>;
}
class dictionary : private std::vector<_Key_Type>
{
public:
    using key_type = _Key_Type;
    using value_type = _Value_Type;
    using map_type = std::map<key_type, value_type, _Compare, _Allocator>;

    using value_generator_type = std::function<value_type(const key_type&)>;

private:
    map_type _map;

public:
    dictionary() = default;

    ~dictionary() = default;

    enable_copy_move(dictionary)

    void __unique_insert_key(const key_type& key) {
        if (std::find(this->begin(), this->end(), key) == this->end()) {
            this->push_back(key);
        }
    }

    void append(const key_type& key) { __unique_insert_key(key); }
    void push_back(const key_type& key) { __unique_insert_key(key); }
    void insert(const key_type& key) { __unique_insert_key(key); }

    void manual_insert(const key_type& key, const value_type& value) {
        __unique_insert_key(key);
        _map[key] = value;
    }

    void erase(const key_type& key) {
        auto it = std::find(this->begin(), this->end(), key);
        if (it != this->end()) {
            this->erase(it);
        }
    }

    map_type build(value_generator_type value_generator) const {
        _map.clear();
        for (const auto& key : *this) {
            _map[key] = value_generator(key);
        }
        return _map;
    }

    map_type build_default() const {
        _map.clear();
        for (const auto& key : *this) {
            _map[key] = value_type{};
        }
        return _map;
    }

    // Only generates values for keys that are not already in the map,
    // and leaves existing key-value pairs unchanged.
    map_type additional_build(value_generator_type value_generator) const {
        auto additional_map = _map;
        for (const auto& key : *this) {
            if (additional_map.find(key) == additional_map.end()) {
                additional_map[key] = value_generator(key);
            }
        }
        return ( _map = additional_map );
    }

    // Remove key-value pairs from the map for keys that are not in the
    // dictionary, and leave existing key-value pairs unchanged.
    map_type decrease_build() const {
        auto decrease_map = _map;
        for (const auto& key : *this) {
            decrease_map.erase(key);
        }
        return ( _map = decrease_map );
    }

    map_type& map() { return _map; }
    const map_type& map() const { return _map; }

    void reset() { _map.clear(); }
};

} // namespace scl2