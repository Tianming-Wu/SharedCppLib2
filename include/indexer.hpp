#pragma once

#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>

// 索引器类
template<typename Container, typename KeyType, typename Compare = std::less<KeyType>>
class Indexer {
public:
    using ValueType = typename Container::value_type;
    using KeyExtractor = std::function<KeyType(const ValueType&)>;
    using MapType = std::multimap<KeyType, ValueType*, Compare>;
    using Iterator = typename MapType::iterator;

    // 构造函数：需要容器引用和键提取函数
    Indexer(Container& container, KeyExtractor key_extractor, Compare comp = Compare())
        : container_(container), key_extractor_(key_extractor), index_map_(comp) {
        rebuild();
    }

    // 重新构建索引（全量更新）
    void rebuild() {
        index_map_.clear();
        for (auto& item : container_) {
            index_map_.emplace(key_extractor_(item), &item);
        }
    }

    // 插入单个项目到索引
    void insert(ValueType& value) {
        index_map_.emplace(key_extractor_(value), &value);
    }

    // 删除单个项目从索引
    void erase(ValueType& value) {
        KeyType key = key_extractor_(value);
        auto range = index_map_.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == &value) {
                index_map_.erase(it);
                break;
            }
        }
    }

    // 搜索单个键（返回第一个匹配项）
    ValueType* search(const KeyType& key) {
        auto it = index_map_.find(key);
        if (it != index_map_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // 搜索所有匹配键（支持别名）
    std::vector<ValueType*> search_all(const KeyType& key) {
        std::vector<ValueType*> results;
        auto range = index_map_.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            results.push_back(it->second);
        }
        return results;
    }

    // 重新平衡索引（增量更新）
    void rebalance() {
        // 检查容器中是否有被移动的项目
        // 这里可以实现更复杂的增量更新逻辑
        // 当前实现简单重建，可根据需要优化
        rebuild();
    }

    // 范围查询
    template<typename Predicate>
    std::vector<ValueType*> range_query(const KeyType& lower, const KeyType& upper, Predicate pred) {
        std::vector<ValueType*> results;
        auto lower_it = index_map_.lower_bound(lower);
        auto upper_it = index_map_.upper_bound(upper);
        
        for (auto it = lower_it; it != upper_it; ++it) {
            if (pred(*(it->second))) {
                results.push_back(it->second);
            }
        }
        return results;
    }

private:
    Container& container_;
    KeyExtractor key_extractor_;
    MapType index_map_;
};