#ifndef PARTIAL_OPTIMIZER_H
#define PARTIAL_OPTIMIZER_H

#include "distance_matrix.h"
#include "evaluator.h"
#include "instance.h"
#include "solution.h"

#include <vector>

class PartialOptimizer {
private:
    const Instance& instance_;
    const DistanceMatrix& dm_;
    const Evaluator& evaluator_;
    int omega_;
    int min_clusters_;
    int max_clusters_free_;
    double time_limit_s_;
    double alpha_r1_;
    int top_t_;

    int selectReferenceCluster(const Solution& solution) const;
    std::vector<int> selectNeighborhoodClusters(const Solution& solution,
                                                int ref_median,
                                                int effective_min_clusters,
                                                int effective_max_clusters) const;
    std::vector<int> collectFreeNodes(const Solution& solution,
                                      const std::vector<int>& free_medians) const;
    std::vector<std::vector<int>> buildCandidateLists(
        const Solution& solution,
        const std::vector<int>& free_nodes) const;
    bool solveSubproblem(Solution& solution,
                         int ref_median,
                         const std::vector<int>& free_medians,
                         const std::vector<int>& free_nodes) const;

public:
    PartialOptimizer(const Instance& instance,
                     const DistanceMatrix& dm,
                     const Evaluator& evaluator,
                     int omega = 300,
                     int min_clusters = 5,
                     int max_clusters_free = 8,
                     double time_limit_s = 30.0,
                     double alpha_r1 = 2.0,
                     int top_t = 15);

    bool optimize(Solution& solution) const;
};

#endif
