#ifndef PHTREE_V16_QUERY_KNN_HS_H
#define PHTREE_V16_QUERY_KNN_HS_H

#include "iterator_base.h"
#include "common.h"
#include <queue>

namespace improbable::phtree::v16 {

namespace {
template <dimension_t DIM, typename T, typename SCALAR>
using EntryDist = std::pair<double, const Entry<DIM, T, SCALAR>*>;

template <typename ENTRY>
struct CompareEntryDistByDistance {
    bool operator()(const ENTRY& left, const ENTRY& right) const {
        return left.first > right.first;
    };
};
}

template <typename T, typename CONVERT, typename DISTANCE, typename FILTER>
class IteratorKnnHS : public IteratorWithFilter<T, CONVERT, FILTER> {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using KeyExternal = typename CONVERT::KeyExternal;
    using KeyInternal = typename CONVERT::KeyInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using EntryT = typename IteratorWithFilter<T, CONVERT, FILTER>::EntryT;
    using EntryDistT = EntryDist<DIM, T, SCALAR>;

  public:
    template <typename DIST, typename F>
    explicit IteratorKnnHS(
        const EntryT& root,
        size_t min_results,
        const KeyInternal& center,
        const CONVERT* converter,
        DIST&& dist,
        F&& filter)
    : IteratorWithFilter<T, CONVERT, F>(converter, std::forward<F>(filter))
    , center_{center}
    , center_post_{converter->post(center)}
    , current_distance_{std::numeric_limits<double>::max()}
    , num_found_results_(0)
    , num_requested_results_(min_results)
    , distance_(std::forward<DIST>(dist)) {
        if (min_results <= 0 || root.GetNode().GetEntryCount() == 0) {
            this->SetFinished();
            return;
        }

        queue_.emplace(0, &root);
        FindNextElement();
    }

    [[nodiscard]] double distance() const {
        return current_distance_;
    }

    IteratorKnnHS& operator++() noexcept {
        FindNextElement();
        return *this;
    }

    IteratorKnnHS operator++(int) noexcept {
        IteratorKnnHS iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    void FindNextElement() {
        while (num_found_results_ < num_requested_results_ && !queue_.empty()) {
            auto& candidate = queue_.top();
            auto* o = candidate.second;
            if (!o->IsNode()) {
                ++num_found_results_;
                this->SetCurrentResult(o);
                current_distance_ = candidate.first;
                queue_.pop();
                return;
            } else {
                auto& node = o->GetNode();
                queue_.pop();
                for (auto& entry : node.Entries()) {
                    auto& e2 = entry.second;
                    if (this->ApplyFilter(e2)) {
                        if (e2.IsNode()) {
                            double d = DistanceToNode(e2.GetKey(), e2.GetNodePostfixLen() + 1);
                            queue_.emplace(d, &e2);
                        } else {
                            double d = distance_(center_post_, this->post(e2.GetKey()));
                            queue_.emplace(d, &e2);
                        }
                    }
                }
            }
        }
        this->SetFinished();
        current_distance_ = std::numeric_limits<double>::max();
    }

    double DistanceToNode(const KeyInternal& prefix, std::uint32_t bits_to_ignore) {
        assert(bits_to_ignore < MAX_BIT_WIDTH<SCALAR>);
        SCALAR mask_min = MAX_MASK<SCALAR> << bits_to_ignore;
        SCALAR mask_max = ~mask_min;
        KeyInternal buf;
        for (dimension_t i = 0; i < DIM; ++i) {
            SCALAR min = prefix[i] & mask_min;
            SCALAR max = prefix[i] | mask_max;
            buf[i] = min > center_[i] ? min : (max < center_[i] ? max : center_[i]);
        }
        return distance_(center_post_, this->post(buf));
    }

  private:
    const KeyInternal center_;
    const KeyExternal center_post_;
    double current_distance_;
    std::priority_queue<EntryDistT, std::vector<EntryDistT>, CompareEntryDistByDistance<EntryDistT>>
        queue_;
    size_t num_found_results_;
    size_t num_requested_results_;
    DISTANCE distance_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_QUERY_KNN_HS_H


