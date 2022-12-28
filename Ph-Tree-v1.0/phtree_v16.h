#ifndef PHTREE_V16_PHTREE_V16_H
#define PHTREE_V16_PHTREE_V16_H

#include "debug_helper_v16.h"
#include "for_each.h"
#include "for_each_hc.h"
#include "iterator_full.h"
#include "iterator_hc.h"
#include "iterator_knn_hs.h"
#include "iterator_with_parent.h"
#include "node.h"

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename T, typename CONVERT = ConverterNoOp<DIM, scalar_64_t>>
class PhTreeV16 {
    friend PhTreeDebugHelper;
    using ScalarExternal = typename CONVERT::ScalarExternal;
    using ScalarInternal = typename CONVERT::ScalarInternal;
    using KeyT = typename CONVERT::KeyInternal;
    using EntryT = Entry<DIM, T, ScalarInternal>;
    using NodeT = Node<DIM, T, ScalarInternal>;

  public:
    static_assert(!std::is_reference<T>::value, "Reference type value are not supported.");
    static_assert(std::is_signed<ScalarInternal>::value, "ScalarInternal must be a signed type");
    static_assert(
        std::is_integral<ScalarInternal>::value, "ScalarInternal must be an integral type");
    static_assert(
        std::is_arithmetic<ScalarExternal>::value, "ScalarExternal must be an arithmetic type");
    static_assert(DIM >= 1 && DIM <= 63, "This PH-Tree supports between 1 and 63 dimensions");

    explicit PhTreeV16(CONVERT* converter)
    : num_entries_{0}
    , root_{{}, NodeT{}, MAX_BIT_WIDTH<ScalarInternal> - 1}
    , converter_{converter} {}

    PhTreeV16(const PhTreeV16& other) = delete;
    PhTreeV16& operator=(const PhTreeV16& other) = delete;
    PhTreeV16(PhTreeV16&& other) noexcept = default;
    PhTreeV16& operator=(PhTreeV16&& other) noexcept = default;
    ~PhTreeV16() noexcept = default;

    template <typename... Args>
    std::pair<T&, bool> try_emplace(const KeyT& key, Args&&... args) {
        auto* current_entry = &root_;
        bool is_inserted = false;
        while (current_entry->IsNode()) {
            current_entry = &current_entry->GetNode().Emplace(
                is_inserted, key, current_entry->GetNodePostfixLen(), std::forward<Args>(args)...);
        }
        num_entries_ += is_inserted;
        return {current_entry->GetValue(), is_inserted};
    }

    template <typename ITERATOR, typename... Args>
    std::pair<T&, bool> try_emplace(const ITERATOR& iterator, const KeyT& key, Args&&... args) {
        if constexpr (!std::is_same_v<ITERATOR, IteratorWithParent<T, CONVERT>>) {
            return try_emplace(key, std::forward<Args>(args)...);
        } else {
            if (!iterator.GetParentNodeEntry()) {
                return try_emplace(key, std::forward<Args>(args)...);
            }

            auto* parent_entry = iterator.GetParentNodeEntry();
            if (NumberOfDivergingBits(key, parent_entry->GetKey()) >
                parent_entry->GetNodePostfixLen() + 1) {
                return try_emplace(key, std::forward<Args>(args)...);
            }
            auto* entry = parent_entry;
            bool is_inserted = false;
            while (entry->IsNode()) {
                entry = &entry->GetNode().Emplace(
                    is_inserted, key, entry->GetNodePostfixLen(), std::forward<Args>(args)...);
            }
            num_entries_ += is_inserted;
            return {entry->GetValue(), is_inserted};
        }
    }

    std::pair<T&, bool> insert(const KeyT& key, const T& value) {
        return try_emplace(key, value);
    }

    T& operator[](const KeyT& key) {
        return try_emplace(key).first;
    }

