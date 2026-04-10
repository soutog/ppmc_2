#include "neighborhoods.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace {
constexpr double kImprovementEps = 1e-9;
}  // namespace

MoveM1 bestM1(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm) {
    MoveM1 best{};
    best.delta = 0.0;
    best.found = false;

    const int n = instance.numNodes();
    const std::vector<int>& medians = solution.medians();
    const std::vector<int>& va = solution.assignments();
    const std::vector<double>& load = solution.load();
    const int p = static_cast<int>(medians.size());

    for (int j = 0; j < n; ++j) {
        const int r1 = va[j];
        const double cost_r1 = dm(j, r1);
        const double demand_j = instance.demand(j);

        for (int k = 0; k < p; ++k) {
            const int r2 = medians[k];
            if (r2 == r1) continue;

            // Checar capacidade
            if (load[r2] + demand_j > instance.capacity(r2)) continue;

            const double delta = dm(j, r2) - cost_r1;

            if (delta < best.delta - kImprovementEps) {
                best.client = j;
                best.old_median = r1;
                best.new_median = r2;
                best.delta = delta;
                best.found = true;
            }
        }
    }

    return best;
}

MoveM4 bestM4(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm) {
    MoveM4 best{};
    best.delta = 0.0;
    best.found = false;

    const int n = instance.numNodes();
    const std::vector<int>& va = solution.assignments();
    const std::vector<double>& load = solution.load();

    for (int j1 = 0; j1 < n; ++j1) {
        const int r1 = va[j1];
        const double demand_j1 = instance.demand(j1);
        const double cost_j1 = dm(j1, r1);

        for (int j2 = j1 + 1; j2 < n; ++j2) {
            const int r2 = va[j2];
            if (r1 == r2) continue;  // mesmo cluster, pula

            const double demand_j2 = instance.demand(j2);

            // Checar capacidade de r1 apos trocar j1 por j2
            if (load[r1] - demand_j1 + demand_j2 > instance.capacity(r1)) continue;
            // Checar capacidade de r2 apos trocar j2 por j1
            if (load[r2] - demand_j2 + demand_j1 > instance.capacity(r2)) continue;

            const double cost_j2 = dm(j2, r2);
            const double delta = dm(j1, r2) + dm(j2, r1) - cost_j1 - cost_j2;

            if (delta < best.delta - kImprovementEps) {
                best.client1 = j1;
                best.client2 = j2;
                best.delta = delta;
                best.found = true;
            }
        }
    }

    return best;
}

MoveM2 bestM2(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm) {
    MoveM2 best{};
    best.delta = 0.0;
    best.found = false;

    const int n = instance.numNodes();
    const std::vector<int>& medians = solution.medians();
    const std::vector<int>& va = solution.assignments();
    const std::vector<double>& load = solution.load();
    const int p = static_cast<int>(medians.size());

    // Construir clusters e marcar medianas
    std::vector<std::vector<int>> clusters(n);
    std::vector<bool> is_median(n, false);
    for (int j = 0; j < n; ++j) {
        clusters[va[j]].push_back(j);
    }
    for (int k = 0; k < p; ++k) {
        is_median[medians[k]] = true;
    }

    for (int k = 0; k < p; ++k) {
        const int r1 = medians[k];
        const std::vector<int>& cluster = clusters[r1];

        // Custo atual do cluster
        double old_cluster_cost = 0.0;
        for (int j : cluster) {
            old_cluster_cost += dm(j, r1);
        }

        // Testar cada nao-mediana do mesmo cluster como substituta
        for (int r2 : cluster) {
            if (is_median[r2]) continue;

            // Viabilidade: carga do cluster cabe em r2
            if (load[r1] > instance.capacity(r2)) continue;

            double new_cluster_cost = 0.0;
            for (int j : cluster) {
                new_cluster_cost += dm(j, r2);
            }
            const double delta = new_cluster_cost - old_cluster_cost;

            if (delta < best.delta - kImprovementEps) {
                best.old_median = r1;
                best.new_median = r2;
                best.delta = delta;
                best.found = true;
            }
        }
    }

    return best;
}

