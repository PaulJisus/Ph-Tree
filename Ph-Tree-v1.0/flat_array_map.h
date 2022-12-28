#ifndef PHTREE_COMMON_FLAT_ARRAY_MAP_H
#define PHTREE_COMMON_FLAT_ARRAY_MAP_H

#include "bits.h"
#include <bitset>
#include <cassert>
#include <tuple>

namespace improbable::phtree {

template <typename T, std::size_t SIZE>
class flat_array_map;

namespace detail {

template <typename T>
using flat_map_pair = std::pair<size_t, T>;

template <typename T, std::size_t SIZE>
class flat_map_iterator {
    friend flat_array_map<T, SIZE>;

  public:
    flat_map_iterator() : first{0}, map_{nullptr} {};

    explicit flat_map_iterator(size_t index, const flat_array_map<T, SIZE>* map)
    : first{index}, map_{map} {
        assert(index <= SIZE);
    }

    auto& operator*() const {
        assert(first < SIZE && map_->occupied(first));
        return const_cast<flat_map_pair<T>&>(map_->data(first));
    }

    auto* operator->() const {
        assert(first < SIZE && map_->occupied(first));
        return const_cast<flat_map_pair<T>*>(&map_->data(first));
    }

    auto& operator++() {
        first = (first + 1) >= SIZE ? SIZE : map_->lower_bound_index(first + 1);
        return *this;
    }

    auto operator++(int) {
        flat_map_iterator it(first, map_);
        ++(*this);
        return it;
    }

    friend bool operator==(const flat_map_iterator& left, const flat_map_iterator& right) {
        return left.first == right.first;
    }

    friend bool operator!=(const flat_map_iterator& left, const flat_map_iterator& right) {
        return left.first != right.first;
    }

  private:
    size_t first;
    const flat_array_map<T, SIZE>* map_;
};
}
template <typename T, std::size_t SIZE>
class flat_array_map {
    using map_pair = detail::flat_map_pair<T>;
    using iterator = detail::flat_map_iterator<T, SIZE>;
    friend iterator;

  public:
    [[nodiscard]] auto find(size_t index) noexcept {
        return iterator{occupied(index) ? index : SIZE, this};
    }

    [[nodiscard]] auto lower_bound(size_t index) const {
          return iterator{lower_bound_index(index), this};
    }

    [[nodiscard]] auto begin() const {
          return iterator{lower_bound_index(0), this};
    }

    [[nodiscard]] auto cbegin() const {
          return iterator{lower_bound_index(0), this};
    }

    [[nodiscard]] auto end() const {
        return iterator{SIZE, this};
    }

    ~flat_array_map() noexcept {
        if (occupancy != 0) {
            for (size_t i = 0; i < SIZE; ++i) {
                if (occupied(i)) {
                    data(i).~pair();
                }
            }
        }
    }

    [[nodiscard]] size_t size() const {
        return std::bitset<64>(occupancy).count();
    }

    template <typename... Args>
    std::pair<map_pair*, bool> try_emplace(size_t index, Args&&... args) {
        if (!occupied(index)) {
            new (reinterpret_cast<void*>(&data_[index])) map_pair(
                std::piecewise_construct,
                std::forward_as_tuple(index),
                std::forward_as_tuple(std::forward<Args>(args)...));
            occupy(index);
            return {&data(index), true};
        }
        return {&data(index), false};
    }

    bool erase(size_t index) {
        if (occupied(index)) {
            data(index).~pair();
            unoccupy(index);
            return true;
        }
        return false;
    }

    bool erase(const iterator& iterator) {
        return erase(iterator.first);
    }

  private:
    map_pair& data(size_t index) {
        assert(occupied(index));
        return *std::launder(reinterpret_cast<map_pair*>(&data_[index]));
    }

    const map_pair& data(size_t index) const {
        assert(occupied(index));
        return *std::launder(reinterpret_cast<const map_pair*>(&data_[index]));
    }

    [[nodiscard]] size_t lower_bound_index(size_t index) const {
        assert(index < SIZE);
        size_t num_zeros = CountTrailingZeros(occupancy >> index);
        return std::min(SIZE, index + num_zeros);
    }

    void occupy(size_t index) {
        assert(index < SIZE);
        assert(!occupied(index));
        occupancy ^= (1ul << index);
    }

    void unoccupy(size_t index) {
        assert(index < SIZE);
        assert(occupied(index));
        occupancy ^= (1ul << index);
    }

    [[nodiscard]] bool occupied(size_t index) const {
        return (occupancy >> index) & 1;
    }

    std::uint64_t occupancy = 0;
    std::aligned_storage_t<sizeof(map_pair), alignof(map_pair)> data_[SIZE];
};

template <typename T, std::size_t SIZE>
class array_map {
    static_assert(SIZE <= 64);
    static_assert(SIZE > 0);
    using iterator = improbable::phtree::detail::flat_map_iterator<T, SIZE>;

  public:
    array_map() {
        data_ = new flat_array_map<T, SIZE>();
    }

    array_map(const array_map& other) = delete;
    array_map& operator=(const array_map& other) = delete;

    array_map(array_map&& other) noexcept : data_{other.data_} {
        other.data_ = nullptr;
    }

    array_map& operator=(array_map&& other) noexcept {
        data_ = other.data_;
        other.data_ = nullptr;
        return *this;
    }

    ~array_map() {
        delete data_;
    }

    [[nodiscard]] auto find(size_t index) noexcept {
        return data_->find(index);
    }

    [[nodiscard]] auto find(size_t key) const noexcept {
        return const_cast<array_map&>(*this).find(key);
    }

    [[nodiscard]] auto lower_bound(size_t index) const {
        return data_->lower_bound(index);
    }

    [[nodiscard]] auto begin() const {
        return data_->begin();
    }

    [[nodiscard]] iterator cbegin() const {
        return data_->cbegin();
    }

    [[nodiscard]] auto end() const {
        return data_->end();
    }

    template <typename... Args>
    auto emplace(Args&&... args) {
        return data_->try_emplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto try_emplace(size_t index, Args&&... args) {
        return data_->try_emplace(index, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto try_emplace(const iterator&, size_t index, Args&&... args) {
        return data_->try_emplace(index, std::forward<Args>(args)...).first;
    }

    bool erase(size_t index) {
        return data_->erase(index);
    }

    bool erase(const iterator& iterator) {
        return data_->erase(iterator);
    }

    [[nodiscard]] size_t size() const {
        return data_->size();
    }

  private:
    flat_array_map<T, SIZE>* data_;
};

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_FLAT_ARRAY_MAP_H


