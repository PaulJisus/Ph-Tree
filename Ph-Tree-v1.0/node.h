#ifndef PHTREE_V16_NODE_H
#define PHTREE_V16_NODE_H

#include "entry.h"
#include "common.h"
#include "phtree_v16.h"
#include <map>

namespace improbable::phtree::v16 {

template <dimension_t DIM, typename Entry>
using EntryMap = typename std::conditional_t<
    DIM <= 3,
    array_map<Entry, (uint64_t(1) << DIM)>,
    typename std::conditional_t<
        DIM <= 8,
        sparse_map<hc_pos_dim_t<DIM>, Entry>,
        b_plus_tree_map<std::uint64_t, Entry, (uint64_t(1) << DIM)>>>;

template <dimension_t DIM, typename Entry>
using EntryIterator = typename std::remove_const_t<decltype(EntryMap<DIM, Entry>().begin())>;
template <dimension_t DIM, typename Entry>
using EntryIteratorC = decltype(EntryMap<DIM, Entry>().cbegin());

template <dimension_t DIM, typename T, typename SCALAR>
class Node {
    using KeyT = PhPoint<DIM, SCALAR>;
    using EntryT = Entry<DIM, T, SCALAR>;
    using hc_pos_t = hc_pos_64_t;

  public:
    Node() : entries_{} {}
    Node(const Node&) = delete;
    Node(Node&&) = default;
    Node& operator=(const Node&) = delete;
    Node& operator=(Node&&) = delete;

    [[nodiscard]] auto GetEntryCount() const {
        return entries_.size();
    }
    template <typename... Args>
    EntryT& Emplace(bool& is_inserted, const KeyT& key, bit_width_t postfix_len, Args&&... args) {
        hc_pos_t hc_pos = CalcPosInArray(key, postfix_len);
        auto emplace_result = entries_.try_emplace(hc_pos, key, std::forward<Args>(args)...);
        auto& entry = emplace_result.first->second;
        if (emplace_result.second) {
            is_inserted = true;
            return entry;
        }
        return HandleCollision(entry, is_inserted, key, postfix_len, std::forward<Args>(args)...);
    }

    template <typename IterT, typename... Args>
    EntryT& Emplace(
        IterT iter, bool& is_inserted, const KeyT& key, bit_width_t postfix_len, Args&&... args) {
        hc_pos_t hc_pos = CalcPosInArray(key, postfix_len);  // TODO pass in -> should be known!
        if (iter == entries_.end() || iter->first != hc_pos) {
            auto emplace_result =
                entries_.try_emplace(iter, hc_pos, key, std::forward<Args>(args)...);
            is_inserted = true;
            return emplace_result->second;
        }
        auto& entry = iter->second;
        return HandleCollision(entry, is_inserted, key, postfix_len, std::forward<Args>(args)...);
    }

    EntryT* Find(const KeyT& key, bit_width_t postfix_len) {
        hc_pos_t hc_pos = CalcPosInArray(key, postfix_len);
        auto iter = entries_.find(hc_pos);
        if (iter != entries_.end() && DoesEntryMatch(iter->second, key, postfix_len)) {
            return &iter->second;
        }
        return nullptr;
    }

    const EntryT* FindC(const KeyT& key, bit_width_t postfix_len) const {
        return const_cast<Node&>(*this).Find(key, postfix_len);
    }

    auto LowerBound(const KeyT& key, bit_width_t postfix_len, bool& found) {
        hc_pos_t hc_pos = CalcPosInArray(key, postfix_len);
        auto iter = entries_.lower_bound(hc_pos);
        found =
            (iter != entries_.end() && iter->first == hc_pos &&
             DoesEntryMatch(iter->second, key, postfix_len));
        return iter;
    }

    auto End() {
        return entries_.end();
    }

    auto End() const {
        return entries_.end();
    }

    EntryIteratorC<DIM, EntryT> FindPrefix(
        const KeyT& prefix, bit_width_t prefix_post_len, bit_width_t node_postfix_len) const {
        assert(prefix_post_len <= node_postfix_len);
        hc_pos_t hc_pos = CalcPosInArray(prefix, node_postfix_len);
        const auto iter = entries_.find(hc_pos);
        if (iter == entries_.end() || iter->second.IsValue() ||
            iter->second.GetNodePostfixLen() < prefix_post_len) {
            return entries_.end();
        }

        if (DoesEntryMatch(iter->second, prefix, node_postfix_len)) {
            return {iter};
        }
        return entries_.end();
    }

    EntryT* Erase(const KeyT& key, EntryT* parent_entry, bool allow_move_into_parent, bool& found) {
        auto postfix_len = parent_entry->GetNodePostfixLen();
        hc_pos_t hc_pos = CalcPosInArray(key, postfix_len);
        auto it = entries_.find(hc_pos);
        if (it != entries_.end() && DoesEntryMatch(it->second, key, postfix_len)) {
            if (it->second.IsNode()) {
                return &it->second;
            }
            entries_.erase(it);

            found = true;
            if (allow_move_into_parent && GetEntryCount() == 1) {
                parent_entry->ReplaceNodeWithDataFromEntry(std::move(entries_.begin()->second));
            }
        }
        return nullptr;
    }

