#ifndef PHTREE_COMMON_FLAT_SPARSE_MAP_H
#define PHTREE_COMMON_FLAT_SPARSE_MAP_H

#include "bits.h"
#include <cassert>
#include <tuple>
#include <vector>

namespace improbable::phtree {

template <typename KeyT, typename ValueT>
class sparse_map {
    using Entry = std::pair<KeyT, ValueT>;
    using iterator = typename std::vector<Entry>::iterator;

  public:
    explicit sparse_map() : data_{} {
        data_.reserve(4);
    }

    [[nodiscard]] auto find(KeyT key) {
        auto it = lower_bound(key);
        if (it != data_.end() && it->first == key) {
            return it;
        }
        return data_.end();
    }

    [[nodiscard]] auto find(KeyT key) const {
        auto it = lower_bound(key);
        if (it != data_.end() && it->first == key) {
            return it;
        }
        return data_.end();
    }

    [[nodiscard]] auto lower_bound(KeyT key) {
        return std::lower_bound(data_.begin(), data_.end(), key, [](Entry& left, const KeyT key) {
            return left.first < key;
        });
    }

    [[nodiscard]] auto lower_bound(KeyT key) const {
        return std::lower_bound(
            data_.cbegin(), data_.cend(), key, [](const Entry& left, const KeyT key) {
                return left.first < key;
            });
    }

    [[nodiscard]] auto begin() {
        return data_.begin();
    }

    [[nodiscard]] auto begin() const {
        return cbegin();
    }

    [[nodiscard]] auto cbegin() const {
        return data_.cbegin();
    }

    [[nodiscard]] auto end() {
        return data_.end();
    }

    [[nodiscard]] auto end() const {
        return data_.end();
    }

    template <typename... Args>
    auto emplace(size_t key, Args&&... args) {
        auto iter = lower_bound(key);
        return try_emplace_base(iter, key, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto try_emplace(size_t key, Args&&... args) {
        auto iter = lower_bound(key);
        return try_emplace_base(iter, key, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto try_emplace(iterator iter, size_t key, Args&&... args) {
        return try_emplace_base(iter, key, std::forward<Args>(args)...).first;
    }

    void erase(KeyT key) {
        auto it = lower_bound(key);
        if (it != end() && it->first == key) {
            data_.erase(it);
        }
    }

    void erase(const iterator& iter) {
        data_.erase(iter);
    }

    [[nodiscard]] size_t size() const {
        return data_.size();
    }

  private:
    template <typename... Args>
    auto try_emplace_base(const iterator& it, KeyT key, Args&&... args) {
        if (it != end() && it->first == key) {
            return std::make_pair(it, false);
        } else {
            auto x = data_.emplace(
                it,
                std::piecewise_construct,
                std::forward_as_tuple(key),
                std::forward_as_tuple(std::forward<Args>(args)...));
            return std::make_pair(x, true);
        }
    }

    std::vector<Entry> data_;
};

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_FLAT_SPARSE_MAP_H