    size_t count(const KeyT& key) const {
        if (empty()) {
            return 0;
        }
        auto* current_entry = &root_;
        while (current_entry && current_entry->IsNode()) {
            current_entry = current_entry->GetNode().FindC(key, current_entry->GetNodePostfixLen());
        }
        return current_entry ? 1 : 0;
    }

    auto find(const KeyT& key) const {
        const EntryT* current_entry = &root_;
        const EntryT* current_node = nullptr;
        const EntryT* parent_node = nullptr;
        while (current_entry && current_entry->IsNode()) {
            parent_node = current_node;
            current_node = current_entry;
            current_entry = current_entry->GetNode().FindC(key, current_entry->GetNodePostfixLen());
        }

        return IteratorWithParent<T, CONVERT>(current_entry, current_node, parent_node, converter_);
    }

    size_t erase(const KeyT& key) {
        auto* entry = &root_;
        bool found = false;
        while (entry) {
            entry = entry->GetNode().Erase(key, entry, entry != &root_, found);
        }
        num_entries_ -= found;
        return found;
    }

    template <typename ITERATOR>
    size_t erase(const ITERATOR& iterator) {
        if (iterator.IsEnd()) {
            return 0;
        }
        if constexpr (std::is_same_v<ITERATOR, IteratorWithParent<T, CONVERT>>) {
            const auto& iter_rich = static_cast<const IteratorWithParent<T, CONVERT>&>(iterator);
            if (!iter_rich.GetNodeEntry() || iter_rich.GetNodeEntry() == &root_) {
                return erase(iter_rich.GetEntry()->GetKey());
            }
            bool found = false;
            EntryT* entry = iter_rich.GetNodeEntry();
            while (entry != nullptr) {
                entry = entry->GetNode().Erase(iter_rich.GetEntry()->GetKey(), entry, true, found);
            }
            num_entries_ -= found;
            return found;
        }
        return erase(iterator.GetEntry()->GetKey());
    }

    template <typename PRED>
    [[deprecated]] size_t relocate_if2(const KeyT& old_key, const KeyT& new_key, PRED pred) {
        auto pair = _find_two(old_key, new_key);
        auto& iter_old = pair.first;
        auto& iter_new = pair.second;

        if (iter_old.IsEnd() || !pred(*iter_old)) {
            return 0;
        }
        if (iter_old == iter_new) {
            iter_old.GetEntry()->SetKey(new_key);
            return 1;
        }

        bool is_inserted = false;
        auto* new_parent = iter_new.GetNodeEntry();
        new_parent->GetNode().Emplace(
            is_inserted, new_key, new_parent->GetNodePostfixLen(), std::move(*iter_old));
        if (!is_inserted) {
            return 0;
        }

        EntryT* old_node_entry = iter_old.GetNodeEntry();
        if (iter_old.GetParentNodeEntry() == iter_new.GetNodeEntry()) {
            old_node_entry = iter_old.GetParentNodeEntry();
        }
        bool found = false;
        while (old_node_entry) {
            old_node_entry = old_node_entry->GetNode().Erase(
                old_key, old_node_entry, old_node_entry != &root_, found);
        }
        assert(found);
        return 1;
    }

