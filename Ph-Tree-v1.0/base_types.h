#ifndef PHTREE_COMMON_BASE_TYPES_H
#define PHTREE_COMMON_BASE_TYPES_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <sstream>

namespace improbable::phtree {

using scalar_64_t = int64_t;
using scalar_32_t = int32_t;
using scalar_16_t = int16_t;

using bit_width_t = uint32_t;

template <typename SCALAR>
static constexpr bit_width_t MAX_BIT_WIDTH =
    std::numeric_limits<SCALAR>::digits + std::numeric_limits<SCALAR>::is_signed;
template <typename SCALAR>
using bit_mask_t = typename std::make_unsigned<SCALAR>::type;
template <typename SCALAR>
static constexpr bit_mask_t<SCALAR> MAX_MASK = std::numeric_limits<bit_mask_t<SCALAR>>::max();
using dimension_t = size_t;
template<dimension_t DIM>
using hc_pos_dim_t = std::conditional_t<(DIM < 32), uint32_t, uint64_t>;
using hc_pos_64_t = uint64_t;

template <dimension_t DIM, typename SCALAR = scalar_64_t>
using PhPoint = std::array<SCALAR, DIM>;

template <dimension_t DIM>
using PhPointD = std::array<double, DIM>;

template <dimension_t DIM>
using PhPointF = std::array<float, DIM>;

template <dimension_t DIM, typename SCALAR = scalar_64_t>
class PhBox {
    using Point = PhPoint<DIM, SCALAR>;

  public:
    explicit PhBox() = default;

    PhBox(const PhBox<DIM, SCALAR>& orig) = default;

    PhBox(const std::array<SCALAR, DIM>& min, const std::array<SCALAR, DIM>& max)
    : min_{min}, max_{max} {}

    [[nodiscard]] const Point& min() const {
        return min_;
    }

    [[nodiscard]] const Point& max() const {
        return max_;
    }

    [[nodiscard]] Point& min() {
        return min_;
    }

    [[nodiscard]] Point& max() {
        return max_;
    }

    void min(const std::array<SCALAR, DIM>& new_min) {
        min_ = new_min;
    }

    void max(const std::array<SCALAR, DIM>& new_max) {
        max_ = new_max;
    }

    auto operator==(const PhBox<DIM, SCALAR>& other) const -> bool {
        return min_ == other.min_ && max_ == other.max_;
    }

    auto operator!=(const PhBox<DIM, SCALAR>& other) const -> bool {
        return !(*this == other);
    }

  private:
    Point min_;
    Point max_;
};

template <dimension_t DIM>
using PhBoxD = PhBox<DIM, double>;

template <dimension_t DIM>
using PhBoxF = PhBox<DIM, float>;

template <dimension_t DIM, typename SCALAR>
std::ostream& operator<<(std::ostream& os, const PhPoint<DIM, SCALAR>& data) {
    assert(DIM >= 1);
    os << "[";
    for (dimension_t i = 0; i < DIM - 1; ++i) {
        os << data[i] << ",";
    }
    os << data[DIM - 1] << "]";
    return os;
}

template <dimension_t DIM, typename SCALAR>
std::ostream& operator<<(std::ostream& os, const PhBox<DIM, SCALAR>& data) {
    os << data.min() << ":" << data.max();
    return os;
}

template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

}

namespace std {
template <improbable::phtree::dimension_t DIM, typename SCALAR>
struct hash<improbable::phtree::PhPoint<DIM, SCALAR>> {
    size_t operator()(const improbable::phtree::PhPoint<DIM, SCALAR>& x) const {
        std::size_t hash_val = 0;
        for (improbable::phtree::dimension_t i = 0; i < DIM; ++i) {
            improbable::phtree::hash_combine(hash_val, x[i]);
        }
        return hash_val;
    }
};
template <improbable::phtree::dimension_t DIM, typename SCALAR>
struct hash<improbable::phtree::PhBox<DIM, SCALAR>> {
    size_t operator()(const improbable::phtree::PhBox<DIM, SCALAR>& x) const {
        std::size_t hash_val = 0;
        for (improbable::phtree::dimension_t i = 0; i < DIM; ++i) {
            improbable::phtree::hash_combine(hash_val, x.min()[i]);
            improbable::phtree::hash_combine(hash_val, x.max()[i]);
        }
        return hash_val;
    }
};
}
#endif  // PHTREE_COMMON_BASE_TYPES_H
