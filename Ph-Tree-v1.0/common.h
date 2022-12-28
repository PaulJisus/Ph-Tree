#ifndef PHTREE_COMMON_COMMON_H
#define PHTREE_COMMON_COMMON_H

#include "b_plus_tree_map.h"
#include "base_types.h"
#include "bits.h"
#include "flat_array_map.h"
#include "flat_sparse_map.h"
#include "tree_stats.h"
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>

namespace improbable::phtree {

template <dimension_t DIM, typename SCALAR>
static hc_pos_64_t CalcPosInArray(const PhPoint<DIM, SCALAR>& valSet, bit_width_t postfix_len) {
    bit_mask_t<SCALAR> valMask = bit_mask_t<SCALAR>(1) << postfix_len;
    hc_pos_64_t pos = 0;
    for (dimension_t i = 0; i < DIM; ++i) {
        pos <<= 1;
        pos |= (valMask & valSet[i]) >> postfix_len;
    }
    return pos;
}

template <dimension_t DIM, typename SCALAR>
static bool IsInRange(
    const PhPoint<DIM, SCALAR>& candidate,
    const PhPoint<DIM, SCALAR>& range_min,
    const PhPoint<DIM, SCALAR>& range_max) {
    for (dimension_t i = 0; i < DIM; ++i) {
        auto k = candidate[i];
        if (k < range_min[i] || k > range_max[i]) {
            return false;
        }
    }
    return true;
}

template <dimension_t DIM, typename SCALAR>
static bit_width_t NumberOfDivergingBits(
    const PhPoint<DIM, SCALAR>& v1, const PhPoint<DIM, SCALAR>& v2) {
    SCALAR diff = 0;
    for (dimension_t i = 0; i < DIM; ++i) {
        diff |= (v1[i] ^ v2[i]);
    }
    auto diff2 = reinterpret_cast<bit_mask_t<SCALAR>&>(diff);
    assert(CountLeadingZeros(diff2) <= MAX_BIT_WIDTH<SCALAR>);
    return MAX_BIT_WIDTH<SCALAR> - CountLeadingZeros(diff2);
}

template <dimension_t DIM, typename SCALAR>
static bool KeyEquals(
    const PhPoint<DIM, SCALAR>& key_a, const PhPoint<DIM, SCALAR>& key_b, bit_width_t ignore_bits) {
    SCALAR diff{0};
    for (dimension_t i = 0; i < DIM; ++i) {
        diff |= key_a[i] ^ key_b[i];
    }
    return diff >> ignore_bits == 0;
}

template <typename SCALAR>
static inline std::string ToBinary(SCALAR l, bit_width_t width = MAX_BIT_WIDTH<SCALAR>) {
    std::ostringstream sb;
    for (bit_width_t i = 0; i < width; ++i) {
        bit_mask_t<SCALAR> mask = (bit_mask_t<SCALAR>(1) << (width - i - 1));
        sb << ((l & mask) != 0 ? "1" : "0");
        if ((i + 1) % 8 == 0 && (i + 1) < width) {
            sb << '.';
        }
    }
    return sb.str();
}

template <dimension_t DIM, typename SCALAR>
static inline std::string ToBinary(
    const PhPoint<DIM, SCALAR>& la, bit_width_t width = MAX_BIT_WIDTH<SCALAR>) {
    std::ostringstream sb;
    for (dimension_t i = 0; i < DIM; ++i) {
        sb << ToBinary(la[i], width) << ", ";
    }
    return sb.str();
}

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_COMMON_H


