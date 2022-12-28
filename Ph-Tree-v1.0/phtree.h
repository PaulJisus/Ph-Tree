#ifndef PHTREE_PHTREE_H
#define PHTREE_PHTREE_H

#include "common.h"
#include "phtree_v16.h"

namespace improbable::phtree {

template <dimension_t DIM, typename T, typename CONVERTER = ConverterNoOp<DIM, scalar_64_t>>
class PhTree {
    friend PhTreeDebugHelper;
    using Key = typename CONVERTER::KeyExternal;
    static constexpr dimension_t DimInternal = CONVERTER::DimInternal;

    using DEFAULT_QUERY_TYPE =
        typename std::conditional<(DIM == DimInternal), QueryPoint, QueryIntersect>::type;

  public:
    using QueryBox = typename CONVERTER::QueryBoxExternal;

    template <typename CONV = CONVERTER>
    explicit PhTree(CONV&& converter = CONV()) : tree_{&converter_}, converter_{converter} {}

    PhTree(const PhTree& other) = delete;
    PhTree& operator=(const PhTree& other) = delete;
    PhTree(PhTree&& other) noexcept = default;
    PhTree& operator=(PhTree&& other) noexcept = default;
    ~PhTree() noexcept = default;

    template <typename... Args>
    std::pair<T&, bool> emplace(const Key& key, Args&&... args) {
        return tree_.try_emplace(converter_.pre(key), std::forward<Args>(args)...);
    }

    template <typename ITERATOR, typename... Args>
    std::pair<T&, bool> emplace_hint(const ITERATOR& iterator, const Key& key, Args&&... args) {
        return tree_.try_emplace(iterator, converter_.pre(key), std::forward<Args>(args)...);
    }

    std::pair<T&, bool> insert(const Key& key, const T& value) {
        return tree_.insert(converter_.pre(key), value);
    }

    template <typename... Args>
    std::pair<T&, bool> try_emplace(const Key& key, Args&&... args) {
        return tree_.try_emplace(converter_.pre(key), std::forward<Args>(args)...);
    }

    template <typename ITERATOR, typename... Args>
    std::pair<T&, bool> try_emplace(const ITERATOR& iterator, const Key& key, Args&&... args) {
        return tree_.try_emplace(iterator, converter_.pre(key), std::forward<Args>(args)...);
    }

    T& operator[](const Key& key) {
        return tree_[converter_.pre(key)];
    }

    size_t count(const Key& key) const {
        return tree_.count(converter_.pre(key));
    }

    auto find(const Key& key) const {
        return tree_.find(converter_.pre(key));
    }

    size_t erase(const Key& key) {
        return tree_.erase(converter_.pre(key));
    }

    template <typename ITERATOR>
    size_t erase(const ITERATOR& iterator) {
        return tree_.erase(iterator);
    }

    auto relocate(const Key& old_key, const Key& new_key) {
        return tree_.relocate_if(
            converter_.pre(old_key), converter_.pre(new_key), [](const T&) { return true; });
    }

    template <typename PRED>
    auto relocate_if(const Key& old_key, const Key& new_key, PRED&& predicate) {
        return tree_.relocate_if(
            converter_.pre(old_key), converter_.pre(new_key), std::forward<PRED>(predicate));
    }

    template <typename CALLBACK, typename FILTER = FilterNoOp>
    void for_each(CALLBACK&& callback, FILTER&& filter = FILTER()) const {
        tree_.for_each(std::forward<CALLBACK>(callback), std::forward<FILTER>(filter));
    }

    template <
        typename CALLBACK,
        typename FILTER = FilterNoOp,
        typename QUERY_TYPE = DEFAULT_QUERY_TYPE>
    void for_each(
        QueryBox query_box,
        CALLBACK&& callback,
        FILTER&& filter = FILTER(),
        QUERY_TYPE query_type = QUERY_TYPE()) const {
        tree_.for_each(
            query_type(converter_.pre_query(query_box)),
            std::forward<CALLBACK>(callback),
            std::forward<FILTER>(filter));
    }

    template <typename FILTER = FilterNoOp>
    auto begin(FILTER&& filter = FILTER()) const {
        return tree_.begin(std::forward<FILTER>(filter));
    }

    template <typename FILTER = FilterNoOp, typename QUERY_TYPE = DEFAULT_QUERY_TYPE>
    auto begin_query(
        const QueryBox& query_box,
        FILTER&& filter = FILTER(),
        QUERY_TYPE query_type = DEFAULT_QUERY_TYPE()) const {
        return tree_.begin_query(
            query_type(converter_.pre_query(query_box)), std::forward<FILTER>(filter));
    }

    template <
        typename DISTANCE,
        typename FILTER = FilterNoOp,
        dimension_t DUMMY = DIM,
        typename std::enable_if<(DUMMY == DimInternal), int>::type = 0>
    auto begin_knn_query(
        size_t min_results,
        const Key& center,
        DISTANCE&& distance_function = DISTANCE(),
        FILTER&& filter = FILTER()) const {
        return tree_.begin_knn_query(
            min_results,
            converter_.pre(center),
            std::forward<DISTANCE>(distance_function),
            std::forward<FILTER>(filter));
    }

    auto end() const {
        return tree_.end();
    }

    void clear() {
        tree_.clear();
    }

    [[nodiscard]] size_t size() const {
        return tree_.size();
    }

    [[nodiscard]] bool empty() const {
        return tree_.empty();
    }

    [[nodiscard]] const CONVERTER& converter() const {
        return converter_;
    }

  private:
    const auto& GetInternalTree() const {
        return tree_;
    }

    void CheckConsistencyExternal() const {
        [[maybe_unused]] size_t n = 0;
        for ([[maybe_unused]] const auto& entry : tree_) {
            ++n;
        }
        assert(n == size());
    }

    v16::PhTreeV16<DimInternal, T, CONVERTER> tree_;
    CONVERTER converter_;
};

template <dimension_t DIM, typename T, typename CONVERTER = ConverterIEEE<DIM>>
using PhTreeD = PhTree<DIM, T, CONVERTER>;

template <dimension_t DIM, typename T, typename CONVERTER = ConverterFloatIEEE<DIM>>
using PhTreeF = PhTree<DIM, T, CONVERTER>;

template <dimension_t DIM, typename T, typename CONVERTER_BOX>
using PhTreeBox = PhTree<DIM, T, CONVERTER_BOX>;

template <dimension_t DIM, typename T, typename CONVERTER_BOX = ConverterBoxIEEE<DIM>>
using PhTreeBoxD = PhTreeBox<DIM, T, CONVERTER_BOX>;

template <dimension_t DIM, typename T, typename CONVERTER_BOX = ConverterBoxFloatIEEE<DIM>>
using PhTreeBoxF = PhTreeBox<DIM, T, CONVERTER_BOX>;

}  // namespace improbable::phtree

#endif  // PHTREE_PHTREE_H

