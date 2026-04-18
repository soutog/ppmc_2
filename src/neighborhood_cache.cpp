#include "neighborhood_cache.h"

#include <algorithm>
#include <utility>

NeighborhoodCache::NeighborhoodCache(const Instance& instance,
                                     const DistanceMatrix& dm) {
    const int n = instance.numNodes();
    const int p = instance.numMedians();

    // k_near cobre a vizinhanca de cada mediana para o M3. Piso de 100
    // e fator 5x o tamanho medio do cluster para nao perder movimentos
    // bons em instancias de p grande (p3038, fnl4461). Nunca maior que n-1.
    k_near_ = std::max(100, 5 * (n / std::max(1, p)));
    k_near_ = std::min(k_near_, n - 1);

    // Teto do armazenamento: no pior caso todas as p medianas estao entre os
    // mais proximos de i, entao precisamos de k_near + p entradas + margem
    // para garantir k_near nao-medianas apos o filtro dinamico.
    const int storage_cap = std::min(n - 1, k_near_ + p + 20);

    nearest_.assign(n, {});
    std::vector<std::pair<double, int>> ordered;
    ordered.reserve(n);

    for (int i = 0; i < n; ++i) {
        ordered.clear();
        for (int j = 0; j < n; ++j) {
            if (j == i) continue;
            ordered.push_back({dm.at(i, j), j});
        }

        // partial_sort nos da os storage_cap primeiros em O(n log k).
        if (storage_cap < static_cast<int>(ordered.size())) {
            std::partial_sort(
                ordered.begin(),
                ordered.begin() + storage_cap,
                ordered.end(),
                [](const std::pair<double, int>& lhs,
                   const std::pair<double, int>& rhs) {
                    if (lhs.first != rhs.first) return lhs.first < rhs.first;
                    return lhs.second < rhs.second;
                });
            ordered.resize(storage_cap);
        } else {
            std::sort(ordered.begin(), ordered.end(),
                      [](const std::pair<double, int>& lhs,
                         const std::pair<double, int>& rhs) {
                          if (lhs.first != rhs.first) return lhs.first < rhs.first;
                          return lhs.second < rhs.second;
                      });
        }

        nearest_[i].reserve(ordered.size());
        for (const auto& entry : ordered) {
            nearest_[i].push_back(entry.second);
        }
    }
}
