#ifndef PHTREE_V16_ITERATOR_HC_H
#define PHTREE_V16_ITERATOR_HC_H

#include "iterator_with_parent.h"
#include "common.h"

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename SCALAR>
class Node;

namespace {
template <dimension_t DIM, typename T, typename SCALAR>
class NodeIterator;
}

template <typename T, typename CONVERT, typename FILTER>
class IteratorHC : public IteratorWithFilter<T, CONVERT, FILTER> {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using KeyInternal = typename CONVERT::KeyInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using EntryT = typename IteratorWithFilter<T, CONVERT, FILTER>::EntryT;

  public:
    template <typename F>
    IteratorHC(
        const EntryT& root,
        const KeyInternal& range_min,
        const KeyInternal& range_max,
        const CONVERT* converter,
        F&& filter)
    : IteratorWithFilter<T, CONVERT, F>(converter, std::forward<F>(filter))
    , stack_size_{0}
    , range_min_{range_min}
    , range_max_{range_max} {
        stack_.reserve(8);
        PrepareAndPush(root);
        FindNextElement();
    }

    IteratorHC& operator++() noexcept {
        FindNextElement();
        return *this;
    }

    IteratorHC operator++(int) noexcept {
        IteratorHC iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    void FindNextElement() noexcept {
        while (!IsEmpty()) {
            auto* p = &Peek();
            const EntryT* current_result;
            while ((current_result = p->Increment(range_min_, range_max_))) {
                if (this->ApplyFilter(*current_result)) {
                    if (current_result->IsNode()) {
                        p = &PrepareAndPush(*current_result);
                    } else {
                        this->SetCurrentResult(current_result);
                        return;
                    }
                }
            }
            Pop();
        }
        this->SetFinished();
    }

    auto& PrepareAndPush(const EntryT& entry) noexcept {
        if (stack_.size() < stack_size_ + 1) {
            stack_.emplace_back();
        }
        assert(stack_size_ < stack_.size());
        auto& ni = stack_[stack_size_++];
        ni.Init(range_min_, range_max_, entry);
        return ni;
    }

    auto& Peek() noexcept {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1];
    }

    auto& Pop() noexcept {
        assert(stack_size_ > 0);
        return stack_[--stack_size_];
    }

    bool IsEmpty() noexcept {
        return stack_size_ == 0;
    }

    std::vector<NodeIterator<DIM, T, SCALAR>> stack_;
    size_t stack_size_;
    const KeyInternal range_min_;
    const KeyInternal range_max_;
};

namespace {
template <dimension_t DIM, typename T, typename SCALAR>
class NodeIterator {
    using KeyT = PhPoint<DIM, SCALAR>;
    using EntryT = Entry<DIM, T, SCALAR>;
    using EntriesT = const EntryMap<DIM, EntryT>;
    using hc_pos_t = hc_pos_dim_t<DIM>;

  public:
    NodeIterator() : iter_{}, entries_{nullptr}, mask_lower_{0}, mask_upper_{0}, postfix_len_{0} {}

    void Init(const KeyT& range_min, const KeyT& range_max, const EntryT& entry) {
        auto& node = entry.GetNode();
        CalcLimits(entry.GetNodePostfixLen(), range_min, range_max, entry.GetKey());
        iter_ = node.Entries().lower_bound(mask_lower_);
        entries_ = &node.Entries();
        postfix_len_ = entry.GetNodePostfixLen();
    }

    const EntryT* Increment(const KeyT& range_min, const KeyT& range_max) {
        while (iter_ != entries_->end() && iter_->first <= mask_upper_) {
            if (IsPosValid(iter_->first)) {
                const auto* be = &iter_->second;
                if (CheckEntry(*be, range_min, range_max)) {
                    ++iter_;
                    return be;
                }
            }
            ++iter_;
        }
        return nullptr;
    }

    bool CheckEntry(const EntryT& candidate, const KeyT& range_min, const KeyT& range_max) const {
        if (candidate.IsValue()) {
            return IsInRange(candidate.GetKey(), range_min, range_max);
        }

        if (!candidate.HasNodeInfix(postfix_len_)) {
            return true;
        }

        assert(candidate.GetNodePostfixLen() + 1 < MAX_BIT_WIDTH<SCALAR>);
        SCALAR comparison_mask = MAX_MASK<SCALAR> << (candidate.GetNodePostfixLen() + 1);
        auto& key = candidate.GetKey();
        for (dimension_t dim = 0; dim < DIM; ++dim) {
            SCALAR in = key[dim] & comparison_mask;
            if (in > range_max[dim] || in < (range_min[dim] & comparison_mask)) {
                return false;
            }
        }
        return true;
    }

  private:
    [[nodiscard]] inline bool IsPosValid(hc_pos_t key) const noexcept {
        return ((key | mask_lower_) & mask_upper_) == key;
    }

    void CalcLimits(
        bit_width_t postfix_len, const KeyT& range_min, const KeyT& range_max, const KeyT& prefix) {
        assert(postfix_len < MAX_BIT_WIDTH<SCALAR>);
        hc_pos_t lower_limit = 0;
        hc_pos_t upper_limit = 0;
        if (postfix_len < MAX_BIT_WIDTH<SCALAR> - 1) {
            for (dimension_t i = 0; i < DIM; ++i) {
                lower_limit <<= 1;
                lower_limit |= range_min[i] >= prefix[i];
            }
            for (dimension_t i = 0; i < DIM; ++i) {
                upper_limit <<= 1;
                upper_limit |= range_max[i] >= prefix[i];
            }
        } else {
            for (dimension_t i = 0; i < DIM; ++i) {
                upper_limit <<= 1;
                upper_limit |= range_min[i] < 0;
            }
            for (dimension_t i = 0; i < DIM; ++i) {
                lower_limit <<= 1;
                lower_limit |= range_max[i] < 0;
            }
        }
        mask_lower_ = lower_limit;
        mask_upper_ = upper_limit;
    }

  private:
    EntryIteratorC<DIM, EntryT> iter_;
    EntriesT* entries_;
    hc_pos_t mask_lower_;
    hc_pos_t mask_upper_;
    bit_width_t postfix_len_;
};
}
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ITERATOR_HC_H


