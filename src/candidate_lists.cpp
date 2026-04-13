#include "candidate_lists.h"

#include <algorithm>
#include <utility>

CandidateLists::CandidateLists(const Instance& instance,
                               const DistanceMatrix& dm,
                               int top_t)
    : top_t_(top_t),
      nearest_(instance.numNodes()) {
    const int n = instance.numNodes();
    const int effective_t = std::min(std::max(1, top_t_), n - 1);
    top_t_ = effective_t;

    std::vector<std::pair<double, int>> ordered;
    ordered.reserve(n);

    for (int v = 0; v < n; ++v) {
        ordered.clear();
        for (int w = 0; w < n; ++w) {
            if (w == v) continue;
            ordered.push_back({dm.at(v, w), w});
        }

        // Partial sort basta: precisamos apenas dos top_t mais proximos.
        const int k = std::min(effective_t, static_cast<int>(ordered.size()));
        std::partial_sort(
            ordered.begin(),
            ordered.begin() + k,
            ordered.end(),
            [](const std::pair<double, int>& lhs,
               const std::pair<double, int>& rhs) {
                if (lhs.first != rhs.first) return lhs.first < rhs.first;
                return lhs.second < rhs.second;
            });

        nearest_[v].reserve(k);
        for (int i = 0; i < k; ++i) {
            nearest_[v].push_back(ordered[i].second);
        }
    }
}
