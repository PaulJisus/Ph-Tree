#ifndef PHTREE_PHTREE_MULTIMAP_H
#define PHTREE_PHTREE_MULTIMAP_H

#include "b_plus_tree_hash_map.h"
#include "common.h"
#include "phtree_v16.h"
#include <unordered_set>

namespace improbable::phtree {

namespace {

template <typename PHTREE>
class IteratorBase {
    friend PHTREE;
    using T = typename PHTREE::ValueType;

  protected:
    using BucketIterType = typename PHTREE::BucketIterType;

  public:
    explicit IteratorBase() noexcept : current_value_ptr_{nullptr} {}

    T& operator*() const noexcept {
        assert(current_value_ptr_);
        return const_cast<T&>(*current_value_ptr_);
    }

    T* operator->() const noexcept {
        assert(current_value_ptr_);
        return const_cast<T*>(current_value_ptr_);
    }

    friend bool operator==(
        const IteratorBase<PHTREE>& left, const IteratorBase<PHTREE>& right) noexcept {
        return left.current_value_ptr_ == right.current_value_ptr_;
    }

    friend bool operator!=(
        const IteratorBase<PHTREE>& left, const IteratorBase<PHTREE>& right) noexcept {
        return left.current_value_ptr_ != right.current_value_ptr_;
    }

  protected:
    void SetFinished() noexcept {
        current_value_ptr_ = nullptr;
    }

    void SetCurrentValue(const T* current_value_ptr) noexcept {
        current_value_ptr_ = current_value_ptr;
    }

  private:
    const T* current_value_ptr_;
};

template <typename ITERATOR_PH, typename PHTREE>
class IteratorNormal : public IteratorBase<PHTREE> {
    friend PHTREE;
    using BucketIterType = typename IteratorBase<PHTREE>::BucketIterType;

  public:
    explicit IteratorNormal() noexcept : IteratorBase<PHTREE>(), iter_ph_{}, iter_bucket_{} {}

    template <typename ITER_PH, typename BucketIterType>
    IteratorNormal(ITER_PH&& iter_ph, BucketIterType&& iter_bucket) noexcept
    : IteratorBase<PHTREE>()
    , iter_ph_{std::forward<ITER_PH>(iter_ph)}
    , iter_bucket_{std::forward<BucketIterType>(iter_bucket)} {
        FindNextElement();
    }

    IteratorNormal& operator++() noexcept {
        ++iter_bucket_;
        FindNextElement();
        return *this;
    }

    IteratorNormal operator++(int) noexcept {
        IteratorNormal iterator(*this);
        ++(*this);
        return iterator;
    }

    auto first() const {
        return iter_ph_.first();
    }

  protected:
    auto& GetIteratorOfBucket() const noexcept {
        return iter_bucket_;
    }

    auto& GetIteratorOfPhTree() const noexcept {
        return iter_ph_;
    }

  private:
    void FindNextElement() {
        while (!iter_ph_.IsEnd()) {
            while (iter_bucket_ != iter_ph_->end()) {
                if (iter_ph_.__Filter().IsBucketEntryValid(
                        iter_ph_.GetEntry()->GetKey(), *iter_bucket_)) {
                    this->SetCurrentValue(&(*iter_bucket_));
                    return;
                }
                ++iter_bucket_;
            }
            ++iter_ph_;
            if (!iter_ph_.IsEnd()) {
                iter_bucket_ = iter_ph_->begin();
            }
        }
        this->SetFinished();
    }

    ITERATOR_PH iter_ph_;
    BucketIterType iter_bucket_;
};

template <typename ITERATOR_PH, typename PHTREE>
class IteratorKnn : public IteratorNormal<ITERATOR_PH, PHTREE> {
  public:
    template <typename ITER_PH, typename BucketIterType>
    IteratorKnn(ITER_PH&& iter_ph, BucketIterType&& iter_bucket) noexcept
    : IteratorNormal<ITER_PH, PHTREE>(
          std::forward<ITER_PH>(iter_ph), std::forward<BucketIterType>(iter_bucket)) {}

