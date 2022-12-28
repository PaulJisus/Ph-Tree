#ifndef PHTREE_COMMON_TREE_STATS_H
#define PHTREE_COMMON_TREE_STATS_H

#include "base_types.h"
#include <sstream>
#include <vector>


namespace improbable::phtree {

class PhTreeStats {
    using SCALAR = scalar_64_t;

  public:
    std::string ToString() {
        std::ostringstream s;
        s << "  nNodes = " << std::to_string(n_nodes_) << std::endl;
        s << "  avgNodeDepth = " << ((double)q_total_depth_ / (double)n_nodes_) << std::endl;
        s << "  AHC=" << n_AHC_ << "  NI=" << n_nt_ << "  nNtNodes_=" << n_nt_nodes_ << std::endl;
        double apl = GetAvgPostlen();
        s << "  avgPostLen = " << apl << " (" << (MAX_BIT_WIDTH<SCALAR> - apl) << ")" << std::endl;
        return s.str();
    }

    std::string ToStringHist() {
        std::ostringstream s;
        s << "  infix_len      = ";
        to_string(s, infix_hist_) << std::endl;
        s << "  nodeSizeLog   = ";
        to_string(s, node_size_log_hist_) << std::endl;
        s << "  node_depth_hist_ = ";
        to_string(s, node_depth_hist_) << std::endl;
        s << "  depthHist     = ";
        to_string(s, q_n_post_fix_n_) << std::endl;
        return s.str();
    }

    double GetAvgPostlen() {
        size_t total = 0;
        size_t num_entry = 0;
        for (bit_width_t i = 0; i < MAX_BIT_WIDTH<SCALAR>; ++i) {
            total += (MAX_BIT_WIDTH<SCALAR> - i) * q_n_post_fix_n_[i];
            num_entry += q_n_post_fix_n_[i];
        }
        return (double)total / (double)num_entry;
    }

    size_t GetNodeCount() {
        return n_nodes_;
    }

    size_t GetCalculatedMemSize() {
        return size_;
    }

  private:
    static std::ostringstream& to_string(std::ostringstream& s, std::vector<size_t>& data) {
        s << "[";
        for (size_t x : data) {
            s << x << ",";
        }
        s << "]";
        return s;
    }

  public:
    size_t n_nodes_ = 0;
    size_t n_AHC_ = 0;
    size_t n_nt_nodes_ = 0;
    size_t n_nt_ = 0;
    size_t n_total_children_ = 0;
    size_t size_ = 0;
    size_t q_total_depth_ = 0;
    std::vector<size_t> q_n_post_fix_n_ =
        std::vector(MAX_BIT_WIDTH<SCALAR>, (size_t)0);
    std::vector<size_t> infix_hist_ = std::vector(MAX_BIT_WIDTH<SCALAR>, (size_t)0);
    std::vector<size_t> node_depth_hist_ =
        std::vector(MAX_BIT_WIDTH<SCALAR>, (size_t)0);
    std::vector<size_t> node_size_log_hist_ = std::vector(32, (size_t)0);
};

}  // namespace improbable::phtree
#endif  // PHTREE_COMMON_TREE_STATS_H

