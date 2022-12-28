#ifndef PHTREE_COMMON_CONVERTER_H
#define PHTREE_COMMON_CONVERTER_H

#include "common.h"
#include <cstring>

namespace improbable::phtree {

class ScalarConverterIEEE {
    static_assert(std::is_same<scalar_64_t, std::int64_t>());

  public:
    static scalar_64_t pre(double value) {
        scalar_64_t r;
        memcpy(&r, &value, sizeof(r));
        return r >= 0 ? r : r ^ 0x7FFFFFFFFFFFFFFFL;
    }

    static double post(scalar_64_t value) {
        auto v = value >= 0 ? value : value ^ 0x7FFFFFFFFFFFFFFFL;
        double r;
        memcpy(&r, &v, sizeof(r));
        return r;
    }

    static scalar_32_t pre(float value) {
        scalar_32_t r;
        memcpy(&r, &value, sizeof(r));
        return r >= 0 ? r : r ^ 0x7FFFFFFFL;
    }

    static float post(scalar_32_t value) {
        auto v = value >= 0 ? value : value ^ 0x7FFFFFFFL;
        float r;
        memcpy(&r, &v, sizeof(r));
        return r;
    }
};

template <int64_t NUMERATOR, int64_t DENOMINATOR>
class ScalarConverterMultiply {
    static_assert(std::is_same<scalar_64_t, std::int64_t>());
    static_assert(NUMERATOR != 0);
    static_assert(DENOMINATOR != 0);
    static constexpr double MULTIPLY = NUMERATOR / (double)DENOMINATOR;
    static constexpr double DIVIDE = DENOMINATOR / (double)NUMERATOR;

  public:
    static scalar_64_t pre(double value) {
        return static_cast<scalar_64_t>(value * MULTIPLY);
    }

    static double post(scalar_64_t value) {
        return value * DIVIDE;
    }

    static scalar_32_t pre(float value) {
        return  static_cast<scalar_32_t>(value * MULTIPLY);
    }

    static float post(scalar_32_t value) {
        return value * DIVIDE;
    }
};

template <
    dimension_t DIM_EXTERNAL,
    dimension_t DIM_INTERNAL,
    typename SCALAR_EXTERNAL,
    typename SCALAR_INTERNAL,
    typename KEY_EXTERNAL,
    typename QUERY_POINT_EXTERNAL = PhBox<DIM_EXTERNAL, SCALAR_EXTERNAL>>
class ConverterBase {
  public:
    static constexpr dimension_t DimExternal = DIM_EXTERNAL;
    static constexpr dimension_t DimInternal = DIM_INTERNAL;
    using ScalarExternal = SCALAR_EXTERNAL;
    using ScalarInternal = SCALAR_INTERNAL;
    using KeyExternal = KEY_EXTERNAL;
    using KeyInternal = PhPoint<DIM_INTERNAL, SCALAR_INTERNAL>;
    using QueryBoxExternal = QUERY_POINT_EXTERNAL;
    using QueryBoxInternal = PhBox<DIM_EXTERNAL, SCALAR_INTERNAL>;
    using QueryPointExternal = PhPoint<DIM_EXTERNAL, SCALAR_EXTERNAL>;
    using QueryPointInternal = PhPoint<DIM_EXTERNAL, SCALAR_INTERNAL>;
};

template <dimension_t DIM, typename SCALAR_EXTERNAL, typename SCALAR_INTERNAL>
using ConverterPointBase =
    ConverterBase<DIM, DIM, SCALAR_EXTERNAL, SCALAR_INTERNAL, PhPoint<DIM, SCALAR_EXTERNAL>>;

template <dimension_t DIM, typename SCALAR_EXTERNAL, typename SCALAR_INTERNAL>
using ConverterBoxBase =
    ConverterBase<DIM, 2 * DIM, SCALAR_EXTERNAL, SCALAR_INTERNAL, PhBox<DIM, SCALAR_EXTERNAL>>;

template <dimension_t DIM, typename SCALAR>
struct ConverterNoOp : public ConverterPointBase<DIM, SCALAR, SCALAR> {
    using BASE = ConverterPointBase<DIM, SCALAR, SCALAR>;
    using Point = typename BASE::KeyExternal;
    using PointInternal = typename BASE::KeyInternal;

    constexpr const PointInternal& pre(const Point& point) const {
        return point;
    }

    constexpr const Point& post(const PointInternal& point) const {
        return point;
    }

    constexpr const PhBox<DIM, SCALAR>& pre_query(const PhBox<DIM, SCALAR>& box) const {
        return box;
    }
};

template <
    dimension_t DIM,
    typename SCALAR_EXTERNAL,
    typename SCALAR_INTERNAL,
    typename CONVERT = ScalarConverterIEEE>
class SimplePointConverter : public ConverterPointBase<DIM, SCALAR_EXTERNAL, SCALAR_INTERNAL> {
    using BASE = ConverterPointBase<DIM, SCALAR_EXTERNAL, SCALAR_INTERNAL>;

  public:
    using Point = typename BASE::KeyExternal;
    using PointInternal = typename BASE::KeyInternal;
    using QueryBox = typename BASE::QueryBoxExternal;

    static_assert(std::is_same<Point, PhPoint<DIM, SCALAR_EXTERNAL>>::value);
    static_assert(std::is_same<PointInternal, PhPoint<DIM, SCALAR_INTERNAL>>::value);

  public:
    explicit SimplePointConverter(const CONVERT converter = CONVERT()) : converter_{converter} {};

    PointInternal pre(const Point& point) const {
        PointInternal out;
        for (dimension_t i = 0; i < DIM; ++i) {
            out[i] = converter_.pre(point[i]);
        }
        return out;
    }

