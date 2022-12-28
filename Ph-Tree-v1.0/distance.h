#ifndef PHTREE_COMMON_DISTANCES_H
#define PHTREE_COMMON_DISTANCES_H

#include "common.h"
#include "converter.h"
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>

namespace improbable::phtree {

template <dimension_t DIM>
struct DistanceEuclidean {
    double operator()(const PhPoint<DIM>& v1, const PhPoint<DIM>& v2) const {
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            assert(
                (v1[i] >= 0) != (v2[i] >= 0) ||
                double(v1[i]) - double(v2[i]) <
                    double(std::numeric_limits<decltype(v1[i] - v2[i])>::max()));
            double d2 = double(v1[i] - v2[i]);
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };

    double operator()(const PhPointD<DIM>& p1, const PhPointD<DIM>& p2) const {
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            double d2 = p1[i] - p2[i];
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };

    double operator()(const PhPointF<DIM>& v1, const PhPointF<DIM>& v2) const {
        double sum2 = 0;
        for (dimension_t i = 0; i < DIM; i++) {
            double d2 = double(v1[i] - v2[i]);
            sum2 += d2 * d2;
        }
        return sqrt(sum2);
    };
};

template <dimension_t DIM>
struct DistanceL1 {
    double operator()(const PhPoint<DIM>& v1, const PhPoint<DIM>& v2) const {
        double sum = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            assert(
                (v1[i] >= 0) != (v2[i] >= 0) ||
                double(v1[i]) - double(v2[i]) <
                    double(std::numeric_limits<decltype(v1[i] - v2[i])>::max()));
            sum += std::abs(double(v1[i] - v2[i]));
        }
        return sum;
    };

    double operator()(const PhPointD<DIM>& v1, const PhPointD<DIM>& v2) const {
        double sum = 0;
        for (dimension_t i = 0; i < DIM; ++i) {
            sum += std::abs(v1[i] - v2[i]);
        }
        return sum;
    };
};

}  // namespace improbable::phtree

#endif  // PHTREE_COMMON_DISTANCES_H
