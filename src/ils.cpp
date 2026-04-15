#include "ils.h"
#include "clustering_search.h"
#include "perturbation.h"
#include "vnd.h"

#include <chrono>

namespace {
constexpr double kImprovementEps = 1e-9;
}  // namespace

ILS::ILS(const Instance& instance,
         const DistanceMatrix& dm,
         const NeighborhoodCache& nh_cache,
         int num_iter_max,
         double time_limit_s)
    : instance_(instance), distance_matrix_(dm),
      nh_cache_(nh_cache),
      num_iter_max_(num_iter_max),
      time_limit_s_(time_limit_s),
      stop_reason_(0),
      total_iterations_(0), improvements_(0) {}

Solution ILS::run(Solution s, std::mt19937& rng, ClusteringSearch* cs) {
    Solution best = s;
    int iter_without_improvement = 0;
    int level = 1;
    total_iterations_ = 0;
    improvements_ = 0;
    stop_reason_ = 1;  // default: iter_limit

    const auto t_start = std::chrono::steady_clock::now();
    const bool has_time_budget = time_limit_s_ > 0.0;

    while (iter_without_improvement < num_iter_max_) {
        if (has_time_budget) {
            const double elapsed =
                std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - t_start)
                    .count();
            if (elapsed >= time_limit_s_) {
                stop_reason_ = 2;  // time_limit
                break;
            }
        }

        // Perturbacao
        Solution s_prime = s;
        perturbate(s_prime, level, instance_, distance_matrix_, rng);

        // Busca local VND
        VND vnd(instance_, distance_matrix_, nh_cache_);
        vnd.run(s_prime);

        ++total_iterations_;

        const double s_cost_before_cs = s.cost();
        if (cs != nullptr) {
            // CS pode promover uma solucao melhorada pelo PO a s (ponto de
            // partida da proxima iteracao do ILS). Passamos &s para que
            // intensifyCluster substitua a solucao corrente quando aplicavel.
            cs->observe(s_prime, total_iterations_, best, &s);
        }

        // Caso 1: CS promoveu uma solucao melhor para s — aceita o novo
        // ponto de partida e reinicia o contador de nao-melhoria.
        if (s.cost() < s_cost_before_cs - kImprovementEps) {
            if (s.cost() < best.cost() - kImprovementEps) {
                best = s;
            }
            iter_without_improvement = 0;
            level = 1;
            ++improvements_;
            continue;
        }

        if (s_prime.cost() < s.cost() - kImprovementEps) {
            s = s_prime;
            if (s.cost() < best.cost() - kImprovementEps) {
                best = s;
            }
            iter_without_improvement = 0;
            level = 1;
            ++improvements_;
        } else {
            ++iter_without_improvement;
            // Escala normalmente ate o nivel 3 (teto do paper). Alem disso,
            // so sobe para 4-5 quando a estagnacao passa de metade de
            // NumIterMax — sinal de que a perturbacao padrao nao esta mais
            // achando bacias novas.
            if (level < 3) {
                ++level;
            } else if (level < 5 &&
                       iter_without_improvement > num_iter_max_ / 2) {
                ++level;
            }
        }
    }

    return best;
}

int ILS::totalIterations() const { return total_iterations_; }
int ILS::improvements() const { return improvements_; }
const char* ILS::stopReason() const {
    switch (stop_reason_) {
        case 1: return "iter_limit";
        case 2: return "time_limit";
        default: return "";
    }
}