    Point post(const PointInternal& point) const {
        Point out;
        for (dimension_t i = 0; i < DIM; ++i) {
            out[i] = converter_.post(point[i]);
        }
        return out;
    }

    PhBox<DIM, SCALAR_INTERNAL> pre_query(const QueryBox& query_box) const {
        return {pre(query_box.min()), pre(query_box.max())};
    }

  private:
    CONVERT converter_;
};

template <
    dimension_t DIM,
    typename SCALAR_EXTERNAL,
    typename SCALAR_INTERNAL,
    typename CONVERT = ScalarConverterIEEE>
class SimpleBoxConverter : public ConverterBoxBase<DIM, SCALAR_EXTERNAL, SCALAR_INTERNAL> {
    using BASE = ConverterBoxBase<DIM, SCALAR_EXTERNAL, SCALAR_INTERNAL>;

  public:
    using Box = typename BASE::KeyExternal;
    using PointInternal = typename BASE::KeyInternal;
    using QueryBox = typename BASE::QueryBoxExternal;
    using QueryBoxInternal = typename BASE::QueryBoxInternal;
    using QueryPoint = typename BASE::QueryPointExternal;
    using QueryPointInternal = typename BASE::QueryPointInternal;

    static_assert(std::is_same<Box, PhBox<DIM, SCALAR_EXTERNAL>>::value);
    static_assert(std::is_same<PointInternal, PhPoint<2 * DIM, SCALAR_INTERNAL>>::value);

  public:
    explicit SimpleBoxConverter(const CONVERT converter = CONVERT()) : converter_{converter} {};
    PointInternal pre(const Box& box) const {
        PointInternal out;
        for (dimension_t i = 0; i < DIM; ++i) {
            out[i] = converter_.pre(box.min()[i]);
            out[i + DIM] = converter_.pre(box.max()[i]);
        }
        return out;
    }

    Box post(const PointInternal& point) const {
        Box out;
        for (dimension_t i = 0; i < DIM; ++i) {
            out.min()[i] = converter_.post(point[i]);
            out.max()[i] = converter_.post(point[i + DIM]);
        }
        return out;
    }

    auto pre_query(const QueryBox& query_box) const {
        QueryBoxInternal out;
        auto& min = out.min();
        auto& max = out.max();
        for (dimension_t i = 0; i < DIM; ++i) {
            min[i] = converter_.pre(query_box.min()[i]);
            max[i] = converter_.pre(query_box.max()[i]);
        }
        return out;
    }

    auto pre_query(const QueryPoint& query_point) const {
        QueryPointInternal out;
        for (dimension_t i = 0; i < DIM; ++i) {
            out[i] = converter_.pre(query_point[i]);
        }
        return out;
    }

    auto post_query(const QueryPointInternal& query_point) const {
        QueryPoint out;
        for (dimension_t i = 0; i < DIM; ++i) {
            out[i] = converter_.post(query_point[i]);
        }
        return out;
    }

  private:
    CONVERT converter_;
};

template <dimension_t DIM>
using ConverterIEEE = SimplePointConverter<DIM, double, scalar_64_t, ScalarConverterIEEE>;

template <dimension_t DIM>
using ConverterFloatIEEE = SimplePointConverter<DIM, float, scalar_32_t, ScalarConverterIEEE>;

template <dimension_t DIM>
using ConverterBoxIEEE = SimpleBoxConverter<DIM, double, scalar_64_t, ScalarConverterIEEE>;

template <dimension_t DIM>
using ConverterBoxFloatIEEE = SimpleBoxConverter<DIM, float, scalar_32_t, ScalarConverterIEEE>;

template <dimension_t DIM, int64_t NUMERATOR, int64_t DENOMINATOR>
using ConverterMultiply =
    SimplePointConverter<DIM, double, scalar_64_t, ScalarConverterMultiply<NUMERATOR, DENOMINATOR>>;

template <dimension_t DIM, int64_t NUMERATOR, int64_t DENOMINATOR>
using ConverterBoxMultiply =
    SimpleBoxConverter<DIM, double, scalar_64_t, ScalarConverterMultiply<NUMERATOR, DENOMINATOR>>;

struct QueryPoint {
    template <dimension_t DIM, typename SCALAR_INTERNAL>
    auto operator()(const PhBox<DIM, SCALAR_INTERNAL>& query_box) {
        return query_box;
    }
};

struct QueryIntersect {
    template <dimension_t DIM, typename SCALAR_INTERNAL>
    auto operator()(const PhBox<DIM, SCALAR_INTERNAL>& query_box) {
        auto neg_inf = std::numeric_limits<SCALAR_INTERNAL>::min();
        auto pos_inf = std::numeric_limits<SCALAR_INTERNAL>::max();
        PhBox<2 * DIM, SCALAR_INTERNAL> min_max;
        auto& min = min_max.min();
        auto& max = min_max.max();
        for (dimension_t i = 0; i < DIM; i++) {
            min[i] = neg_inf;
            min[i + DIM] = query_box.min()[i];
            max[i] = query_box.max()[i];
            max[i + DIM] = pos_inf;
        }
        return min_max;
    }
};

struct QueryInclude {
    template <dimension_t DIM, typename SCALAR_INTERNAL>
    auto operator()(const PhBox<DIM, SCALAR_INTERNAL>& query_box) {
        PhBox<2 * DIM, SCALAR_INTERNAL> min_max;
        auto& min = min_max.min();
        auto& max = min_max.max();
        for (dimension_t i = 0; i < DIM; i++) {
            min[i] = query_box.min()[i];
            min[i + DIM] = query_box.min()[i];
            max[i] = query_box.max()[i];
            max[i + DIM] = query_box.max()[i];
        }
        return min_max;
    }
};
}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_CONVERTER_H

