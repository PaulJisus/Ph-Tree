#ifndef PHTREE_V16_ENTRY_H
#define PHTREE_V16_ENTRY_H

#include "node.h"
#include "common.h"
#include <cassert>
#include <memory>

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename SCALAR>
class Node;

template <dimension_t DIM, typename T, typename SCALAR>
class Entry {
    using KeyT = PhPoint<DIM, SCALAR>;
    using ValueT = std::remove_const_t<T>;
    using NodeT = Node<DIM, T, SCALAR>;

    enum {
        VALUE = 0,
        NODE = 1,
        EMPTY = 2,
    };

  public:

    Entry(const KeyT& k, NodeT&& node, bit_width_t postfix_len) noexcept
    : kd_key_{k}
    , node_{std::move(node)}
    , union_type_{NODE}
    , postfix_len_{static_cast<std::uint16_t>(postfix_len)} {}

    template <typename ValueT2 = ValueT>
    Entry(
        const KeyT& k,
        ValueT2&& value,
        typename std::enable_if_t<!std::is_move_constructible_v<ValueT2>, int>::type = 0) noexcept
    : kd_key_{k}, value_(value), union_type_{VALUE}, postfix_len_{0} {}

    template <typename ValueT2 = ValueT>
    Entry(
        const KeyT& k,
        ValueT2&& value,
        typename std::enable_if_t<std::is_move_constructible_v<ValueT2>, int>::type = 0) noexcept
    : kd_key_{k}, value_(std::forward<ValueT2>(value)), union_type_{VALUE}, postfix_len_{0} {}

    template <
        typename ValueT2 = ValueT,
        typename = std::enable_if_t<!std::is_move_constructible_v<ValueT2>>>
    explicit Entry(const KeyT& k, const ValueT& value) noexcept
    : kd_key_{k}, value_(value), union_type_{VALUE}, postfix_len_{0} {}

    template <
        typename ValueT2 = ValueT,
        typename = std::enable_if_t<!std::is_move_constructible_v<ValueT2>>>
    explicit Entry(const KeyT& k) noexcept
    : kd_key_{k}, value_(), union_type_{VALUE}, postfix_len_{0} {}

    template <
        typename... Args,
        typename ValueT2 = ValueT,
        typename = std::enable_if_t<std::is_move_constructible_v<ValueT2>>>
    explicit Entry(const KeyT& k, Args&&... args) noexcept
    : kd_key_{k}, value_(std::forward<Args>(args)...), union_type_{VALUE}, postfix_len_{0} {}

    Entry(const Entry& other) = delete;
    Entry& operator=(const Entry& other) = delete;

    Entry(Entry&& other) noexcept
    : kd_key_{std::move(other.kd_key_)}, union_type_{std::move(other.union_type_)} {
        postfix_len_ = std::move(other.postfix_len_);
        AssignUnion(std::move(other));
    }

    Entry& operator=(Entry&& other) noexcept {
        kd_key_ = std::move(other.kd_key_);
        postfix_len_ = std::move(other.postfix_len_);
        DestroyUnion();
        AssignUnion(std::move(other));
        return *this;
    }

    ~Entry() noexcept {
        DestroyUnion();
    }

    void SetNodeCenter() {
        assert(union_type_ == NODE);
        bit_mask_t<SCALAR> maskHcBit = bit_mask_t<SCALAR>(1) << postfix_len_;
        bit_mask_t<SCALAR> maskVT = MAX_MASK<SCALAR> << postfix_len_;
        if (postfix_len_ < MAX_BIT_WIDTH<SCALAR> - 1) {
            for (dimension_t i = 0; i < DIM; ++i) {
                kd_key_[i] = (kd_key_[i] | maskHcBit) & maskVT;
            }
        } else {
            for (dimension_t i = 0; i < DIM; ++i) {
                kd_key_[i] = 0;
            }
        }
    }

    [[nodiscard]] const KeyT& GetKey() const {
        return kd_key_;
    }

    [[nodiscard]] bool IsValue() const {
        return union_type_ == VALUE;
    }

    [[nodiscard]] bool IsNode() const {
        return union_type_ == NODE;
    }

    [[nodiscard]] T& GetValue() const {
        assert(union_type_ == VALUE);
        return const_cast<T&>(value_);
    }

    [[nodiscard]] const NodeT& GetNode() const {
        assert(union_type_ == NODE);
        return node_;
    }

    [[nodiscard]] NodeT& GetNode() {
        assert(union_type_ == NODE);
        return node_;
    }

    void SetKey(const KeyT& key) noexcept {
        assert(union_type_ == VALUE);
        kd_key_ = key;
    }

    void SetNode(NodeT&& node, bit_width_t postfix_len) noexcept {
        postfix_len_ = static_cast<std::uint16_t>(postfix_len);
        DestroyUnion();
        union_type_ = NODE;
        new (&node_) NodeT{std::move(node)};
        SetNodeCenter();
    }

    [[nodiscard]] bit_width_t GetNodePostfixLen() const noexcept {
        assert(IsNode());
        return postfix_len_;
    }

    [[nodiscard]] bit_width_t GetNodeInfixLen(bit_width_t parent_postfix_len) const noexcept {
        assert(IsNode());
        return parent_postfix_len - GetNodePostfixLen() - 1;
    }

    [[nodiscard]] bool HasNodeInfix(bit_width_t parent_postfix_len) const noexcept {
        assert(IsNode());
        return parent_postfix_len - GetNodePostfixLen() - 1 > 0;
    }

    [[nodiscard]] ValueT&& ExtractValue() noexcept {
        assert(IsValue());
        return std::move(value_);
    }

    [[nodiscard]] NodeT&& ExtractNode() noexcept {
        assert(IsNode());
        union_type_ = EMPTY;
        return std::move(node_);
    }

    void ReplaceNodeWithDataFromEntry(Entry&& other) {
        assert(IsNode());
        auto node = std::move(node_);
        union_type_ = EMPTY;
        *this = std::move(other);
        if (IsNode()) {
            SetNodeCenter();
        }
    }

  private:
    void AssignUnion(Entry&& other) noexcept {
        union_type_ = std::move(other.union_type_);
        if (union_type_ == NODE) {
            new (&node_) NodeT{std::move(other.node_)};
        } else if (union_type_ == VALUE) {
            if constexpr (std::is_move_constructible_v<ValueT>) {
                new (&value_) ValueT{std::move(other.value_)};
            } else {
                new (&value_) ValueT{other.value_};
            }
        } else {
            assert(false && "Assigning from an EMPTY variant is a waste of time.");
        }
    }

    void DestroyUnion() noexcept {
        if (union_type_ == VALUE) {
            value_.~ValueT();
        } else if (union_type_ == NODE) {
            node_.~NodeT();
        } else {
            assert(union_type_ == EMPTY);
        }
        union_type_ = EMPTY;
    }

    KeyT kd_key_;
    union {
        NodeT node_;
        ValueT value_;
    };
    std::uint16_t union_type_;
    std::uint16_t postfix_len_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ENTRY_H

