#include "neighborhoods.h"

#include <limits>

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
        const double cost_r1 = dm.at(j, r1);
        const double demand_j = instance.demand(j);

        for (int k = 0; k < p; ++k) {
            const int r2 = medians[k];
            if (r2 == r1) continue;

            // Checar capacidade
            if (load[r2] + demand_j > instance.capacity(r2)) continue;

            const double delta = dm.at(j, r2) - cost_r1;

            if (delta < best.delta) {
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
        const double cost_j1 = dm.at(j1, r1);

        for (int j2 = j1 + 1; j2 < n; ++j2) {
            const int r2 = va[j2];
            if (r1 == r2) continue;  // mesmo cluster, pula

            const double demand_j2 = instance.demand(j2);

            // Checar capacidade de r1 apos trocar j1 por j2
            if (load[r1] - demand_j1 + demand_j2 > instance.capacity(r1)) continue;
            // Checar capacidade de r2 apos trocar j2 por j1
            if (load[r2] - demand_j2 + demand_j1 > instance.capacity(r2)) continue;

            const double cost_j2 = dm.at(j2, r2);
            const double delta = dm.at(j1, r2) + dm.at(j2, r1) - cost_j1 - cost_j2;

            if (delta < best.delta) {
                best.client1 = j1;
                best.client2 = j2;
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

void applyMove(Solution& solution, const MoveM4& move, const Instance& instance) {
    solution.applySwap(move.client1, move.client2, move.delta,
                       instance.demand(move.client1),
                       instance.demand(move.client2));
}
