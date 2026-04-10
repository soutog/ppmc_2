#ifndef PERTURBATION_H
#define PERTURBATION_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <random>

// Perturbacao: aplica level+1 trocas aleatorias via M4.
// Em cada passo, sorteia uniformemente uma troca viavel da vizinhanca atual.
// Se a vizinhanca M4 estiver vazia, encerra por impossibilidade real.
void perturbate(Solution& solution, int level,
                const Instance& instance, const DistanceMatrix& dm,
                std::mt19937& rng);

#endif