    template <typename PRED>
    auto relocate_if(const KeyT& old_key, const KeyT& new_key, PRED&& pred) {
        bit_width_t n_diverging_bits = NumberOfDivergingBits(old_key, new_key);

        EntryT* current_entry = &root_;
        EntryT* old_node_entry = nullptr;
        EntryT* old_node_entry_parent = nullptr;
        EntryT* new_node_entry = nullptr;
        while (current_entry && current_entry->IsNode()) {
            old_node_entry_parent = old_node_entry;
            old_node_entry = current_entry;
            auto postfix_len = old_node_entry->GetNodePostfixLen();
            if (postfix_len + 1 >= n_diverging_bits) {
                new_node_entry = old_node_entry;
            }
            current_entry = current_entry->GetNode().Find(old_key, postfix_len);
        }
        EntryT* old_entry = current_entry;

        if (old_entry == nullptr || !pred(old_entry->GetValue())) {
            return 0;
        }

        if (n_diverging_bits == 0) {
            return 1;
        }
        if (old_node_entry->GetNodePostfixLen() >= n_diverging_bits) {
            old_entry->SetKey(new_key);
            return 1;
        }

        auto new_entry = new_node_entry;
        while (new_entry && new_entry->IsNode()) {
            new_node_entry = new_entry;
            new_entry = new_entry->GetNode().Find(new_key, new_entry->GetNodePostfixLen());
        }
        if (new_entry != nullptr) {
            return 0;
        }
        bool is_inserted = false;
        new_entry = &new_node_entry->GetNode().Emplace(
            is_inserted,
            new_key,
            new_node_entry->GetNodePostfixLen(),
            std::move(old_entry->ExtractValue()));

        if (old_node_entry_parent == new_node_entry) {
            old_node_entry = old_node_entry_parent;
        }

        bool is_found = false;
        while (old_node_entry) {
            old_node_entry = old_node_entry->GetNode().Erase(
                old_key, old_node_entry, old_node_entry != &root_, is_found);
        }
        assert(is_found);
        return 1;
    }

  private:
    auto _find_two(const KeyT& old_key, const KeyT& new_key) {
        using Iter = IteratorWithParent<T, CONVERT>;
        bit_width_t n_diverging_bits = NumberOfDivergingBits(old_key, new_key);

        EntryT* current_entry = &root_;
        EntryT* old_node_entry = nullptr;
        EntryT* old_node_entry_parent = nullptr;
        EntryT* new_node_entry = nullptr;
        while (current_entry && current_entry->IsNode()) {
            old_node_entry_parent = old_node_entry;
            old_node_entry = current_entry;
            auto postfix_len = old_node_entry->GetNodePostfixLen();
            if (postfix_len + 1 >= n_diverging_bits) {
                new_node_entry = old_node_entry;
            }
            current_entry = current_entry->GetNode().Find(old_key, postfix_len);
        }
        EntryT* old_entry = current_entry;
        if (old_entry == nullptr) {
            auto iter = Iter(nullptr, nullptr, nullptr, converter_);
            return std::make_pair(iter, iter);
        }
        assert(old_node_entry != nullptr);
        if (n_diverging_bits == 0 || old_node_entry->GetNodePostfixLen() >= n_diverging_bits) {
            auto iter = Iter(old_entry, old_node_entry, old_node_entry_parent, converter_);
            return std::make_pair(iter, iter);
        }
        auto new_entry = new_node_entry;
        while (new_entry && new_entry->IsNode()) {
            new_node_entry = new_entry;
            new_entry = new_entry->GetNode().Find(new_key, new_entry->GetNodePostfixLen());
        }

        auto iter1 = Iter(old_entry, old_node_entry, old_node_entry_parent, converter_);
        auto iter2 = Iter(new_entry, new_node_entry, nullptr, converter_);
        return std::make_pair(iter1, iter2);
    }

