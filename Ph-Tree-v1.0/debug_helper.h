#ifndef PHTREE_COMMON_DEBUG_HELPER_H
#define PHTREE_COMMON_DEBUG_HELPER_H

#include "tree_stats.h"

namespace improbable::phtree {

class PhTreeDebugHelper {
  public:
    enum class PrintDetail { name, entries, tree };

    class DebugHelper {
        virtual void CheckConsistency() const = 0;

        [[nodiscard]] virtual PhTreeStats GetStats() const = 0;

        [[nodiscard]] virtual std::string ToString(const PrintDetail& detail) const = 0;
    };

    template <typename TREE>
    static void CheckConsistency(const TREE& tree) {
        tree.GetInternalTree().GetDebugHelper().CheckConsistency();
        tree.CheckConsistencyExternal();
    }

    template <typename TREE>
    static PhTreeStats GetStats(const TREE& tree) {
        return tree.GetInternalTree().GetDebugHelper().GetStats();
    }

    template <typename TREE>
    static std::string ToString(const TREE& tree, const PrintDetail& detail) {
        return tree.GetInternalTree().GetDebugHelper().ToString(detail);
    }
};

}  // namespace improbable::phtree
#endif  // PHTREE_COMMON_DEBUG_HELPER_H