    [[nodiscard]] double distance() const noexcept {
        return this->GetIteratorOfPhTree().distance();
    }
};

}
template <
    dimension_t DIM,
    typename T,
    typename CONVERTER = ConverterNoOp<DIM, scalar_64_t>,
    typename BUCKET = b_plus_tree_hash_set<T>,
    bool POINT_KEYS = true,
    typename DEFAULT_QUERY_TYPE = QueryPoint>
class PhTreeMultiMap {
    using KeyInternal = typename CONVERTER::KeyInternal;
    using Key = typename CONVERTER::KeyExternal;
    static constexpr dimension_t DimInternal = CONVERTER::DimInternal;
    using PHTREE = PhTreeMultiMap<DIM, T, CONVERTER, BUCKET, POINT_KEYS, DEFAULT_QUERY_TYPE>;
    using ValueType = T;
    using BucketIterType = decltype(std::declval<BUCKET>().begin());
    using EndType = decltype(std::declval<v16::PhTreeV16<DimInternal, BUCKET, CONVERTER>>().end());

    friend PhTreeDebugHelper;
    friend IteratorBase<PHTREE>;

  public:
    using QueryBox = typename CONVERTER::QueryBoxExternal;

    explicit PhTreeMultiMap(CONVERTER converter = CONVERTER())
    : tree_{&converter_}, converter_{converter}, size_{0} {}

    PhTreeMultiMap(const PhTreeMultiMap& other) = delete;
    PhTreeMultiMap& operator=(const PhTreeMultiMap& other) = delete;
    PhTreeMultiMap(PhTreeMultiMap&& other) noexcept = default;
    PhTreeMultiMap& operator=(PhTreeMultiMap&& other) noexcept = default;
    ~PhTreeMultiMap() noexcept = default;

    template <typename... Args>
    std::pair<T&, bool> emplace(const Key& key, Args&&... args) {
        auto& outer_iter = tree_.try_emplace(converter_.pre(key)).first;
        auto bucket_iter = outer_iter.emplace(std::forward<Args>(args)...);
        size_ += bucket_iter.second ? 1 : 0;
        return {const_cast<T&>(*bucket_iter.first), bucket_iter.second};
    }

    template <typename ITERATOR, typename... Args>
    std::pair<T&, bool> emplace_hint(const ITERATOR& iterator, const Key& key, Args&&... args) {
        auto result_ph = tree_.try_emplace(iterator.GetIteratorOfPhTree(), converter_.pre(key));
        auto& bucket = result_ph.first;
        if (result_ph.second) {
            auto result = bucket.emplace(std::forward<Args>(args)...);
            size_ += result.second;
            return {const_cast<T&>(*result.first), result.second};
        } else {
            size_t old_size = bucket.size();
            auto result =
                bucket.emplace_hint(iterator.GetIteratorOfBucket(), std::forward<Args>(args)...);
            bool success = old_size < bucket.size();
            size_ += success;
            return {const_cast<T&>(*result), success};
        }
    }

    std::pair<T&, bool> insert(const Key& key, const T& value) {
        return emplace(key, value);
    }

    template <typename... Args>
    std::pair<T&, bool> try_emplace(const Key& key, Args&&... args) {
        return emplace(key, std::forward<Args>(args)...);
    }

    template <typename ITERATOR, typename... Args>
    std::pair<T&, bool> try_emplace(const ITERATOR& iterator, const Key& key, Args&&... args) {
        return emplace_hint(iterator, key, std::forward<Args>(args)...);
    }

    size_t count(const Key& key) const {
        auto iter = tree_.find(converter_.pre(key));
        if (iter != tree_.end()) {
            return iter->size();
        }
        return 0;
    }

