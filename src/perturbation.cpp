#include "perturbation.h"

#include <vector>

void perturbate(Solution& solution, int level,
                const Instance& instance, const DistanceMatrix& dm,
                std::mt19937& rng) {
    const int n = instance.numNodes();
    const int num_swaps = level + 1;
    struct FeasibleSwap {
        int client1;
        int client2;
        double delta;
        double demand1;
        double demand2;
    };

    for (int swaps_done = 0; swaps_done < num_swaps; ++swaps_done) {
        std::vector<FeasibleSwap> feasible_swaps;

        for (int j1 = 0; j1 < n; ++j1) {
            const int r1 = solution.assignments()[j1];
            const double demand_j1 = instance.demand(j1);

            for (int j2 = j1 + 1; j2 < n; ++j2) {
                const int r2 = solution.assignments()[j2];
                if (r1 == r2) continue;

                const double demand_j2 = instance.demand(j2);

                if (solution.load()[r1] - demand_j1 + demand_j2 > instance.capacity(r1)) {
                    continue;
                }
                if (solution.load()[r2] - demand_j2 + demand_j1 > instance.capacity(r2)) {
                    continue;
                }

                const double delta = dm(j1, r2) + dm(j2, r1)
                                   - dm(j1, r1) - dm(j2, r2);
                feasible_swaps.push_back({j1, j2, delta, demand_j1, demand_j2});
            }
        }

        if (feasible_swaps.empty()) {
            break;
        }

        std::uniform_int_distribution<std::size_t> dist(0, feasible_swaps.size() - 1);
        const FeasibleSwap& selected = feasible_swaps[dist(rng)];
        solution.applySwap(selected.client1, selected.client2, selected.delta,
                           selected.demand1, selected.demand2);
    }
}
