#ifndef CLUSTERING_SEARCH_H
#define CLUSTERING_SEARCH_H

#include "candidate_lists.h"
#include "distance_matrix.h"
#include "evaluator.h"
#include "grasp_constructor.h"
#include "instance.h"
#include "partial_optimizer.h"
#include "solution.h"

#include <random>
#include <vector>

struct ClusterInfo {
    Solution center;
    int volume = 0;
    int ineff_count = 0;
    int hits = 0;
    int last_update_iter = -1;
    bool initialized = false;
};

struct ClusteringSearchStats {
    int observations = 0;
    int active_clusters = 0;
    int new_clusters = 0;
    int center_updates = 0;
    int assignments_to_existing = 0;
    int max_cluster_volume = 0;
    int po_triggers = 0;
    int po_improvements = 0;
    double po_total_gain = 0.0;
    double po_best_gain = 0.0;
    int destroy_repair_calls = 0;
    int destroy_repair_improvements = 0;
    int ils_current_promotions = 0;
};

class ClusteringSearch {
private:
    const Instance& instance_;
    const DistanceMatrix& dm_;
    const Evaluator& evaluator_;
    const CandidateLists* r1_filter_;
    PartialOptimizer* partial_optimizer_;
    GRASPConstructor* grasp_;
    int gamma_;
    int max_volume_;
    int max_inef_;
    int distance_threshold_;
    int destroy_k_;
    std::mt19937 rng_;
    std::vector<ClusterInfo> clusters_;
    ClusteringSearchStats stats_;

    int distanceBetween(const Solution& a, const Solution& b) const;
    int findBestCluster(const Solution& s, int* best_distance) const;
    int selectClusterFor(const Solution& s, int* best_distance, bool* created_new) const;
    void updateCenter(ClusterInfo& cluster, const Solution& s, int iter);
    void intensifyCluster(ClusterInfo& cluster,
                          Solution& best_global,
                          Solution* ils_current);
    void destroyRepairCluster(ClusterInfo& cluster, Solution& best_global);

public:
    ClusteringSearch(const Instance& instance,
                     const DistanceMatrix& dm,
                     const Evaluator& evaluator,
                     const CandidateLists* r1_filter,
                     PartialOptimizer* partial_optimizer,
                     GRASPConstructor* grasp,
                     int gamma = 12,
                     int max_volume = 5,
                     int max_inef = 3,
                     int distance_threshold = -1,
                     unsigned int seed = 987654321u);

    void observe(const Solution& local_opt,
                 int iter,
                 Solution& best_global,
                 Solution* ils_current = nullptr);

    const ClusteringSearchStats& stats() const;
    const std::vector<ClusterInfo>& clusters() const;
};

#endif
