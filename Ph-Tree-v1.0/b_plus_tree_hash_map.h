#ifndef PHTREE_COMMON_B_PLUS_TREE_HASH_MAP_H
#define PHTREE_COMMON_B_PLUS_TREE_HASH_MAP_H

#include "b_plus_tree_base.h"
#include "bits.h"
#include <cassert>
#include <tuple>
#include <vector>

namespace improbable::phtree {
using namespace ::phtree::bptree::detail;

template <typename T, typename HashT = std::hash<T>, typename PredT = std::equal_to<T>>
class b_plus_tree_hash_set {
    using hash_t = std::uint32_t;

    class bpt_node_leaf;
    class bpt_iterator;
    using LeafEntryT = std::pair<hash_t, T>;
    using IterT = bpt_iterator;
    using NLeafT = bpt_node_leaf;
    using NInnerT = bpt_node_inner<hash_t, NLeafT, IterT>;
    using NodeT = bpt_node_base<hash_t, NInnerT, bpt_node_leaf>;
    using LeafIteratorT = decltype(std::vector<LeafEntryT>().begin());
    using TreeT = b_plus_tree_hash_set<T, HashT, PredT>;

  public:
    using value_compare = PredT;
    explicit b_plus_tree_hash_set() : root_{new NLeafT(nullptr, nullptr, nullptr)}, size_{0} {};

    b_plus_tree_hash_set(const b_plus_tree_hash_set& other) : size_{other.size_} {
        root_ = other.root_->is_leaf() ? new NLeafT(*other.root_->as_leaf())
                                       : new NInnerT(*other.root_->as_inner());
    }

    b_plus_tree_hash_set(b_plus_tree_hash_set&& other) noexcept
    : root_{other.root_}, size_{other.size_} {
        other.root_ = nullptr;
        other.size_ = 0;
    }

    b_plus_tree_hash_set& operator=(const b_plus_tree_hash_set& other) {
        assert(this != &other);
        delete root_;
        root_ = other.root_->is_leaf() ? new NLeafT(*other.root_->as_leaf())
                                       : new NInnerT(*other.root_->as_inner());
        size_ = other.size_;
        return *this;
    }

    b_plus_tree_hash_set& operator=(b_plus_tree_hash_set&& other) noexcept {
        delete root_;
        root_ = other.root_;
        other.root_ = nullptr;
        size_ = other.size_;
        other.size_ = 0;
        return *this;
    }

    ~b_plus_tree_hash_set() {
        delete root_;
        root_ = nullptr;
    }

    [[nodiscard]] auto find(const T& value) {
        auto hash = (hash_t)HashT{}(value);
        auto leaf = lower_bound_leaf(hash, root_);
        return leaf != nullptr ? leaf->find(hash, value) : IterT{};
    }

    [[nodiscard]] auto find(const T& value) const {
        return const_cast<b_plus_tree_hash_set&>(*this).find(value);
    }

    [[nodiscard]] auto lower_bound(const T& value) {
        auto hash = (hash_t)HashT{}(value);
        auto leaf = lower_bound_leaf(hash, root_);
        return leaf != nullptr ? leaf->lower_bound_value(hash, value) : IterT{};
    }

    [[nodiscard]] auto lower_bound(const T& value) const {
        return const_cast<b_plus_tree_hash_set&>(*this).lower_bound(value);
    }

    [[nodiscard]] size_t count(const T& value) const {
        return const_cast<b_plus_tree_hash_set&>(*this).find(value) != end();
    }

    [[nodiscard]] auto begin() noexcept {
        return IterT(root_);
    }

    [[nodiscard]] auto begin() const noexcept {
        return IterT(root_);
    }

    [[nodiscard]] auto cbegin() const noexcept {
        return IterT(root_);
    }

    [[nodiscard]] auto end() noexcept {
        return IterT();
    }

    [[nodiscard]] auto end() const noexcept {
        return IterT();
    }

    template <typename... Args>
    auto emplace(Args&&... args) {
        T t(std::forward<Args>(args)...);
        hash_t hash = (hash_t)HashT{}(t);
        auto leaf = lower_bound_or_last_leaf(hash, root_);
        return leaf->try_emplace(hash, root_, size_, std::move(t));
    }