    template <typename QUERY_TYPE = DEFAULT_QUERY_TYPE>
    size_t estimate_count(QueryBox query_box, QUERY_TYPE query_type = QUERY_TYPE()) const {
        size_t n = 0;
        auto counter_lambda = [&](const Key&, const BUCKET& bucket) { n += bucket.size(); };
        tree_.for_each(query_type(converter_.pre_query(query_box)), counter_lambda);
        return n;
    }

    auto find(const Key& key) const {
        return CreateIterator(tree_.find(converter_.pre(key)));
    }

    auto find(const Key& key, const T& value) const {
        return CreateIteratorFind(tree_.find(converter_.pre(key)), value);
    }

    size_t erase(const Key& key, const T& value) {
        auto iter_outer = tree_.find(converter_.pre(key));
        if (iter_outer != tree_.end()) {
            auto& bucket = *iter_outer;
            auto result = bucket.erase(value);
            if (bucket.empty()) {
                tree_.erase(iter_outer);
            }
            size_ -= result;
            return result;
        }
        return 0;
    }

    template <typename ITERATOR>
    size_t erase(const ITERATOR& iterator) {
        static_assert(
            std::is_convertible_v<ITERATOR*, IteratorBase<PHTREE>*>,
            "erase(iterator) requires an iterator argument. For erasing by key please use "
            "erase(key, value).");
        if (iterator != end()) {
            auto& bucket = const_cast<BUCKET&>(*iterator.GetIteratorOfPhTree());
            size_t old_size = bucket.size();
            bucket.erase(iterator.GetIteratorOfBucket());
            bool success = bucket.size() < old_size;
            if (bucket.empty()) {
                success &= tree_.erase(iterator.GetIteratorOfPhTree()) > 0;
            }
            size_ -= success;
            return success;
        }
        return 0;
    }

    template <typename T2>
    size_t relocate(const Key& old_key, const Key& new_key, T2&& value, bool verify_exists = true) {
        auto fn = [&value](BUCKET& src, BUCKET& dst) -> size_t {
            auto it = src.find(value);
            if (it != src.end() && dst.emplace(std::move(*it)).second) {
                src.erase(it);
                return 1;
            }
            return 0;
        };
        auto count_fn = [&value](BUCKET& src) -> size_t { return src.find(value) != src.end(); };
        return tree_._relocate_mm(
            converter_.pre(old_key), converter_.pre(new_key), verify_exists, fn, count_fn);
    }

    template <typename T2>
    [[deprecated]] size_t relocate2(
        const Key& old_key, const Key& new_key, T2&& value, bool count_equals = true) {
        auto pair = tree_._find_or_create_two_mm(
            converter_.pre(old_key), converter_.pre(new_key), count_equals);
        auto& iter_old = pair.first;
        auto& iter_new = pair.second;

        if (iter_old.IsEnd()) {
            return 0;
        }
        auto iter_old_value = iter_old->find(value);
        if (iter_old_value == iter_old->end()) {
            if (iter_new->empty()) {
                tree_.erase(iter_new);
            }
            return 0;
        }

        if (iter_old == iter_new) {
            assert(old_key == new_key);
            return 1;
        }

        assert(iter_old_value != iter_old->end());
        if (!iter_new->emplace(std::move(*iter_old_value)).second) {
            return 0;
        }

        iter_old->erase(iter_old_value);
        if (iter_old->empty()) {
            [[maybe_unused]] auto found = tree_.erase(iter_old);
            assert(found);
        }
        return 1;
    }

