/*
    All the xsort is a combinition of different sorting algorithms.
*/
#pragma once
#include <functional>
#include <iterator>
#include <concepts>
#include <utility>

namespace atxsort {

// 比较函数 concept
template<typename T>
concept CompareFn = requires(const T& a, const T& b) {
    { a < b } -> std::convertible_to<bool>;
};

// 迭代器 concept
template<typename Iter>
concept SortableIterator = requires(Iter it) {
    typename std::iterator_traits<Iter>::value_type;
    { ++it } -> std::same_as<Iter&>;
    { *it };
};

// 默认比较函数
template<typename T>
bool default_compare(const T& a, const T& b) { return a < b; }


// 通用 quick_sort
template<SortableIterator Iter, typename Compare = decltype(default_compare<typename std::iterator_traits<Iter>::value_type>)>
void quick_sort(Iter begin, Iter end, Compare comp = default_compare<typename std::iterator_traits<Iter>::value_type>) {
    if (begin == end) return;
    Iter left = begin, right = end;
    --right;
    auto pivot = *left;
    while (left < right) {
        while (left < right && !comp(*right, pivot)) --right;
        if (left < right) *left = *right, ++left;
        while (left < right && comp(*left, pivot)) ++left;
        if (left < right) *right = *left, --right;
    }
    *left = pivot;
    quick_sort(begin, left, comp);
    quick_sort(left + 1, end, comp);
}

// 通用 merge_sort
template<SortableIterator Iter, typename Compare = decltype(default_compare<typename std::iterator_traits<Iter>::value_type>)>
void merge_sort(Iter begin, Iter end, Compare comp = default_compare<typename std::iterator_traits<Iter>::value_type>) {
    auto len = std::distance(begin, end);
    if (len <= 1) return;
    Iter mid = begin;
    std::advance(mid, len / 2);
    merge_sort(begin, mid, comp);
    merge_sort(mid, end, comp);
    using T = typename std::iterator_traits<Iter>::value_type;
    std::vector<T> temp;
    temp.reserve(len);
    Iter left = begin, right = mid;
    while (left != mid && right != end) {
        if (comp(*right, *left)) {
            temp.push_back(*right);
            ++right;
        } else {
            temp.push_back(*left);
            ++left;
        }
    }
    while (left != mid) {
        temp.push_back(*left);
        ++left;
    }
    while (right != end) {
        temp.push_back(*right);
        ++right;
    }
    std::move(temp.begin(), temp.end(), begin);
}

// 通用 LSD radix_sort（仅限整数类型）
template<SortableIterator Iter>
requires std::is_integral_v<typename std::iterator_traits<Iter>::value_type>
void radix_sort(Iter begin, Iter end) {
    using T = typename std::iterator_traits<Iter>::value_type;
    constexpr size_t bits = sizeof(T) * 8;
    std::vector<Iter> buckets[2];
    size_t len = std::distance(begin, end);
    if (len <= 1) return;
    std::vector<T> temp(len);
    for (size_t bit = 0; bit < bits; ++bit) {
        size_t idx = 0;
        for (Iter it = begin; it != end; ++it) {
            if (((*it) >> bit) & 1)
                temp[idx++] = *it;
        }
        for (Iter it = begin; it != end; ++it) {
            if (!(((*it) >> bit) & 1))
                temp[idx++] = *it;
        }
        std::move(temp.begin(), temp.end(), begin);
    }
}

} // namespace atxsort