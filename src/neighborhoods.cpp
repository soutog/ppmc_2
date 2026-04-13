#include "neighborhoods.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace {
constexpr double kImprovementEps = 1e-9;

bool evaluateExactM3Move(const Solution& solution,
                         int old_median,
                         int new_median,
                         const Instance& instance,
                         const DistanceMatrix& dm,
                         double* delta_out) {
    const int n = instance.numNodes();
    const int old_cluster_of_new = solution.assignments()[new_median];
    const std::vector<int>& medians = solution.medians();
    const std::vector<int>& assignments = solution.assignments();

    std::vector<double> load = solution.load();
    load[old_cluster_of_new] -= instance.demand(new_median);
    load[new_median] += instance.demand(new_median);

    double delta = -dm(new_median, old_cluster_of_new);

    for (int j = 0; j < n; ++j) {
        if (assignments[j] != old_median) continue;

        double best_distance = std::numeric_limits<double>::max();
        int best_median = -1;

        for (int median : medians) {
            const int candidate_median = (median == old_median ? new_median : median);
            if (load[candidate_median] + instance.demand(j) > instance.capacity(candidate_median)) {
                continue;
            }

            const double distance = dm(j, candidate_median);
            if (distance < best_distance ||
                (distance == best_distance && candidate_median < best_median)) {
                best_distance = distance;
                best_median = candidate_median;
            }
        }

        if (best_median < 0) {
            return false;
        }

        load[old_median] -= instance.demand(j);
        load[best_median] += instance.demand(j);
        delta += best_distance - dm(j, old_median);
    }

    if (delta_out != nullptr) {
        *delta_out = delta;
    }

    return true;
}

bool applyExactM3Move(Solution& solution,
                      int old_median,
                      int new_median,
                      const Instance& instance,
                      const DistanceMatrix& dm) {
    const int n = instance.numNodes();
    const int old_cluster_of_new = solution.assignments()[new_median];

    solution.applyReallocation(new_median, old_cluster_of_new, new_median,
                               -dm(new_median, old_cluster_of_new),
                               instance.demand(new_median));
    solution.replaceMedian(old_median, new_median);

    const std::vector<int>& assignments = solution.assignments();
    std::vector<int> orphans;
    orphans.reserve(n);
    for (int j = 0; j < n; ++j) {
        if (assignments[j] == old_median) {
            orphans.push_back(j);
        }
    }

    const std::vector<int>& medians = solution.medians();
    for (int j : orphans) {
        double best_distance = std::numeric_limits<double>::max();
        int best_median = -1;

        for (int median : medians) {
            if (solution.load()[median] + instance.demand(j) > instance.capacity(median)) {
                continue;
            }

            const double distance = dm(j, median);
            if (distance < best_distance ||
                (distance == best_distance && median < best_median)) {
                best_distance = distance;
                best_median = median;
            }
        }

        if (best_median < 0) {
            return false;
        }

        solution.applyReallocation(j, old_median, best_median,
                                   best_distance - dm(j, old_median),
                                   instance.demand(j));
    }

    return true;
}
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

    std::vector<bool> is_median(n, false);
    for (int k = 0; k < p; ++k) {
        is_median[medians[k]] = true;
    }

    for (int ki = 0; ki < p; ++ki) {
        const int r1 = medians[ki];

        auto consider = [&](int r2) {
            if (is_median[r2]) return;
            if (va[r2] == r1) return;

            double delta = 0.0;
            if (!evaluateExactM3Move(solution, r1, r2, instance, dm, &delta)) {
                return;
            }

            if (delta < best.delta - kImprovementEps) {
                best.old_median = r1;
                best.new_median = r2;
                best.delta = delta;
                best.found = true;
            }
        };

        for (int r2 = 0; r2 < n; ++r2) {
            consider(r2);
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
    Solution backup = solution;
    if (!applyExactM3Move(solution, move.old_median, move.new_median, instance, dm)) {
        solution = backup;
    }
}

void applyMove(Solution& solution, const MoveM4& move, const Instance& instance) {
    solution.applySwap(move.client1, move.client2, move.delta,
                       instance.demand(move.client1),
                       instance.demand(move.client2));
}
