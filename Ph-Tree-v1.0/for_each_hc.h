#ifndef PHTREE_V16_FOR_EACH_HC_H
#define PHTREE_V16_FOR_EACH_HC_H

#include "iterator_with_parent.h"
#include "common.h"

namespace improbable::phtree::v16 {

template <typename T, typename CONVERT, typename CALLBACK, typename FILTER>
class ForEachHC {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using KeyInternal = typename CONVERT::KeyInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using EntryT = Entry<DIM, T, SCALAR>;
    using hc_pos_t = hc_pos_dim_t<DIM>;

  public:
    template <typename CB, typename F>
    ForEachHC(
        const KeyInternal& range_min,
        const KeyInternal& range_max,
        const CONVERT* converter,
        CB&& callback,
        F&& filter)
    : range_min_{range_min}
    , range_max_{range_max}
    , converter_{converter}
    , callback_{std::forward<CB>(callback)}
    , filter_(std::forward<F>(filter)) {}

    void Traverse(const EntryT& entry, const EntryIteratorC<DIM, EntryT>* opt_it = nullptr) {
        assert(entry.IsNode());
        hc_pos_t mask_lower = 0;
        hc_pos_t mask_upper = 0;
        CalcLimits(entry.GetNodePostfixLen(), entry.GetKey(), mask_lower, mask_upper);
        auto& entries = entry.GetNode().Entries();
        auto postfix_len = entry.GetNodePostfixLen();
        auto end = entries.end();
        auto iter = opt_it != nullptr && *opt_it != end ? *opt_it : entries.lower_bound(mask_lower);
        for (; iter != end && iter->first <= mask_upper; ++iter) {
            auto child_hc_pos = iter->first;
            if (((child_hc_pos | mask_lower) & mask_upper) == child_hc_pos) {
                const auto& child = iter->second;
                const auto& child_key = child.GetKey();
                if (child.IsNode()) {
                    if (CheckNode(child, postfix_len)) {
                        Traverse(child);
                    }
                } else {
                    T& value = child.GetValue();
                    if (IsInRange(child_key, range_min_, range_max_) &&
                        filter_.IsEntryValid(child_key, value)) {
                        callback_(converter_->post(child_key), value);
                    }
                }
            }
        }
    }

  private:
    bool CheckNode(const EntryT& entry, bit_width_t parent_postfix_len) {
        const KeyInternal& key = entry.GetKey();
        bool mismatch = false;
        if (entry.HasNodeInfix(parent_postfix_len)) {
            assert(entry.GetNodePostfixLen() + 1 < MAX_BIT_WIDTH<SCALAR>);
            SCALAR comparison_mask = MAX_MASK<SCALAR> << (entry.GetNodePostfixLen() + 1);
            for (dimension_t dim = 0; dim < DIM; ++dim) {
                SCALAR prefix = key[dim] & comparison_mask;
                mismatch |= (prefix > range_max_[dim] || prefix < (range_min_[dim] & comparison_mask));
            }
        }
        return mismatch ? false : filter_.IsNodeValid(key, entry.GetNodePostfixLen() + 1);
    }

    void CalcLimits(
        bit_width_t postfix_len,
        const KeyInternal& prefix,
        hc_pos_t& lower_limit,
        hc_pos_t& upper_limit) {
        assert(postfix_len < MAX_BIT_WIDTH<SCALAR>);
        if (postfix_len < MAX_BIT_WIDTH<SCALAR> - 1) {
            for (dimension_t i = 0; i < DIM; ++i) {
                lower_limit <<= 1;
                lower_limit |= range_min_[i] >= prefix[i];
            }
            for (dimension_t i = 0; i < DIM; ++i) {
                upper_limit <<= 1;
                upper_limit |= range_max_[i] >= prefix[i];
            }
        } else {
            for (dimension_t i = 0; i < DIM; ++i) {
                upper_limit <<= 1;
                upper_limit |= range_min_[i] < 0;
            }
            for (dimension_t i = 0; i < DIM; ++i) {
                lower_limit <<= 1;
                lower_limit |= range_max_[i] < 0;
            }
        }
    }

    const KeyInternal range_min_;
    const KeyInternal range_max_;
    const CONVERT* converter_;
    CALLBACK callback_;
    FILTER filter_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_FOR_EACH_HC_H


