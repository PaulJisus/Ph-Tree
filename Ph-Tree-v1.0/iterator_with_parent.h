#ifndef PHTREE_V16_ITERATOR_SIMPLE_H
#define PHTREE_V16_ITERATOR_SIMPLE_H

#include "common.h"
#include "iterator_base.h"

namespace improbable::phtree::v16 {

template <typename T, typename CONVERT>
class IteratorWithParent : public IteratorWithFilter<T, CONVERT> {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using EntryT = typename IteratorWithFilter<T, CONVERT>::EntryT;
    friend PhTreeV16<DIM, T, CONVERT>;

  public:
    explicit IteratorWithParent(
        const EntryT* current_result,
        const EntryT* current_node,
        const EntryT* parent_node,
        const CONVERT* converter) noexcept
    : IteratorWithFilter<T, CONVERT>(current_result, converter)
    , current_node_{current_node}
    , parent_node_{parent_node} {}

    IteratorWithParent& operator++() {
        this->SetFinished();
        return *this;
    }

    IteratorWithParent operator++(int) {
        IteratorWithParent iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    EntryT* GetNodeEntry() const {
        return const_cast<EntryT*>(current_node_);
    }

    EntryT* GetParentNodeEntry() const {
        return const_cast<EntryT*>(parent_node_);
    }

    const EntryT* current_node_;
    const EntryT* parent_node_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ITERATOR_SIMPLE_H

