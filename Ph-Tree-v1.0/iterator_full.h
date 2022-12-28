#ifndef PHTREE_V16_ITERATOR_FULL_H
#define PHTREE_V16_ITERATOR_FULL_H

#include "common.h"
#include "iterator_base.h"

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename SCALAR>
class Node;

template <typename T, typename CONVERT, typename FILTER>
class IteratorFull : public IteratorWithFilter<T, CONVERT, FILTER> {
    static constexpr dimension_t DIM = CONVERT::DimInternal;
    using SCALAR = typename CONVERT::ScalarInternal;
    using NodeT = Node<DIM, T, SCALAR>;
    using EntryT = typename IteratorWithFilter<T, CONVERT, FILTER>::EntryT;

  public:
    template <typename F>
    IteratorFull(const EntryT& root, const CONVERT* converter, F&& filter)
    : IteratorWithFilter<T, CONVERT, F>(converter, std::forward<F>(filter))
    , stack_{}
    , stack_size_{0} {
        PrepareAndPush(root.GetNode());
        FindNextElement();
    }

    IteratorFull& operator++() noexcept {
        FindNextElement();
        return *this;
    }

    IteratorFull operator++(int) noexcept {
        IteratorFull iterator(*this);
        ++(*this);
        return iterator;
    }

  private:
    void FindNextElement() noexcept {
        while (!IsEmpty()) {
            auto* p = &Peek();
            while (*p != PeekEnd()) {
                auto& candidate = (*p)->second;
                ++(*p);
                if (this->ApplyFilter(candidate)) {
                    if (candidate.IsNode()) {
                        p = &PrepareAndPush(candidate.GetNode());
                    } else {
                        this->SetCurrentResult(&candidate);
                        return;
                    }
                }
            }
            Pop();
        }
        this->SetFinished();
    }

    auto& PrepareAndPush(const NodeT& node) {
        assert(stack_size_ < stack_.size() - 1);
        stack_[stack_size_].first = node.Entries().cbegin();
        stack_[stack_size_].second = node.Entries().end();
        ++stack_size_;
        return stack_[stack_size_ - 1].first;
    }

    auto& Peek() noexcept {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1].first;
    }

    auto& PeekEnd() noexcept {
        assert(stack_size_ > 0);
        return stack_[stack_size_ - 1].second;
    }

    auto& Pop() noexcept {
        assert(stack_size_ > 0);
        return stack_[--stack_size_].first;
    }

    bool IsEmpty() noexcept {
        return stack_size_ == 0;
    }

    std::array<
        std::pair<EntryIteratorC<DIM, EntryT>, EntryIteratorC<DIM, EntryT>>,
        MAX_BIT_WIDTH<SCALAR>>
        stack_;
    size_t stack_size_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_ITERATOR_FULL_H

