#include "reduction.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace {
constexpr double kReductionEps = 1e-9;
}

R1Filter::R1Filter(const Instance& instance,
                   const DistanceMatrix& dm,
                   double alpha)
    : allowed_(instance.numNodes()),
      allowed_mask_(instance.numNodes(),
                    std::vector<char>(instance.numNodes(), 0)),
      capacity_ok_mask_(instance.numNodes(),
                        std::vector<char>(instance.numNodes(), 0)),
      alpha_(alpha) {
    const int n = instance.numNodes();

    for (int i = 0; i < n; ++i) {
        std::vector<std::pair<double, int>> ordered;
        ordered.reserve(n);
        for (int j = 0; j < n; ++j) {
            ordered.push_back({dm.at(i, j), j});
            const bool capacity_ok =
                instance.demand(i) <=
                instance.capacity(j) - instance.demand(j) + kReductionEps;
            capacity_ok_mask_[i][j] = capacity_ok ? 1 : 0;
        }

        std::sort(ordered.begin(),
                  ordered.end(),
                  [](const std::pair<double, int>& lhs,
                     const std::pair<double, int>& rhs) {
                      if (lhs.first != rhs.first) {
                          return lhs.first < rhs.first;
                      }
                      return lhs.second < rhs.second;
                  });

        const double threshold =
            alpha_ * instance.capacity(i) - instance.demand(i);
        double cumulative_before = 0.0;
        for (const auto& [distance, node] : ordered) {
            (void)distance;
            if (cumulative_before <= threshold + kReductionEps) {
                allowed_[i].push_back(node);
                allowed_mask_[i][node] = 1;
            } else {
                break;
            }
            cumulative_before += instance.demand(node);
        }
    }
}

const std::vector<int>& R1Filter::allowed(int i) const { return allowed_[i]; }

bool R1Filter::inKi(int i, int j) const { return allowed_mask_[i][j] != 0; }

bool R1Filter::trivialCapacityOk(int i, int j) const {
    return capacity_ok_mask_[i][j] != 0;
}

bool R1Filter::keepsX(int i, int j) const {
    return inKi(i, j) && trivialCapacityOk(i, j);
}

double R1Filter::alpha() const { return alpha_; }

std::vector<char> R2Filter::allowedOpenMaskForSubproblem(
    const Solution& solution,
    const std::vector<int>& free_nodes,
    const std::vector<int>& free_medians,
    const DistanceMatrix& dm,
    int beta) {
    std::vector<char> allowed_open_mask(free_nodes.size(), 0);
    if (free_nodes.empty() || free_medians.empty()) {
        return allowed_open_mask;
    }

    const int n = static_cast<int>(solution.assignments().size());
    const int effective_beta = std::max(1, beta);
    std::vector<int> local_of_global(n, -1);
    std::vector<char> is_free_median(n, 0);
    std::vector<std::vector<int>> cluster_nodes(n);

    for (int idx = 0; idx < static_cast<int>(free_nodes.size()); ++idx) {
        local_of_global[free_nodes[idx]] = idx;
    }
    for (int median : free_medians) {
        is_free_median[median] = 1;
    }

    for (int node : free_nodes) {
        const int median = solution.assignments()[node];
        if (median >= 0 && median < n && is_free_median[median] != 0) {
            cluster_nodes[median].push_back(node);
        }
    }

    for (int median : free_medians) {
        std::vector<int>& cluster = cluster_nodes[median];
        if (cluster.empty()) {
            continue;
        }

        const int keep_count =
            std::min(effective_beta, static_cast<int>(cluster.size()));
        if (keep_count < static_cast<int>(cluster.size())) {
            std::partial_sort(
                cluster.begin(),
                cluster.begin() + keep_count,
                cluster.end(),
                [&](int lhs, int rhs) {
                    const double dl = dm.at(median, lhs);
                    const double dr = dm.at(median, rhs);
                    if (dl != dr) {
                        return dl < dr;
                    }
                    return lhs < rhs;
                });
        } else {
            std::sort(cluster.begin(),
                      cluster.end(),
                      [&](int lhs, int rhs) {
                          const double dl = dm.at(median, lhs);
                          const double dr = dm.at(median, rhs);
                          if (dl != dr) {
                              return dl < dr;
                          }
                          return lhs < rhs;
                      });
        }

        for (int idx = 0; idx < keep_count; ++idx) {
            const int node = cluster[idx];
            const int local_idx = local_of_global[node];
            if (local_idx >= 0) {
                allowed_open_mask[local_idx] = 1;
            }
        }
    }

    return allowed_open_mask;
}