    template <typename PREDICATE>
    size_t relocate_if(
        const Key& old_key, const Key& new_key, PREDICATE&& pred_fn, bool verify_exists = true) {
        auto fn = [&pred_fn](BUCKET& src, BUCKET& dst) -> size_t {
            size_t result = 0;
            auto iter_src = src.begin();
            while (iter_src != src.end()) {
                if (pred_fn(*iter_src) && dst.emplace(std::move(*iter_src)).second) {
                    iter_src = src.erase(iter_src);
                    ++result;
                } else {
                    ++iter_src;
                }
            }
            return result;
        };
        auto count_fn = [&pred_fn](BUCKET& src) -> size_t {
            size_t result = 0;
            auto iter_src = src.begin();
            while (iter_src != src.end()) {
                if (pred_fn(*iter_src)) {
                    ++result;
                }
                ++iter_src;
            }
            return result;
        };
        return tree_._relocate_mm(
            converter_.pre(old_key), converter_.pre(new_key), verify_exists, fn, count_fn);
    }

    template <typename PREDICATE>
    [[deprecated]] size_t relocate_if2(
        const Key& old_key, const Key& new_key, PREDICATE&& predicate, bool count_equals = true) {
        auto pair = tree_._find_or_create_two_mm(
            converter_.pre(old_key), converter_.pre(new_key), count_equals);
        auto& iter_old = pair.first;
        auto& iter_new = pair.second;

        if (iter_old.IsEnd()) {
            assert(iter_new.IsEnd() || !iter_new->empty());
            return 0;
        }

        if (iter_old == iter_new) {
            assert(old_key == new_key);
            return 1;
        }

        size_t n = 0;
        auto it = iter_old->begin();
        while (it != iter_old->end()) {
            if (predicate(*it) && iter_new->emplace(std::move(*it)).second) {
                it = iter_old->erase(it);
                ++n;
            } else {
                ++it;
            }
        }

        if (iter_old->empty()) {
            [[maybe_unused]] auto found = tree_.erase(iter_old);
            assert(found);
        } else if (iter_new->empty()) {
            [[maybe_unused]] auto found = tree_.erase(iter_new);
            assert(found);
        }
        return n;
    }

    auto relocate_all(const Key& old_key, const Key& new_key) {
        return tree_.relocate(old_key, new_key);
    }

    template <typename CALLBACK, typename FILTER = FilterNoOp>
    void for_each(CALLBACK&& callback, FILTER&& filter = FILTER()) const {
        tree_.for_each(
            NoOpCallback{},
            WrapCallbackFilter<CALLBACK, FILTER>{
                std::forward<CALLBACK>(callback), std::forward<FILTER>(filter), converter_});
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
        tree_.template for_each<NoOpCallback, WrapCallbackFilter<CALLBACK, FILTER>>(
            query_type(converter_.pre_query(query_box)),
            {},
            {std::forward<CALLBACK>(callback), std::forward<FILTER>(filter), converter_});
    }

    template <typename FILTER = FilterNoOp>
    auto begin(FILTER&& filter = FILTER()) const {
        return CreateIterator(tree_.begin(std::forward<FILTER>(filter)));
    }

    template <typename FILTER = FilterNoOp, typename QUERY_TYPE = DEFAULT_QUERY_TYPE>
    auto begin_query(
        const QueryBox& query_box,
        FILTER&& filter = FILTER(),
        QUERY_TYPE&& query_type = QUERY_TYPE()) const {
        return CreateIterator(tree_.begin_query(
            query_type(converter_.pre_query(query_box)), std::forward<FILTER>(filter)));
    }

    template <
        typename DISTANCE,
        typename FILTER = FilterNoOp,
        bool DUMMY = POINT_KEYS,
        typename std::enable_if<DUMMY, int>::type = 0>
    auto begin_knn_query(
        size_t min_results,
        const Key& center,
        DISTANCE&& distance_function = DISTANCE(),
        FILTER&& filter = FILTER()) const {
        return CreateIteratorKnn(tree_.begin_knn_query(
            min_results,
            converter_.pre(center),
            std::forward<DISTANCE>(distance_function),
            std::forward<FILTER>(filter)));
    }

    auto end() const {
        return IteratorNormal<EndType, PHTREE>{};
    }

