#ifndef ILS_H
#define ILS_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <array>
#include <random>

class ClusteringSearch;

class ILS {
private:
    const Instance& instance_;
    const DistanceMatrix& distance_matrix_;
    int num_iter_max_;
    double time_limit_s_;

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
        double time_limit_s = 0.0);

    // Recebe solucao ja refinada por VND. Retorna a melhor encontrada.
    Solution run(Solution s, std::mt19937& rng, ClusteringSearch* cs = nullptr);

    int totalIterations() const;
    int improvements() const;
    int perturbationsChangedSolution() const;
    int returnsToSameLocalOptimum() const;
    int returnsToDifferentEqualCost() const;
    int iterationsAtLevel(int level) const;
    int improvementsAtLevel(int level) const;
};

#endif
