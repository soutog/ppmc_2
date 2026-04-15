#ifndef NEIGHBORHOOD_CACHE_H
#define NEIGHBORHOOD_CACHE_H

#include "distance_matrix.h"
#include "instance.h"

#include <vector>

// Cache de vizinhanca por no: para cada i, armazena os nos ordenados por
// distancia crescente a i (truncados em um teto que garante espaco para
// k_near nao-medianas mesmo no pior caso em que os mais proximos sao todos
// medianas).
//
// Usado por bestM3 para restringir os candidatos r2 a uma vizinhanca de r1,
// evitando o scan O(n) original.
class NeighborhoodCache {
private:
    std::vector<std::vector<int>> nearest_;
    int k_near_;

public:
    NeighborhoodCache(const Instance& instance,
                      const DistanceMatrix& dm);

    // Lista de candidatos a r2 quando r1 eh o no consultado. Ordenada por
    // distancia crescente. Pode conter medianas — o caller filtra dinamicamente.
    const std::vector<int>& nearest(int i) const { return nearest_[i]; }

    // Teto de r2 nao-medianas a considerar por r1 em bestM3.
    int kNear() const { return k_near_; }
};

#endif
