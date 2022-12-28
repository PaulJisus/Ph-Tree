#ifndef PHTREE_COMMON_B_PLUS_TREE_H
#define PHTREE_COMMON_B_PLUS_TREE_H

#include "b_plus_tree_base.h"
#include "bits.h"
#include <cassert>
#include <tuple>
#include <vector>

namespace improbable::phtree {
using namespace ::phtree::bptree::detail;

template <typename KeyT, typename ValueT, std::uint64_t COUNT_MAX>
class b_plus_tree_map {
    static_assert(std::is_integral<KeyT>() && "Key type must be integer");
    static_assert(std::is_unsigned<KeyT>() && "Key type must unsigned");

    constexpr static size_t LEAF_MAX = std::min(std::uint64_t(16), COUNT_MAX);
    constexpr static size_t INNER_MAX = std::min(std::uint64_t(16), COUNT_MAX / LEAF_MAX * 2);
    static_assert(LEAF_MAX > 2 && LEAF_MAX < 1000);
    static_assert(COUNT_MAX <= (16 * 16) || (INNER_MAX > 2 && INNER_MAX < 1000));
    constexpr static size_t LEAF_MIN = 2;
    constexpr static size_t INNER_MIN = 2;
    constexpr static size_t LEAF_INIT = std::min(size_t(2), LEAF_MAX);
    constexpr static size_t INNER_INIT = std::min(size_t(4), INNER_MAX);
    using LEAF_CFG = bpt_config<LEAF_MAX, LEAF_MIN, LEAF_INIT>;
    using INNER_CFG = bpt_config<INNER_MAX, INNER_MIN, INNER_INIT>;

    class bpt_node_leaf;
    class bpt_iterator;
    using LeafEntryT = std::pair<KeyT, ValueT>;
    using IterT = bpt_iterator;
    using NLeafT = bpt_node_leaf;
    using NInnerT = bpt_node_inner<KeyT, NLeafT, IterT, INNER_CFG>;
    using NodeT = bpt_node_base<KeyT, NInnerT, bpt_node_leaf>;
    using LeafIteratorT = decltype(std::vector<LeafEntryT>().begin());
    using TreeT = b_plus_tree_map<KeyT, ValueT, COUNT_MAX>;

  public:
    explicit b_plus_tree_map() : root_{new NLeafT(nullptr, nullptr, nullptr)}, size_{0} {};

    b_plus_tree_map(const b_plus_tree_map& other) : size_{other.size_} {
        root_ = other.root_->is_leaf() ? new NLeafT(*other.root_->as_leaf())
                                       : new NInnerT(*other.root_->as_inner());
    }

    b_plus_tree_map(b_plus_tree_map&& other) noexcept : root_{other.root_}, size_{other.size_} {
        other.root_ = nullptr;
        other.size_ = 0;
    }

    b_plus_tree_map& operator=(const b_plus_tree_map& other) {
        assert(this != &other);
        delete root_;
        root_ = other.root_->is_leaf() ? new NLeafT(*other.root_->as_leaf())
                                       : new NInnerT(*other.root_->as_inner());
        size_ = other.size_;
        return *this;
    }

    b_plus_tree_map& operator=(b_plus_tree_map&& other) noexcept {
        delete root_;
        root_ = other.root_;
        other.root_ = nullptr;
        size_ = other.size_;
        other.size_ = 0;
        return *this;
    }

    ~b_plus_tree_map() {
        delete root_;
        root_ = nullptr;
    }

    [[nodiscard]] auto find(KeyT key) noexcept {
        auto leaf = lower_bound_leaf(key, root_);
        return leaf != nullptr ? leaf->find(key) : IterT{};
    }

    [[nodiscard]] auto find(KeyT key) const noexcept {
        return const_cast<b_plus_tree_map&>(*this).find(key);
    }

    [[nodiscard]] auto lower_bound(KeyT key) noexcept {
        auto leaf = lower_bound_leaf(key, root_);
        return leaf != nullptr ? leaf->lower_bound_as_iter(key) : IterT{};
    }

