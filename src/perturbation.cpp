#include "perturbation.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace {

struct FeasibleSwap {
    int client1;
    int client2;
    double delta;
    double demand1;
    double demand2;
};

struct RepairCandidate {
    int client;
    double demand;
    double regret;
    double best_distance;
};

std::vector<FeasibleSwap> enumerateFeasibleSwaps(const Solution& solution,
                                                 const Instance& instance,
                                                 const DistanceMatrix& dm) {
    const int n = instance.numNodes();
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

    return feasible_swaps;
}

bool applyRandomFeasibleSwap(Solution& solution,
                             const Instance& instance,
                             const DistanceMatrix& dm,
                             std::mt19937& rng) {
    const std::vector<FeasibleSwap> feasible_swaps =
        enumerateFeasibleSwaps(solution, instance, dm);
    if (feasible_swaps.empty()) {
        return false;
    }

    std::uniform_int_distribution<std::size_t> dist(0, feasible_swaps.size() - 1);
    const FeasibleSwap& selected = feasible_swaps[dist(rng)];
    solution.applySwap(selected.client1, selected.client2, selected.delta,
                       selected.demand1, selected.demand2);
    return true;
}

bool repairAssignmentsAfterMedianChange(Solution& solution,
                                        int closed_median,
                                        const Instance& instance,
                                        const DistanceMatrix& dm) {
    const int n = instance.numNodes();
    const std::vector<int>& assignments = solution.assignments();
    const std::vector<int>& medians = solution.medians();

    std::vector<RepairCandidate> orphans;
    orphans.reserve(n);

    for (int client = 0; client < n; ++client) {
        if (assignments[client] != closed_median) continue;

        double best_distance = std::numeric_limits<double>::infinity();
        double second_best_distance = std::numeric_limits<double>::infinity();
        int feasible_count = 0;

        for (int median : medians) {
            if (solution.load()[median] + instance.demand(client) > instance.capacity(median)) {
                continue;
            }

            ++feasible_count;
            const double distance = dm(client, median);
            if (distance < best_distance) {
                second_best_distance = best_distance;
                best_distance = distance;
            } else if (distance < second_best_distance) {
                second_best_distance = distance;
            }
        }

        if (feasible_count == 0) {
            return false;
        }

        const double regret =
            (feasible_count == 1 ? std::numeric_limits<double>::infinity()
                                 : second_best_distance - best_distance);
        orphans.push_back({client, instance.demand(client), regret, best_distance});
    }

    std::sort(orphans.begin(), orphans.end(),
              [](const RepairCandidate& lhs, const RepairCandidate& rhs) {
                  if (lhs.demand != rhs.demand) {
                      return lhs.demand > rhs.demand;
                  }
                  if (lhs.regret != rhs.regret) {
                      return lhs.regret > rhs.regret;
                  }
                  if (lhs.best_distance != rhs.best_distance) {
                      return lhs.best_distance < rhs.best_distance;
                  }
                  return lhs.client < rhs.client;
              });

    for (const RepairCandidate& orphan : orphans) {
        double best_distance = std::numeric_limits<double>::infinity();
        int best_median = -1;

        for (int median : medians) {
            if (solution.load()[median] + instance.demand(orphan.client) >
                instance.capacity(median)) {
                continue;
            }

            const double distance = dm(orphan.client, median);
            if (distance < best_distance ||
                (distance == best_distance && median < best_median)) {
                best_distance = distance;
                best_median = median;
            }
        }

        if (best_median < 0) {
            return false;
        }

        solution.applyReallocation(orphan.client, closed_median, best_median,
                                   best_distance - dm(orphan.client, closed_median),
                                   instance.demand(orphan.client));
    }

    return true;
}

bool applyRandomMedianReplacement(Solution& solution,
                                  const Instance& instance,
                                  const DistanceMatrix& dm,
                                  std::mt19937& rng) {
    const std::vector<int>& current_medians = solution.medians();
    const int n = instance.numNodes();
    if (current_medians.empty() || static_cast<int>(current_medians.size()) == n) {
        return false;
    }

    std::vector<bool> is_median(n, false);
    for (int median : current_medians) {
        is_median[median] = true;
    }

    std::vector<int> non_medians;
    non_medians.reserve(n - static_cast<int>(current_medians.size()));
    for (int node = 0; node < n; ++node) {
        if (!is_median[node]) {
            non_medians.push_back(node);
        }
    }
    if (non_medians.empty()) {
        return false;
    }

    std::uniform_int_distribution<int> dist_old(0, static_cast<int>(current_medians.size()) - 1);
    std::vector<int> candidate_new = non_medians;
    std::shuffle(candidate_new.begin(), candidate_new.end(), rng);

    const int old_median = current_medians[dist_old(rng)];
    for (int new_median : candidate_new) {
        Solution backup = solution;
        const int previous_median_of_new = solution.assignments()[new_median];

        solution.applyReallocation(new_median, previous_median_of_new, new_median,
                                   -dm(new_median, previous_median_of_new),
                                   instance.demand(new_median));
        solution.replaceMedian(old_median, new_median);

        if (repairAssignmentsAfterMedianChange(solution, old_median, instance, dm)) {
            return true;
        }

        solution = backup;
    }

    return false;
}

}  // namespace

void perturbate(Solution& solution, int level,
                const Instance& instance, const DistanceMatrix& dm,
                std::mt19937& rng) {
    // Escalonamento da perturbacao por nivel:
    //   1: 1 substituicao estrutural              (intensificacao leve)
    //   2: 2 substituicoes estruturais            (intensificacao media)
    //   3: 2 substituicoes + 1 swap M4            (diversificacao)
    //   4: 3 substituicoes estruturais            (estagnacao profunda)
    //   5+: 3 substituicoes + 1 swap M4           (diversificacao maxima)
    int structural_moves;
    int swap_moves;
    switch (level) {
    case 1: structural_moves = 1; swap_moves = 0; break;
    case 2: structural_moves = 2; swap_moves = 0; break;
    case 3: structural_moves = 2; swap_moves = 1; break;
    case 4: structural_moves = 3; swap_moves = 0; break;
    default: structural_moves = 3; swap_moves = 1; break;
    }

    // A perturbacao estrutural altera as medianas; swaps M4 entram como diversificacao extra.
    for (int move = 0; move < structural_moves; ++move) {
        if (!applyRandomMedianReplacement(solution, instance, dm, rng)) {
            applyRandomFeasibleSwap(solution, instance, dm, rng);
        }
    }

    for (int move = 0; move < swap_moves; ++move) {
        if (!applyRandomFeasibleSwap(solution, instance, dm, rng)) {
            break;
        }
    }
}