    void clear() {
        tree_.clear();
        size_ = 0;
    }

    [[nodiscard]] size_t size() const {
        return size_;
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
        size_t n = 0;
        for (const auto& bucket : tree_) {
            assert(!bucket.empty());
            n += bucket.size();
        }
        assert(n == size_);
    }

    template <typename OUTER_ITER>
    auto CreateIteratorFind(OUTER_ITER&& outer_iter, const T& value) const {
        auto bucket_iter =
            outer_iter == tree_.end() ? BucketIterType{} : outer_iter.second().find(value);
        return IteratorNormal<OUTER_ITER, PHTREE>(
            std::forward<OUTER_ITER>(outer_iter), std::move(bucket_iter));
    }

    template <typename OUTER_ITER>
    auto CreateIterator(OUTER_ITER&& outer_iter) const {
        auto bucket_iter =
            outer_iter == tree_.end() ? BucketIterType{} : outer_iter.second().begin();
        return IteratorNormal<OUTER_ITER, PHTREE>(
            std::forward<OUTER_ITER>(outer_iter), std::move(bucket_iter));
    }

    template <typename OUTER_ITER>
    auto CreateIteratorKnn(OUTER_ITER&& outer_iter) const {
        auto bucket_iter =
            outer_iter == tree_.end() ? BucketIterType{} : outer_iter.second().begin();
        return IteratorKnn<OUTER_ITER, PHTREE>(
            std::forward<OUTER_ITER>(outer_iter), std::move(bucket_iter));
    }

    template <typename CALLBACK, typename FILTER>
    class WrapCallbackFilter {
      public:

        template <typename CB, typename F>
        WrapCallbackFilter(CB&& callback, F&& filter, const CONVERTER& converter)
        : callback_{std::forward<CB>(callback)}
        , filter_{std::forward<F>(filter)}
        , converter_{converter} {}

        [[nodiscard]] inline bool IsEntryValid(
            const KeyInternal& internal_key, const BUCKET& bucket) {
            if (filter_.IsEntryValid(internal_key, bucket)) {
                auto key = converter_.post(internal_key);
                for (auto& entry : bucket) {
                    if (filter_.IsBucketEntryValid(internal_key, entry)) {
                        callback_(key, entry);
                    }
                }
            }
            return false;
        }

        [[nodiscard]] inline bool IsNodeValid(const KeyInternal& prefix, int bits_to_ignore) {
            return filter_.IsNodeValid(prefix, bits_to_ignore);
        }

      private:
        CALLBACK callback_;
        FILTER filter_;
        const CONVERTER& converter_;
    };

    struct NoOpCallback {
        constexpr void operator()(const Key&, const BUCKET&) const noexcept {}
    };

    v16::PhTreeV16<DimInternal, BUCKET, CONVERTER> tree_;
    CONVERTER converter_;
    size_t size_;
};

template <
    dimension_t DIM,
    typename T,
    typename CONVERTER = ConverterIEEE<DIM>,
    typename BUCKET = b_plus_tree_hash_set<T>>
using PhTreeMultiMapD = PhTreeMultiMap<DIM, T, CONVERTER, BUCKET>;

template <
    dimension_t DIM,
    typename T,
    typename CONVERTER_BOX,
    typename BUCKET = b_plus_tree_hash_set<T>>
using PhTreeMultiMapBox = PhTreeMultiMap<DIM, T, CONVERTER_BOX, BUCKET, false, QueryIntersect>;

template <
    dimension_t DIM,
    typename T,
    typename CONVERTER_BOX = ConverterBoxIEEE<DIM>,
    typename BUCKET = b_plus_tree_hash_set<T>>
using PhTreeMultiMapBoxD = PhTreeMultiMapBox<DIM, T, CONVERTER_BOX, BUCKET>;

}  // namespace improbable::phtree

#endif  // PHTREE_PHTREE_MULTIMAP_H
