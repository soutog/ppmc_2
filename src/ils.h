#ifndef ILS_H
#define ILS_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <random>

class ILS {
private:
    const Instance& instance_;
    const DistanceMatrix& distance_matrix_;
    int num_iter_max_;

    // Estatisticas
    int total_iterations_;
    int improvements_;

public:
    ILS(const Instance& instance, const DistanceMatrix& dm, int num_iter_max);

    // Recebe solucao ja refinada por VND. Retorna a melhor encontrada.
    Solution run(Solution s, std::mt19937& rng);

    int totalIterations() const;
    int improvements() const;
};

#endif
