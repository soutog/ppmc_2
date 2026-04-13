#ifndef ILS_H
#define ILS_H

#include "candidate_lists.h"
#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <array>
#include <random>

class ILS {
private:
    const Instance& instance_;
    const DistanceMatrix& distance_matrix_;
    const CandidateLists* r1_filter_;
    int num_iter_max_;

    // Estatisticas
    int total_iterations_;
    int improvements_;
    int perturbations_changed_solution_;
    int returns_to_same_local_optimum_;
    int returns_to_different_equal_cost_;
    std::array<int, 3> iterations_per_level_;
    std::array<int, 3> improvements_per_level_;

public:
    ILS(const Instance& instance,
        const DistanceMatrix& dm,
        int num_iter_max,
        const CandidateLists* r1_filter = nullptr);

    // Recebe solucao ja refinada por VND. Retorna a melhor encontrada.
    Solution run(Solution s, std::mt19937& rng);

    int totalIterations() const;
    int improvements() const;
    int perturbationsChangedSolution() const;
    int returnsToSameLocalOptimum() const;
    int returnsToDifferentEqualCost() const;
    int iterationsAtLevel(int level) const;
    int improvementsAtLevel(int level) const;
};

#endif