    template <typename... Args>
    auto emplace_hint(const IterT& hint, Args&&... args) {
        if (empty() || hint.is_end()) {
            return emplace(std::forward<Args>(args)...).first;
        }

        T t(std::forward<Args>(args)...);
        auto hash = (hash_t)HashT{}(t);
        auto node = hint.node_->as_leaf();

        if (node->data_.begin()->first > hash || (node->data_.end() - 1)->first < hash) {
            return emplace(std::move(t)).first;
        }

        return node->try_emplace(hash, root_, size_, std::move(t)).first;
    }

    size_t erase(const T& value) {
        auto hash = (hash_t)HashT{}(value);
        auto leaf = lower_bound_leaf(hash, root_);
        if (leaf == nullptr) {
            return 0;
        }

        auto iter = leaf->lower_bound_value(hash, value);
        if (!iter.is_end() && PredT{}(*iter, value)) {
            iter.node_->erase_entry(iter.iter_, root_);
            --size_;
            return 1;
        }
        return 0;
    }

    auto erase(const IterT& iterator) {
        assert(iterator != end());
        --size_;
        auto result = iterator.node_->erase_entry(iterator.iter_, root_);
        if (result.node_) {
            return IterT(static_cast<NLeafT*>(result.node_), result.iter_);
        }
        return IterT();
    }

    [[nodiscard]] size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    void _check() {
        size_t count = 0;
        NLeafT* prev_leaf = nullptr;
        hash_t known_min = std::numeric_limits<hash_t>::max();
        root_->_check(count, nullptr, prev_leaf, known_min, 0);
        assert(count == size());
    }

  private:
    using bpt_leaf_super = bpt_node_data<hash_t, NInnerT, NLeafT, NLeafT, LeafEntryT, IterT>;
    class bpt_node_leaf : public bpt_leaf_super {
      public:
        explicit bpt_node_leaf(NInnerT* parent, NLeafT* prev, NLeafT* next) noexcept
        : bpt_leaf_super(true, parent, prev, next) {}

        ~bpt_node_leaf() noexcept = default;

        [[nodiscard]] IterT find(hash_t hash, const T& value) noexcept {
            PredT equals{};
            IterT iter_full = this->lower_bound_as_iter(hash);
            while (!iter_full.is_end() && iter_full.hash() == hash) {
                if (equals(*iter_full, value)) {
                    return iter_full;
                }
                ++iter_full;
            }
            return IterT();
        }

        [[nodiscard]] auto lower_bound_value(hash_t hash, const T& value) noexcept {
            PredT equals{};
            IterT iter_full = this->lower_bound_as_iter(hash);
            while (!iter_full.is_end() && iter_full.hash() == hash) {
                if (equals(*iter_full, value)) {
                    break;
                }
                ++iter_full;
            }
            return iter_full;
        }

        auto try_emplace(hash_t hash, NodeT*& root, size_t& entry_count, T&& t) {
            auto it = this->lower_bound(hash);
            if (it != this->data_.end() && it->first == hash) {
                PredT equals{};
                IterT full_iter(this, it);
                while (!full_iter.is_end() && full_iter.hash() == hash) {
                    if (equals(*full_iter, t)) {
                        return std::make_pair(full_iter, false);
                    }
                    ++full_iter;
                }
            }
            ++entry_count;

            auto full_it = this->check_split_and_adjust_iterator(it, hash, root);
            auto it_result = full_it.node_->data_.emplace(full_it.iter_, hash, std::move(t));
            return std::make_pair(IterT(full_it.node_, it_result), true);
        }

        void _check(
            size_t& count,
            NInnerT* parent,
            NLeafT*& prev_leaf,
            hash_t& known_min,
            hash_t known_max) {
            this->_check_data(parent, known_max);

            assert(prev_leaf == this->prev_node_);
            for (auto& e : this->data_) {
                assert(count == 0 || e.first >= known_min);
                assert(this->parent_ == nullptr || e.first <= known_max);
                ++count;
                known_min = e.first;
            }
            prev_leaf = this;
        }
    };

    class bpt_iterator : public bpt_iterator_base<LeafIteratorT, NLeafT, NodeT, TreeT> {
        using SuperT = bpt_iterator_base<LeafIteratorT, NLeafT, NodeT, TreeT>;

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        explicit bpt_iterator(NLeafT* node, LeafIteratorT it) noexcept : SuperT(node, it) {}
        explicit bpt_iterator(NodeT* node) noexcept : SuperT(node) {}
        bpt_iterator() noexcept : SuperT() {}