    auto& Entries() {
        return entries_;
    }

    const auto& Entries() const {
        return entries_;
    }

    void GetStats(
        PhTreeStats& stats, const EntryT& current_entry, bit_width_t current_depth = 0) const {
        size_t num_children = entries_.size();

        ++stats.n_nodes_;
        ++stats.node_depth_hist_[current_depth];
        ++stats.node_size_log_hist_[32 - CountLeadingZeros(std::uint32_t(num_children))];
        stats.n_total_children_ += num_children;
        stats.q_total_depth_ += current_depth;

        for (auto& entry : entries_) {
            auto& child = entry.second;
            if (child.IsNode()) {
                auto child_infix_len = child.GetNodeInfixLen(current_entry.GetNodePostfixLen());
                ++stats.infix_hist_[child_infix_len];
                auto& sub = child.GetNode();
                sub.GetStats(stats, child, current_depth + 1 + child_infix_len);
            } else {
                ++stats.q_n_post_fix_n_[current_depth];
                ++stats.size_;
            }
        }
    }

    size_t CheckConsistency(const EntryT& current_entry, bit_width_t current_depth = 0) const {
        assert(entries_.size() >= 2 || current_depth == 0);
        size_t num_entries_local = 0;
        size_t num_entries_children = 0;
        for (auto& entry : entries_) {
            auto& child = entry.second;
            if (child.IsNode()) {
                auto& sub = child.GetNode();
                auto sub_infix_len = child.GetNodeInfixLen(current_entry.GetNodePostfixLen());
                assert(
                    sub_infix_len + 1 + child.GetNodePostfixLen() ==
                    current_entry.GetNodePostfixLen());
                num_entries_children +=
                    sub.CheckConsistency(child, current_depth + 1 + sub_infix_len);
            } else {
                ++num_entries_local;
            }
        }

        auto post_len = current_entry.GetNodePostfixLen();
        if (post_len == MAX_BIT_WIDTH<SCALAR> - 1) {
            for (auto d : current_entry.GetKey()) {
                assert(d == 0);
            }
        } else {
            for (auto d : current_entry.GetKey()) {
                assert(((d >> post_len) & 0x1) == 1 && "Last bit of node center must be `1`");
                assert(((d >> post_len) << post_len) == d && "postlen bits must all be `0`");
            }
        }

        return num_entries_local + num_entries_children;
    }

  private:
    template <typename... Args>
    auto& WriteValue(hc_pos_t hc_pos, const KeyT& new_key, Args&&... args) {
        return entries_.try_emplace(hc_pos, new_key, std::forward<Args>(args)...).first->second;
    }

    void WriteEntry(hc_pos_t hc_pos, EntryT& entry) {
        if (entry.IsNode()) {
            auto postfix_len = entry.GetNodePostfixLen();
            entries_.try_emplace(hc_pos, entry.GetKey(), entry.ExtractNode(), postfix_len);
        } else {
            entries_.try_emplace(hc_pos, entry.GetKey(), entry.ExtractValue());
        }
    }

    template <typename... Args>
    auto& HandleCollision(
        EntryT& entry,
        bool& is_inserted,
        const KeyT& new_key,
        bit_width_t current_postfix_len,
        Args&&... args) {
        bool is_node = entry.IsNode();
        if (is_node && !entry.HasNodeInfix(current_postfix_len)) {
            return entry;
        }

        bit_width_t max_conflicting_bits = NumberOfDivergingBits(new_key, entry.GetKey());
        auto split_len = is_node ? entry.GetNodePostfixLen() + 1 : 0;
        if (max_conflicting_bits <= split_len) {
            return entry;
        }

        is_inserted = true;
        return InsertSplit(entry, new_key, max_conflicting_bits, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto& InsertSplit(
        EntryT& current_entry,
        const KeyT& new_key,
        bit_width_t max_conflicting_bits,
        Args&&... args) {
        bit_width_t new_postfix_len = max_conflicting_bits - 1;
        hc_pos_t pos_sub_1 = CalcPosInArray(new_key, new_postfix_len);
        hc_pos_t pos_sub_2 = CalcPosInArray(current_entry.GetKey(), new_postfix_len);
        Node new_sub_node{};
        new_sub_node.WriteEntry(pos_sub_2, current_entry);
        auto& new_entry = new_sub_node.WriteValue(pos_sub_1, new_key, std::forward<Args>(args)...);
        current_entry.SetNode(std::move(new_sub_node), new_postfix_len);
        return new_entry;
    }
    bool DoesEntryMatch(
        const EntryT& entry, const KeyT& key, const bit_width_t parent_postfix_len) const {
        if (entry.IsNode()) {
            if (entry.HasNodeInfix(parent_postfix_len)) {
                return KeyEquals(entry.GetKey(), key, entry.GetNodePostfixLen() + 1);
            }
            return true;
        }
        return entry.GetKey() == key;
    }

    EntryMap<DIM, EntryT> entries_;
};

}  // namespace improbable::phtree::v16
#endif  // PHTREE_V16_NODE_H