    [[nodiscard]] auto lower_bound(KeyT key) const noexcept {
        return const_cast<b_plus_tree_map&>(*this).lower_bound(key);
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
        return try_emplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto emplace_hint(const IterT& hint, KeyT key, Args&&... args) {
        if (empty() || hint.is_end()) {
            return emplace(key, std::forward<Args>(args)...);
        }
        assert(hint.node_->is_leaf());

        auto node = hint.node_->as_leaf();
        if (node->data_.begin()->first > key || (node->data_.end() - 1)->first < key) {
            return emplace(key, std::forward<Args>(args)...);
        }
        return node->try_emplace(key, root_, size_, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto try_emplace(KeyT key, Args&&... args) {
        auto leaf = lower_bound_or_last_leaf(key, root_);
        return leaf->try_emplace(key, root_, size_, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto try_emplace(IterT iter, KeyT key, Args&&... args) {
        return emplace_hint(iter, key, std::forward<Args>(args)...).first;
    }

    void erase(KeyT key) {
        auto leaf = lower_bound_leaf(key, root_);
        if (leaf != nullptr) {
            size_ -= leaf->erase_key(key, root_);
        }
    }

    void erase(const IterT& iterator) {
        assert(iterator != end());
        --size_;
        iterator.node_->erase_entry(iterator.iter_, root_);
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
        KeyT known_min = std::numeric_limits<KeyT>::max();
        root_->_check(count, nullptr, prev_leaf, known_min, 0);
        assert(count == size());
    }

  private:
    using bpt_leaf_super =
        bpt_node_data<KeyT, NInnerT, NLeafT, NLeafT, LeafEntryT, IterT, LEAF_CFG>;
    class bpt_node_leaf : public bpt_leaf_super {
      public:
        explicit bpt_node_leaf(NInnerT* parent, NLeafT* prev, NLeafT* next) noexcept
        : bpt_leaf_super(true, parent, prev, next) {}

        ~bpt_node_leaf() noexcept = default;

        [[nodiscard]] IterT find(KeyT key) noexcept {
            auto it = this->lower_bound(key);
            if (it != this->data_.end() && it->first == key) {
                return IterT(this, it);
            }
            return IterT();
        }

        template <typename... Args>
        auto try_emplace(KeyT key, NodeT*& root, size_t& entry_count, Args&&... args) {
            auto it = this->lower_bound(key);
            if (it != this->data_.end() && it->first == key) {
                return std::make_pair(IterT(this, it), false);
            }
            ++entry_count;

            auto full_it = this->check_split_and_adjust_iterator(it, key, root);
            auto it_result = full_it.node_->data_.emplace(
                full_it.iter_,
                std::piecewise_construct,
                std::forward_as_tuple(key),
                std::forward_as_tuple(std::forward<Args>(args)...));
            return std::make_pair(IterT(full_it.node_, it_result), true);
        }

        bool erase_key(KeyT key, NodeT*& root) {
            auto it = this->lower_bound(key);
            if (it != this->data_.end() && it->first == key) {
                this->erase_entry(it, root);
                return true;
            }
            return false;
        }

        void _check(
            size_t& count, NInnerT* parent, NLeafT*& prev_leaf, KeyT& known_min, KeyT known_max) {
            this->_check_data(parent, known_max);

            assert(prev_leaf == this->prev_node_);
            for (auto& e : this->data_) {
                assert(count == 0 || e.first > known_min);
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
        using value_type = ValueT;
        using difference_type = std::ptrdiff_t;
        using pointer = ValueT*;
        using reference = ValueT&;

        explicit bpt_iterator(NLeafT* node, LeafIteratorT it) noexcept : SuperT(node, it) {}

        explicit bpt_iterator(NodeT* node) noexcept : SuperT(node) {}

        bpt_iterator() noexcept : SuperT() {}

        auto& operator*() const noexcept {
            return const_cast<LeafEntryT&>(*this->iter());
        }

        auto* operator->() const noexcept {
            return const_cast<LeafEntryT*>(&*this->iter());
        }
    };

  private:
    NodeT* root_;
    size_t size_;
};
}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_B_PLUS_TREE_H


