#ifndef PHTREE_COMMON_FILTERS_H
#define PHTREE_COMMON_FILTERS_H

#include "converter.h"
#include "distance.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>

namespace improbable::phtree {

struct FilterNoOp {
    template <typename KeyT, typename ValueT>
    constexpr bool IsEntryValid(const KeyT& /*key*/, const ValueT& /*value*/) const noexcept {
        return true;
    }

    template <typename KeyT>
    constexpr bool IsNodeValid(const KeyT& /*prefix*/, int /*bits_to_ignore*/) const noexcept {
        return true;
    }

    template <typename KeyT, typename ValueT>
    constexpr bool IsBucketEntryValid(const KeyT& /*key*/, const ValueT& /*value*/) const noexcept {
        return true;
    }
};

template <typename CONVERTER>
class FilterAABB {
    using KeyExternal = typename CONVERTER::KeyExternal;
    using KeyInternal = typename CONVERTER::KeyInternal;
    using ScalarInternal = typename CONVERTER::ScalarInternal;
    static constexpr auto DIM = CONVERTER::DimInternal;

  public:
    FilterAABB(
        const KeyExternal& min_include, const KeyExternal& max_include, const CONVERTER& converter)
    : min_external_{min_include}
    , max_external_{max_include}
    , min_internal_{converter.pre(min_include)}
    , max_internal_{converter.pre(max_include)}
    , converter_{converter} {};

    void set(const KeyExternal& min_include, const KeyExternal& max_include) {
        min_external_ = min_include;
        max_external_ = max_include;
        min_internal_ = converter_.get().pre(min_include);
        max_internal_ = converter_.get().pre(max_include);
    }

