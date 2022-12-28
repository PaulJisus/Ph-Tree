#ifndef PHTREE_V16_FOR_EACH_H
#define PHTREE_V16_FOR_EACH_H

#include "common.h"
#include "iterator_with_parent.h"

namespace improbable::phtree::v16 {

template <typename T, typename CONVERT, typename CALLBACK, typename FILTER>
class ForEach {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using KeyInternal = typename CONVERT::KeyInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using EntryT = Entry<DIM, T, SCALAR>;

  public:
    template <typename CB, typename F>
    ForEach(const CONVERT* converter, CB&& callback, F&& filter)
    : converter_{converter}
    , callback_{std::forward<CB>(callback)}
    , filter_(std::forward<F>(filter)) {}

    void Traverse(const EntryT& entry) {
        assert(entry.IsNode());
        auto& entries = entry.GetNode().Entries();
        auto iter = entries.begin();
        auto end = entries.end();
        for (; iter != end; ++iter) {
            const auto& child = iter->second;
            const auto& child_key = child.GetKey();
            if (child.IsNode()) {
                if (filter_.IsNodeValid(child_key, child.GetNodePostfixLen() + 1)) {
                    Traverse(child);
                }
            } else {
                T& value = child.GetValue();
                if (filter_.IsEntryValid(child_key, value)) {
                    callback_(converter_->post(child_key), value);
                }
            }
        }
    }

    const CONVERT* converter_;
    CALLBACK callback_;
    FILTER filter_;
};
}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_FOR_EACH_H