  public:
    template <typename RELOCATE, typename COUNT>
    size_t _relocate_mm(
        const KeyT& old_key,
        const KeyT& new_key,
        bool verify_exists,
        RELOCATE&& relocate_fn,
        COUNT&& count_fn) {
        bit_width_t n_diverging_bits = NumberOfDivergingBits(old_key, new_key);

        if (!verify_exists && n_diverging_bits == 0) {
            return 1;
        }

        EntryT* current_entry = &root_;
        EntryT* old_node_entry = nullptr;
        EntryT* new_node_entry = nullptr;
        while (current_entry && current_entry->IsNode()) {
            old_node_entry = current_entry;
            auto postfix_len = old_node_entry->GetNodePostfixLen();
            if (postfix_len + 1 >= n_diverging_bits) {
                new_node_entry = old_node_entry;
            }
            current_entry = current_entry->GetNode().Find(old_key, postfix_len);
        }
        EntryT* old_entry = current_entry;
        if (old_entry == nullptr) {
            return 0;
        }
        if (n_diverging_bits == 0) {
            return count_fn(old_entry->GetValue());
        }
        if (old_node_entry->GetNodePostfixLen() >= n_diverging_bits) {
            if (old_entry->GetValue().size() == 1) {
                auto result = count_fn(old_entry->GetValue());
                if (result > 0) {
                    old_entry->SetKey(new_key);
                }
                return result;
            }
        }
        auto new_entry = new_node_entry;
        bool same_node = old_node_entry == new_node_entry;
        bool is_inserted = false;
        while (new_entry && new_entry->IsNode()) {
            new_node_entry = new_entry;
            is_inserted = false;
            new_entry =
                &new_entry->GetNode().Emplace(is_inserted, new_key, new_entry->GetNodePostfixLen());
            num_entries_ += is_inserted;
        }
        if (is_inserted && same_node) {
            old_entry = old_node_entry;
            while (old_entry && old_entry->IsNode()) {
                old_node_entry = old_entry;
                old_entry = old_entry->GetNode().Find(old_key, old_entry->GetNodePostfixLen());
            }
        }

        auto result = relocate_fn(old_entry->GetValue(), new_entry->GetValue());

        if (result == 0) {
            clean_up(new_key, new_entry, new_node_entry);
        }
        clean_up(old_key, old_entry, old_node_entry);
        return result;
    }

  private:
    void clean_up(const KeyT& key, EntryT* entry, EntryT* node_entry) {
        if (entry != nullptr && entry->GetValue().empty()) {
            bool found = false;
            while (node_entry != nullptr && node_entry->IsNode()) {
                found = false;
                node_entry =
                    node_entry->GetNode().Erase(key, node_entry, node_entry != &root_, found);
            }
            num_entries_ -= found;
        }
    }

  public:
    auto _find_or_create_two_mm(const KeyT& old_key, const KeyT& new_key, bool count_equals) {
        using Iter = IteratorWithParent<T, CONVERT>;
        bit_width_t n_diverging_bits = NumberOfDivergingBits(old_key, new_key);

        if (!count_equals && n_diverging_bits == 0) {
            auto iter = Iter(nullptr, nullptr, nullptr, converter_);
            return std::make_pair(iter, iter);
        }

        EntryT* new_entry = &root_;
        EntryT* old_node_entry = nullptr;
        EntryT* new_node_entry = nullptr;
        bool is_inserted = false;
        while (new_entry && new_entry->IsNode() &&
               new_entry->GetNodePostfixLen() + 1 >= n_diverging_bits) {
            new_node_entry = new_entry;
            auto postfix_len = new_entry->GetNodePostfixLen();
            new_entry = &new_entry->GetNode().Emplace(is_inserted, new_key, postfix_len);
        }
        old_node_entry = new_node_entry;

        while (new_entry->IsNode()) {
            new_node_entry = new_entry;
            new_entry =
                &new_entry->GetNode().Emplace(is_inserted, new_key, new_entry->GetNodePostfixLen());
        }
        num_entries_ += is_inserted;
        assert(new_entry != nullptr);

        auto* old_entry = old_node_entry;
        while (old_entry && old_entry->IsNode()) {
            old_node_entry = old_entry;
            old_entry = old_entry->GetNode().Find(old_key, old_entry->GetNodePostfixLen());
        }

        if (old_entry == nullptr) {
            auto iter = Iter(nullptr, nullptr, nullptr, converter_);
            return std::make_pair(iter, iter);
        }

        if (n_diverging_bits == 0) {
            auto iter = Iter(old_entry, old_node_entry, nullptr, converter_);
            return std::make_pair(iter, iter);
        }

        auto iter1 = Iter(old_entry, old_node_entry, nullptr, converter_);
        auto iter2 = Iter(new_entry, new_node_entry, nullptr, converter_);
        return std::make_pair(iter1, iter2);
    }

    template <typename CALLBACK, typename FILTER = FilterNoOp>
    void for_each(CALLBACK&& callback, FILTER&& filter = FILTER()) {
        ForEach<T, CONVERT, CALLBACK, FILTER>(
            converter_, std::forward<CALLBACK>(callback), std::forward<FILTER>(filter))
            .Traverse(root_);
    }

