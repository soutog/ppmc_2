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

struct MoveM2 {
    int old_median;
    int new_median;
    double delta;
    bool found;
};

struct MoveM3 {
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

MoveM1 bestM1(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm);

MoveM2 bestM2(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm);

MoveM3 bestM3(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm);

MoveM4 bestM4(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm);

void applyMove(Solution& solution, const MoveM1& move, const Instance& instance);
void applyMove(Solution& solution, const MoveM2& move, const Instance& instance,
               const DistanceMatrix& dm);
void applyMove(Solution& solution, const MoveM3& move, const Instance& instance,
               const DistanceMatrix& dm);
void applyMove(Solution& solution, const MoveM4& move, const Instance& instance);

#endif
