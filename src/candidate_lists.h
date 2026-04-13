#ifndef CANDIDATE_LISTS_H
#define CANDIDATE_LISTS_H

#include "distance_matrix.h"
#include "instance.h"

#include <vector>

// R1 estatico: para cada no v, guarda os top-t nos mais proximos de v
// (excluindo o proprio v). Usado como filtro de busca local em M1 e M3.
// Computado uma unica vez por instancia, independente da solucao atual.
class CandidateLists {
private:
    int top_t_;
    std::vector<std::vector<int>> nearest_;

public:
    CandidateLists(const Instance& instance,
                   const DistanceMatrix& dm,
                   int top_t);

    int topT() const { return top_t_; }
    const std::vector<int>& nearest(int v) const { return nearest_[v]; }
};

#endif
