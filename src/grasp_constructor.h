#ifndef GRASP_CONSTRUCTOR_H
#define GRASP_CONSTRUCTOR_H

#include "distance_matrix.h"
#include "evaluator.h"
#include "instance.h"
#include "solution.h"

#include <random>
#include <string>
#include <vector>

class GRASPConstructor {
private:
    const Instance& instance_;
    const DistanceMatrix& distance_matrix_;
    const Evaluator& evaluator_;
    double alpha_;
    int max_tries_;
    std::mt19937 rng_;
    std::vector<std::pair<double, int>> ranked_candidates_;
    std::vector<int> restricted_candidate_list_;
    int last_attempts_;
    std::string last_error_;

    void buildRankedCandidates();
    void buildRestrictedCandidateList();

    std::vector<int> selectRandomMedians();
    bool assignClientsToNearestMedian(const std::vector<int>& medians,
                                      Solution& solution,
                                      std::string* error);

public:
    GRASPConstructor(const Instance& instance,
                     const DistanceMatrix& distance_matrix,
                     const Evaluator& evaluator,
                     double alpha = 0.6,
                     int max_tries = 1000,
                     unsigned int seed = 123456u);

    Solution construct();

    double alpha() const;
    int maxTries() const;
    int lastAttempts() const;
    const std::string& lastError() const;
    const std::vector<std::pair<double, int>>& rankedCandidates() const;
    const std::vector<int>& restrictedCandidateList() const;
};

#endif
