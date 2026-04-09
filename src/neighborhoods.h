#ifndef NEIGHBORHOODS_H
#define NEIGHBORHOODS_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

struct MoveM1 {
    int client;
    int old_median;
    int new_median;
    double delta;
    bool found;
};

struct MoveM4 {
    int client1;
    int client2;
    double delta;
    bool found;
};

// Best Improvement: varre toda a vizinhanca M1 e retorna o melhor movimento
MoveM1 bestM1(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm);

// Best Improvement: varre toda a vizinhanca M4 e retorna o melhor movimento
MoveM4 bestM4(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm);

// Aplica o movimento M1 na solucao
void applyMove(Solution& solution, const MoveM1& move, const Instance& instance);

// Aplica o movimento M4 na solucao
void applyMove(Solution& solution, const MoveM4& move, const Instance& instance);

#endif