    template <typename CALLBACK, typename FILTER = FilterNoOp>
    void for_each(CALLBACK&& callback, FILTER&& filter = FILTER()) const {
        ForEach<T, CONVERT, CALLBACK, FILTER>(
            converter_, std::forward<CALLBACK>(callback), std::forward<FILTER>(filter))
            .Traverse(root_);
    }

    template <typename CALLBACK, typename FILTER = FilterNoOp>
    void for_each(
        const PhBox<DIM, ScalarInternal> query_box,
        CALLBACK&& callback,
        FILTER&& filter = FILTER()) const {
        auto pair = find_starting_node(query_box);
        ForEachHC<T, CONVERT, CALLBACK, FILTER>(
            query_box.min(),
            query_box.max(),
            converter_,
            std::forward<CALLBACK>(callback),
            std::forward<FILTER>(filter))
            .Traverse(*pair.first, &pair.second);
    }

    template <typename FILTER = FilterNoOp>
    auto begin(FILTER&& filter = FILTER()) const {
        return IteratorFull<T, CONVERT, FILTER>(root_, converter_, std::forward<FILTER>(filter));
    }

    template <typename FILTER = FilterNoOp>
    auto begin_query(
        const PhBox<DIM, ScalarInternal>& query_box, FILTER&& filter = FILTER()) const {
        auto pair = find_starting_node(query_box);
        return IteratorHC<T, CONVERT, FILTER>(
            *pair.first,
            query_box.min(),
            query_box.max(),
            converter_,
            std::forward<FILTER>(filter));
    }

    template <typename DISTANCE, typename FILTER = FilterNoOp>
    auto begin_knn_query(
        size_t min_results,
        const KeyT& center,
        DISTANCE&& distance_function = DISTANCE(),
        FILTER&& filter = FILTER()) const {
        return IteratorKnnHS<T, CONVERT, DISTANCE, FILTER>(
            root_,
            min_results,
            center,
            converter_,
            std::forward<DISTANCE>(distance_function),
            std::forward<FILTER>(filter));
    }

    auto end() const {
        return IteratorEnd<EntryT>();
    }

    void clear() {
        num_entries_ = 0;
        root_ = EntryT({}, NodeT{}, MAX_BIT_WIDTH<ScalarInternal> - 1);
    }

    [[nodiscard]] size_t size() const {
        return num_entries_;
    }

    [[nodiscard]] bool empty() const {
        return num_entries_ == 0;
    }

  private:
    auto GetDebugHelper() const {
        return DebugHelperV16(root_, num_entries_);
    }

    std::pair<const EntryT*, EntryIteratorC<DIM, EntryT>> find_starting_node(
        const PhBox<DIM, ScalarInternal>& query_box) const {
        auto& prefix = query_box.min();
        bit_width_t max_conflicting_bits = NumberOfDivergingBits(query_box.min(), query_box.max());
        const EntryT* parent = &root_;
        if (max_conflicting_bits > root_.GetNodePostfixLen()) {
            return {&root_, root_.GetNode().Entries().end()};
        }
        EntryIteratorC<DIM, EntryT> entry_iter =
            root_.GetNode().FindPrefix(prefix, max_conflicting_bits, root_.GetNodePostfixLen());
        while (entry_iter != parent->GetNode().Entries().end() && entry_iter->second.IsNode() &&
               entry_iter->second.GetNodePostfixLen() >= max_conflicting_bits) {
            parent = &entry_iter->second;
            entry_iter = parent->GetNode().FindPrefix(
                prefix, max_conflicting_bits, parent->GetNodePostfixLen());
        }
        return {parent, entry_iter};
    }

    size_t num_entries_;
    EntryT root_;
    CONVERT* converter_;
};

}  // namespace improbable::phtree::v16

#endif  // PHTREE_V16_PHTREE_V16_H