        auto& operator*() const noexcept {
            return const_cast<T&>(this->iter()->second);
        }

        auto* operator->() const noexcept {
            return const_cast<T*>(&this->iter()->second);
        }

        [[nodiscard]] auto hash() const noexcept {
            return this->iter()->first;
        }
    };

  private:
    NodeT* root_;
    size_t size_;
};

template <
    typename KeyT,
    typename ValueT,
    typename HashT = std::hash<KeyT>,
    typename PredT = std::equal_to<KeyT>>
class b_plus_tree_hash_map {
    class iterator;
    using IterT = iterator;
    using EntryT = std::pair<KeyT, ValueT>;

  public:
    using value_compare = PredT;
    b_plus_tree_hash_map() : map_{} {};

    b_plus_tree_hash_map(const b_plus_tree_hash_map&) = default;
    b_plus_tree_hash_map(b_plus_tree_hash_map&&) noexcept = default;
    b_plus_tree_hash_map& operator=(const b_plus_tree_hash_map&) = default;
    b_plus_tree_hash_map& operator=(b_plus_tree_hash_map&&) noexcept = default;
    ~b_plus_tree_hash_map() = default;

    auto begin() const {
        return IterT(map_.begin());
    }

    auto end() const {
        return IterT(map_.end());
    }

    auto find(const KeyT& key) const {
        return IterT(map_.find(EntryT{key, {}}));
    }

    [[nodiscard]] auto lower_bound(const KeyT& key) const {
        return IterT(map_.lower_bound(EntryT{key, {}}));
    }

    auto count(const KeyT& key) const {
        return map_.count(EntryT{key, {}});
    }

    template <typename... Args>
    auto emplace(Args&&... args) {
        return try_emplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto emplace_hint(const IterT& hint, Args&&... args) {
        return try_emplace(hint, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto try_emplace(const KeyT& key, Args&&... args) {
        auto result = map_.emplace(key, std::forward<Args>(args)...);
        return std::make_pair(iterator(result.first), result.second);
    }

    template <typename... Args>
    auto try_emplace(const IterT& hint, const KeyT& key, Args&&... args) {
        auto result = map_.emplace_hint(hint.map_iter_, key, std::forward<Args>(args)...);
        return IterT(result);
    }

    auto erase(const KeyT& key) {
        return map_.erase({key, {}});
    }

    auto erase(const IterT& iterator) {
        return IterT(map_.erase(iterator.map_iter_));
    }

    auto size() const {
        return map_.size();
    }

    auto empty() const {
        return map_.empty();
    }

    void _check() {
        map_._check();
    }

  private:
    struct EntryHashT {
        size_t operator()(const EntryT& x) const {
            return HashT{}(x.first);
        }
    };

    struct EntryEqualsT {
        bool operator()(const EntryT& x, const EntryT& y) const {
            return PredT{}(x.first, y.first);
        }
    };

    class iterator {
        using T = EntryT;
        using MapIterType =
            decltype(std::declval<b_plus_tree_hash_set<EntryT, EntryHashT, EntryEqualsT>>()
                         .begin());
        friend b_plus_tree_hash_map<KeyT, ValueT, HashT, PredT>;

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        explicit iterator(MapIterType map_iter) noexcept : map_iter_{map_iter} {}

        // end() iterator
        iterator() noexcept : map_iter_{} {}

        auto& operator*() const noexcept {
            return *map_iter_;
        }

        auto* operator->() const noexcept {
            return &*map_iter_;
        }

        auto& operator++() noexcept {
            ++map_iter_;
            return *this;
        }

        auto operator++(int) noexcept {
            IterT iterator(*this);
            ++(*this);
            return iterator;
        }

        friend bool operator==(const IterT& left, const IterT& right) noexcept {
            return left.map_iter_ == right.map_iter_;
        }

        friend bool operator!=(const IterT& left, const IterT& right) noexcept {
            return left.map_iter_ != right.map_iter_;
        }

      private:
        MapIterType map_iter_;
    };

    b_plus_tree_hash_set<EntryT, EntryHashT, EntryEqualsT> map_;
};

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_B_PLUS_TREE_HASH_MAP_H


