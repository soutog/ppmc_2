#ifndef PERTURBATION_H
#define PERTURBATION_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <random>

// Perturbacao: altera a estrutura de medianas abertas e pode combinar com swaps M4.
// O objetivo eh tirar a busca de uma bacia de atracao antes de aplicar o VND.
void perturbate(Solution& solution, int level,
                const Instance& instance, const DistanceMatrix& dm,
                std::mt19937& rng);

#endif
