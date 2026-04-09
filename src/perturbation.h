#ifndef PERTURBATION_H
#define PERTURBATION_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <random>

// Perturbacao: aplica level+1 trocas aleatorias via M4.
// Se nao encontrar troca viavel apos 20*n tentativas, encerra cedo.
void perturbate(Solution& solution, int level,
                const Instance& instance, const DistanceMatrix& dm,
                std::mt19937& rng);

#endif