    template <typename T>
    [[nodiscard]] bool IsEntryValid(const KeyInternal& key, const T& /*value*/) const {
        auto point = converter_.get().post(key);
        for (dimension_t i = 0; i < DIM; ++i) {
            if (point[i] < min_external_[i] || point[i] > max_external_[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool IsNodeValid(const KeyInternal& prefix, std::uint32_t bits_to_ignore) const {
        if (bits_to_ignore >= (MAX_BIT_WIDTH<ScalarInternal> - 1)) {
            return true;
        }
        ScalarInternal node_min_bits = MAX_MASK<ScalarInternal> << bits_to_ignore;
        ScalarInternal node_max_bits = ~node_min_bits;

        for (dimension_t i = 0; i < DIM; ++i) {
            if ((prefix[i] | node_max_bits) < min_internal_[i] ||
                (prefix[i] & node_min_bits) > max_internal_[i]) {
                return false;
            }
        }
        return true;
    }

  private:
    KeyExternal min_external_;
    KeyExternal max_external_;
    KeyInternal min_internal_;
    KeyInternal max_internal_;
    std::reference_wrapper<const CONVERTER> converter_;
};

template <typename CONVERTER, typename DISTANCE>
class FilterSphere {
    using KeyExternal = typename CONVERTER::KeyExternal;
    using KeyInternal = typename CONVERTER::KeyInternal;
    using ScalarInternal = typename CONVERTER::ScalarInternal;
    static constexpr auto DIM = CONVERTER::DimInternal;

  public:
    template <typename DIST = DistanceEuclidean<CONVERTER::DimExternal>>
    FilterSphere(
        const KeyExternal& center,
        const double radius,
        const CONVERTER& converter,
        DIST&& distance_function = DIST())
    : center_external_{center}
    , center_internal_{converter.pre(center)}
    , radius_{radius}
    , converter_{converter}
    , distance_function_(std::forward<DIST>(distance_function)){};

    template <typename T>
    [[nodiscard]] bool IsEntryValid(const KeyInternal& key, const T&) const {
        KeyExternal point = converter_.get().post(key);
        return distance_function_(center_external_, point) <= radius_;
    }

    [[nodiscard]] bool IsNodeValid(const KeyInternal& prefix, std::uint32_t bits_to_ignore) const {

        if (bits_to_ignore >= (MAX_BIT_WIDTH<ScalarInternal> - 1)) {
            return true;
        }

        ScalarInternal node_min_bits = MAX_MASK<ScalarInternal> << bits_to_ignore;
        ScalarInternal node_max_bits = ~node_min_bits;

        KeyInternal closest_in_bounds;
        for (dimension_t i = 0; i < DIM; ++i) {
            ScalarInternal lo = prefix[i] & node_min_bits;
            ScalarInternal hi = prefix[i] | node_max_bits;
            closest_in_bounds[i] = std::clamp(center_internal_[i], lo, hi);
        }

        KeyExternal closest_point = converter_.get().post(closest_in_bounds);
        return distance_function_(center_external_, closest_point) <= radius_;
    }

  private:
    KeyExternal center_external_;
    KeyInternal center_internal_;
    double radius_;
    std::reference_wrapper<const CONVERTER> converter_;
    DISTANCE distance_function_;
};

template <
    typename CONV,
    typename DIST = DistanceEuclidean<CONV::DimExternal>,
    typename P = typename CONV::KeyExternal>
FilterSphere(const P&, double, const CONV&, DIST&& fn = DIST()) -> FilterSphere<CONV, DIST>;

template <typename CONVERTER>
class FilterBoxAABB {
    using KeyInternal = typename CONVERTER::KeyInternal;
    using ScalarInternal = typename CONVERTER::ScalarInternal;
    using QueryPoint = typename CONVERTER::QueryPointExternal;
    using QueryPointInternal = typename CONVERTER::QueryPointInternal;
    static constexpr auto DIM = CONVERTER::DimExternal;

  public:
    FilterBoxAABB(
        const QueryPoint& min_include, const QueryPoint& max_include, const CONVERTER& converter)
    : min_internal_{converter.pre_query(min_include)}
    , max_internal_{converter.pre_query(max_include)}
    , converter_{converter} {};

    void set(const QueryPoint& min_include, const QueryPoint& max_include) {
        min_internal_ = converter_.get().pre_query(min_include);
        max_internal_ = converter_.get().pre_query(max_include);
    }

    template <typename T>
    [[nodiscard]] bool IsEntryValid(const KeyInternal& key, const T& /*value*/) const {
        for (dimension_t i = 0; i < DIM; ++i) {
            if (key[i + DIM] < min_internal_[i] || key[i] > max_internal_[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool IsNodeValid(const KeyInternal& prefix, std::uint32_t bits_to_ignore) const {
        if (bits_to_ignore >= (MAX_BIT_WIDTH<ScalarInternal> - 1)) {
            return true;
        }
        ScalarInternal node_min_bits = MAX_MASK<ScalarInternal> << bits_to_ignore;
        ScalarInternal node_max_bits = ~node_min_bits;

        for (dimension_t i = 0; i < DIM; ++i) {
            if ((prefix[i] | node_max_bits) < min_internal_[i] ||
                (prefix[i + DIM] & node_min_bits) > max_internal_[i]) {
                return false;
            }
        }
        return true;
    }

  private:
    QueryPointInternal min_internal_;
    QueryPointInternal max_internal_;
    std::reference_wrapper<const CONVERTER> converter_;
};

template <typename CONVERTER, typename DISTANCE>
class FilterBoxSphere {
    using KeyInternal = typename CONVERTER::KeyInternal;
    using ScalarInternal = typename CONVERTER::ScalarInternal;
    using QueryPoint = typename CONVERTER::QueryPointExternal;
    using QueryPointInternal = typename CONVERTER::QueryPointInternal;
    static constexpr auto DIM = CONVERTER::DimExternal;

  public:
    template <typename DIST = DistanceEuclidean<DIM>>
    FilterBoxSphere(
        const QueryPoint& center,
        const double radius,
        const CONVERTER& converter,
        DIST&& distance_function = DIST())
    : center_external_{center}
    , center_internal_{converter.pre_query(center)}
    , radius_{radius}
    , converter_{converter}
    , distance_function_(std::forward<DIST>(distance_function)){};

    template <typename T>
    [[nodiscard]] bool IsEntryValid(const KeyInternal& key, const T&) const {
        QueryPointInternal closest_in_bounds;
        for (dimension_t i = 0; i < DIM; ++i) {
            closest_in_bounds[i] = std::clamp(center_internal_[i], key[i], key[i + DIM]);
        }
        QueryPoint closest_point = converter_.get().post_query(closest_in_bounds);
        return distance_function_(center_external_, closest_point) <= radius_;
    }

    [[nodiscard]] bool IsNodeValid(const KeyInternal& prefix, std::uint32_t bits_to_ignore) const {
        if (bits_to_ignore >= (MAX_BIT_WIDTH<ScalarInternal> - 1)) {
            return true;
        }

        ScalarInternal node_min_bits = MAX_MASK<ScalarInternal> << bits_to_ignore;
        ScalarInternal node_max_bits = ~node_min_bits;

        QueryPointInternal closest_in_bounds;
        for (dimension_t i = 0; i < DIM; ++i) {
            ScalarInternal lo = prefix[i] & node_min_bits;
            ScalarInternal hi = prefix[i + DIM] | node_max_bits;
            closest_in_bounds[i] = std::clamp(center_internal_[i], lo, hi);
        }
        QueryPoint closest_point = converter_.get().post_query(closest_in_bounds);
        return distance_function_(center_external_, closest_point) <= radius_;
    }

  private:
    QueryPoint center_external_;
    QueryPointInternal center_internal_;
    double radius_;
    std::reference_wrapper<const CONVERTER> converter_;
    DISTANCE distance_function_;
};

template <
    typename CONV,
    typename DIST = DistanceEuclidean<CONV::DimExternal>,
    typename P = typename CONV::KeyExternal>
FilterBoxSphere(const P&, double, const CONV&, DIST&& fn = DIST()) -> FilterBoxSphere<CONV, DIST>;

template <typename CONVERTER>
class FilterMultiMapAABB : public FilterAABB<CONVERTER> {
    using Key = typename CONVERTER::KeyExternal;
    using KeyInternal = typename CONVERTER::KeyInternal;

  public:
    FilterMultiMapAABB(const Key& min_include, const Key& max_include, CONVERTER& converter)
    : FilterAABB<CONVERTER>(min_include, max_include, converter){};

    template <typename ValueT>
    [[nodiscard]] inline bool IsBucketEntryValid(const KeyInternal&, const ValueT&) const noexcept {
        return true;
    }
};

template <typename CONVERTER, typename DISTANCE>
class FilterMultiMapSphere : public FilterSphere<CONVERTER, DISTANCE> {
    using Key = typename CONVERTER::KeyExternal;
    using KeyInternal = typename CONVERTER::KeyInternal;

  public:
    template <typename DIST = DistanceEuclidean<CONVERTER::DimExternal>>
    FilterMultiMapSphere(
        const Key& center, double radius, const CONVERTER& converter, DIST&& dist_fn = DIST())
    : FilterSphere<CONVERTER, DIST>(center, radius, converter, std::forward<DIST>(dist_fn)){};

    template <typename ValueT>
    [[nodiscard]] inline bool IsBucketEntryValid(const KeyInternal&, const ValueT&) const noexcept {
        return true;
    }
};

template <
    typename CONV,
    typename DIST = DistanceEuclidean<CONV::DimExternal>,
    typename P = typename CONV::KeyExternal>
FilterMultiMapSphere(const P&, double, const CONV&, DIST&& fn = DIST())
    -> FilterMultiMapSphere<CONV, DIST>;

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_FILTERS_H