MoveM3 bestM3(const Solution& solution,
              const Instance& instance,
              const DistanceMatrix& dm) {
    MoveM3 best{};
    best.delta = 0.0;
    best.found = false;

    const int n = instance.numNodes();
    const std::vector<int>& medians = solution.medians();
    const std::vector<int>& va = solution.assignments();
    const int p = static_cast<int>(medians.size());

    // Construir clusters e marcar medianas
    std::vector<std::vector<int>> clusters(n);
    std::vector<bool> is_median(n, false);
    for (int j = 0; j < n; ++j) {
        clusters[va[j]].push_back(j);
    }
    for (int k = 0; k < p; ++k) {
        is_median[medians[k]] = true;
    }

    for (int ki = 0; ki < p; ++ki) {
        const int r1 = medians[ki];
        const std::vector<int>& cluster = clusters[r1];
        const int csize = static_cast<int>(cluster.size());

        // Custo atual do cluster de r1
        double old_cost = 0.0;
        for (int j : cluster) {
            old_cost += dm(j, r1);
        }

        // Precomputar: para cada orfao, distancia ao mais proximo excluindo r1
        // Avaliacao sem restricao de capacidade (lower bound no delta real)
        std::vector<double> nearest_dist(csize);
        for (int ci = 0; ci < csize; ++ci) {
            const int j = cluster[ci];
            double best_d = std::numeric_limits<double>::max();
            for (int mk = 0; mk < p; ++mk) {
                const int m = medians[mk];
                if (m == r1) continue;
                best_d = std::min(best_d, dm(j, m));
            }
            nearest_dist[ci] = best_d;
        }

        // Testar cada nao-mediana r2 de outro cluster
        for (int r2 = 0; r2 < n; ++r2) {
            if (is_median[r2]) continue;
            if (va[r2] == r1) continue;

            // Delta sem restricao de capacidade:
            // cada orfao vai ao min(r2, nearest_existing)
            double new_cost = 0.0;
            for (int ci = 0; ci < csize; ++ci) {
                new_cost += std::min(dm(cluster[ci], r2), nearest_dist[ci]);
            }

            const double delta = new_cost - old_cost - dm(r2, va[r2]);

            if (delta < best.delta - kImprovementEps) {
                best.old_median = r1;
                best.new_median = r2;
                best.delta = delta;
                best.found = true;
            }
        }
    }

    return best;
}

void applyMove(Solution& solution, const MoveM1& move, const Instance& instance) {
    solution.applyReallocation(move.client, move.old_median, move.new_median,
                               move.delta, instance.demand(move.client));
}

void applyMove(Solution& solution, const MoveM2& move, const Instance& instance,
               const DistanceMatrix& dm) {
    const int r1 = move.old_median;
    const int r2 = move.new_median;
    const int n = instance.numNodes();
    const std::vector<int>& va = solution.assignments();

    // Substituir mediana
    solution.replaceMedian(r1, r2);

    // Mover todos os clientes de r1 para r2
    std::vector<int> clients;
    for (int j = 0; j < n; ++j) {
        if (va[j] == r1) clients.push_back(j);
    }

    for (int j : clients) {
        const double delta_j = dm(j, r2) - dm(j, r1);
        solution.applyReallocation(j, r1, r2, delta_j, instance.demand(j));
    }
}

void applyMove(Solution& solution, const MoveM3& move, const Instance& instance,
               const DistanceMatrix& dm) {
    const int r1 = move.old_median;
    const int r2 = move.new_median;
    const int n = instance.numNodes();
    const int r_old = solution.assignments()[r2];

    // 1. Remover r2 do cluster antigo
    solution.applyReallocation(r2, r_old, r2,
                               -dm(r2, r_old), instance.demand(r2));

    // 2. Substituir r1 por r2 no vetor de medianas
    solution.replaceMedian(r1, r2);

    // 3. Cada orfao vai ao mais proximo viavel (incluindo r2)
    const std::vector<int>& va = solution.assignments();
    std::vector<int> orphans;
    for (int j = 0; j < n; ++j) {
        if (va[j] == r1) orphans.push_back(j);
    }

    const std::vector<int>& medians = solution.medians();
    for (int j : orphans) {
        const double old_cost_j = dm(j, r1);
        double best_d = std::numeric_limits<double>::max();
        int best_m = -1;

        for (int m : medians) {
            if (solution.load()[m] + instance.demand(j) <= instance.capacity(m)) {
                const double d = dm(j, m);
                if (d < best_d || (d == best_d && m < best_m)) {
                    best_d = d;
                    best_m = m;
                }
            }
        }

        solution.applyReallocation(j, r1, best_m,
                                   best_d - old_cost_j, instance.demand(j));
    }
}

void applyMove(Solution& solution, const MoveM4& move, const Instance& instance) {
    solution.applySwap(move.client1, move.client2, move.delta,
                       instance.demand(move.client1),
                       instance.demand(move.client2));
}
