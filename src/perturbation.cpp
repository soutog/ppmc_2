#include "perturbation.h"

void perturbate(Solution& solution, int level,
                const Instance& instance, const DistanceMatrix& dm,
                std::mt19937& rng) {
    const int n = instance.numNodes();
    const int num_swaps = level + 1;
    const int max_attempts = 20 * n;

    std::uniform_int_distribution<int> dist(0, n - 1);

    int swaps_done = 0;

    while (swaps_done < num_swaps) {
        bool found = false;

        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            const int j1 = dist(rng);
            const int j2 = dist(rng);

            if (j1 == j2) continue;

            const int r1 = solution.assignments()[j1];
            const int r2 = solution.assignments()[j2];

            if (r1 == r2) continue;

            const double demand_j1 = instance.demand(j1);
            const double demand_j2 = instance.demand(j2);

            if (solution.load()[r1] - demand_j1 + demand_j2 > instance.capacity(r1))
                continue;
            if (solution.load()[r2] - demand_j2 + demand_j1 > instance.capacity(r2))
                continue;

            const double delta = dm(j1, r2) + dm(j2, r1)
                               - dm(j1, r1) - dm(j2, r2);
            solution.applySwap(j1, j2, delta, demand_j1, demand_j2);

            found = true;
            break;
        }

        if (!found) break;  // nao achou troca viavel, encerra cedo
        ++swaps_done;
    }
}
