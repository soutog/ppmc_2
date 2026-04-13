#ifndef PARTIAL_OPTIMIZER_H
#define PARTIAL_OPTIMIZER_H

#include "distance_matrix.h"
#include "evaluator.h"
#include "instance.h"
#include "solution.h"

#include <string>
#include <vector>

struct PartialOptimizerStats {
    int calls = 0;
    int improving_calls = 0;
    int skipped_calls = 0;
    int calls_without_improvement = 0;
    double total_gain = 0.0;
    double best_gain = 0.0;
    double total_time_s = 0.0;
    bool improved = false;
};

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
    int max_calls_;
    int max_no_improve_;
    double total_time_budget_s_;

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
                     int top_t = 15,
                     int max_calls = 3,
                     int max_no_improve = 2,
                     double total_time_budget_s = 10.0);

    PartialOptimizerStats run(Solution& solution) const;
    bool optimize(Solution& solution) const;
};

#endif
