#include "ils.h"
#include "perturbation.h"
#include "vnd.h"

ILS::ILS(const Instance& instance, const DistanceMatrix& dm, int num_iter_max)
    : instance_(instance), distance_matrix_(dm), num_iter_max_(num_iter_max),
      total_iterations_(0), improvements_(0) {}

Solution ILS::run(Solution s, std::mt19937& rng) {
    Solution best = s;
    int iter_without_improvement = 0;
    int level = 1;
    total_iterations_ = 0;
    improvements_ = 0;

    while (iter_without_improvement < num_iter_max_) {
        // Perturbacao
        Solution s_prime = s;
        perturbate(s_prime, level, instance_, distance_matrix_, rng);

        // Busca local VND
        VND vnd(instance_, distance_matrix_);
        vnd.run(s_prime);

        ++total_iterations_;

        if (s_prime.cost() < s.cost()) {
            s = s_prime;
            if (s.cost() < best.cost()) {
                best = s;
            }
            iter_without_improvement = 0;
            level = 1;
            ++improvements_;
        } else {
            ++iter_without_improvement;
            if (level < 3) ++level;
        }
    }

    return best;
}

int ILS::totalIterations() const { return total_iterations_; }
int ILS::improvements() const { return improvements_; }
