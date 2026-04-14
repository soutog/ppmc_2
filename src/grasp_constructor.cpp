#include "grasp_constructor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>

namespace {
constexpr int kConstructionIterationLimit = 10;

struct RegretChoice {
    int client;
    int best_median;
    double best_distance;
    double regret;
    double demand;
    bool has_single_option;
};
}  // namespace

GRASPConstructor::GRASPConstructor(const Instance& instance,
                                   const DistanceMatrix& distance_matrix,
                                   const Evaluator& evaluator,
                                   const R1Filter* r1_filter,
                                   double alpha,
                                   int max_tries,
                                   unsigned int seed)
    : instance_(instance),
      distance_matrix_(distance_matrix),
      evaluator_(evaluator),
      r1_filter_(r1_filter),
      alpha_(alpha),
      max_tries_(max_tries),
      rng_(seed),
      last_attempts_(0) {
    buildRankedCandidates();
    buildRestrictedCandidateList();
}

void GRASPConstructor::buildRankedCandidates() {
    ranked_candidates_.clear();
    ranked_candidates_.reserve(instance_.numNodes());

    for (int candidate = 0; candidate < instance_.numNodes(); ++candidate) {
        double total_distance = 0.0;
        for (int client = 0; client < instance_.numNodes(); ++client) {
            total_distance += distance_matrix_.at(candidate, client);
        }

        ranked_candidates_.push_back({total_distance, candidate});
    }

    std::sort(ranked_candidates_.begin(),
              ranked_candidates_.end(),
              [](const std::pair<double, int>& lhs,
                 const std::pair<double, int>& rhs) {
                  if (lhs.first != rhs.first) {
                      return lhs.first < rhs.first;
                  }
                  return lhs.second < rhs.second;
              });
}

void GRASPConstructor::buildRestrictedCandidateList() {
    restricted_candidate_list_.clear();

    if (ranked_candidates_.empty()) {
        return;
    }

    const double g_min = ranked_candidates_.front().first;
    const double g_max = ranked_candidates_.back().first;
    const double threshold = g_min + alpha_ * (g_max - g_min);

    for (const auto& entry : ranked_candidates_) {
        if (entry.first <= threshold) {
            restricted_candidate_list_.push_back(entry.second);
        }
    }
}

std::vector<int> GRASPConstructor::selectRandomMedians() {
    std::vector<int> selected = restricted_candidate_list_;
    std::shuffle(selected.begin(), selected.end(), rng_);
    selected.resize(instance_.numMedians());
    return selected;
}

bool GRASPConstructor::assignClientsByRegret(const std::vector<int>& medians,
                                             std::vector<int>& assignments,
                                             std::string* error) {
    const int n = instance_.numNodes();
    std::vector<double> residual_capacity(n, 0.0);
    std::vector<bool> assigned(n, false);
    assignments.assign(n, -1);
    int assigned_count = 0;

    for (int median : medians) {
        residual_capacity[median] = instance_.capacity(median);
    }

    // Poda por R1: para cada cliente, pre-calcula a lista de medianas
    // correntes que sobrevivem ao filtro R1. Feito uma vez por chamada do
    // regret (nao por iteracao do while), amortizando o custo O(n*p).
    // Fallback: se a interseccao ficar vazia, usamos o vetor completo de
    // medianas (garante correcao em casos extremos).
    std::vector<std::vector<int>> allowed_medians_per_client(n);
    if (r1_filter_ != nullptr) {
        for (int client = 0; client < n; ++client) {
            allowed_medians_per_client[client].reserve(medians.size());
            for (int m : medians) {
                if (r1_filter_->inKi(client, m)) {
                    allowed_medians_per_client[client].push_back(m);
                }
            }
            if (allowed_medians_per_client[client].empty()) {
                allowed_medians_per_client[client] = medians;
            }
        }
    }
    const bool use_prune = (r1_filter_ != nullptr);

    while (assigned_count < n) {
        // Em cada passo, escolhemos o cliente mais "critico" dado o estado atual.
        RegretChoice selected{-1, -1, 0.0, 0.0, 0.0, false};
        bool found_candidate = false;

        for (int client = 0; client < n; ++client) {
            if (assigned[client]) {
                continue;
            }

            int best_median = -1;
            double best_distance = std::numeric_limits<double>::infinity();
            double second_best_distance = std::numeric_limits<double>::infinity();
            int feasible_count = 0;

            const std::vector<int>& candidate_medians =
                use_prune ? allowed_medians_per_client[client] : medians;

            for (int median : candidate_medians) {
                if (residual_capacity[median] + 1e-9 < instance_.demand(client)) {
                    continue;
                }

                ++feasible_count;
                const double distance = distance_matrix_.at(client, median);

                if (distance < best_distance ||
                    (distance == best_distance && median < best_median)) {
                    second_best_distance = best_distance;
                    best_distance = distance;
                    best_median = median;
                } else if (distance < second_best_distance) {
                    second_best_distance = distance;
                }
            }

            // Fallback: se a lista podada nao tem viavel mas o conjunto
            // completo tem, retesta com ele. Evita falha artificial.
            if (use_prune && best_median < 0) {
                for (int median : medians) {
                    if (residual_capacity[median] + 1e-9 < instance_.demand(client)) {
                        continue;
                    }
                    ++feasible_count;
                    const double distance = distance_matrix_.at(client, median);
                    if (distance < best_distance ||
                        (distance == best_distance && median < best_median)) {
                        second_best_distance = best_distance;
                        best_distance = distance;
                        best_median = median;
                    } else if (distance < second_best_distance) {
                        second_best_distance = distance;
                    }
                }
            }

            if (best_median < 0) {
                if (error != nullptr) {
                    std::ostringstream oss;
                    oss << "Falha de alocacao para o cliente " << client
                        << " durante a construcao inicial por regret.";
                    *error = oss.str();
                }
                return false;
            }

            RegretChoice current{
                client,
                best_median,
                best_distance,
                // Regret alto indica que o cliente perdera muito se nao pegar a melhor opcao agora.
                (feasible_count == 1
                     ? std::numeric_limits<double>::infinity()
                     : second_best_distance - best_distance),
                instance_.demand(client),
                feasible_count == 1};

            bool better_choice = false;
            if (!found_candidate) {
                better_choice = true;
            } else if (current.has_single_option != selected.has_single_option) {
                better_choice = current.has_single_option;
            } else if (current.regret > selected.regret + 1e-9) {
                better_choice = true;
            } else if (std::abs(current.regret - selected.regret) <= 1e-9 &&
                       current.demand > selected.demand + 1e-9) {
                better_choice = true;
            } else if (std::abs(current.regret - selected.regret) <= 1e-9 &&
                       std::abs(current.demand - selected.demand) <= 1e-9 &&
                       current.best_distance < selected.best_distance - 1e-9) {
                better_choice = true;
            } else if (std::abs(current.regret - selected.regret) <= 1e-9 &&
                       std::abs(current.demand - selected.demand) <= 1e-9 &&
                       std::abs(current.best_distance - selected.best_distance) <= 1e-9 &&
                       current.client < selected.client) {
                better_choice = true;
            }

            if (better_choice) {
                selected = current;
                found_candidate = true;
            }
        }

        assignments[selected.client] = selected.best_median;
        assigned[selected.client] = true;
        residual_capacity[selected.best_median] -= instance_.demand(selected.client);
        ++assigned_count;
    }

    if (error != nullptr) {
        error->clear();
    }

    return true;
}

std::vector<int> GRASPConstructor::recomputeClusterMedians(
    const std::vector<int>& medians,
    const std::vector<int>& assignments) const {
    const int n = instance_.numNodes();
    std::vector<std::vector<int>> clusters(n);
    for (int client = 0; client < n; ++client) {
        const int median = assignments[client];
        if (median >= 0) {
            clusters[median].push_back(client);
        }
    }

    std::vector<int> recomputed = medians;
    for (std::size_t idx = 0; idx < medians.size(); ++idx) {
        const int current_median = medians[idx];
        const std::vector<int>& cluster = clusters[current_median];
        if (cluster.empty()) {
            continue;
        }

        double cluster_load = 0.0;
        for (int client : cluster) {
            cluster_load += instance_.demand(client);
        }

        int best_candidate = current_median;
        double best_cost = std::numeric_limits<double>::infinity();
        // A nova mediana precisa ser interna ao cluster e suportar toda a carga atual.
        for (int candidate : cluster) {
            if (instance_.capacity(candidate) + 1e-9 < cluster_load) {
                continue;
            }

            double total_distance = 0.0;
            for (int client : cluster) {
                total_distance += distance_matrix_.at(client, candidate);
            }

            if (total_distance < best_cost ||
                (total_distance == best_cost && candidate < best_candidate)) {
                best_cost = total_distance;
                best_candidate = candidate;
            }
        }

        recomputed[idx] = best_candidate;
    }

    return recomputed;
}

bool GRASPConstructor::buildIterativeSolution(const std::vector<int>& initial_medians,
                                              Solution& solution,
                                              std::string* error) {
    const int n = instance_.numNodes();
    std::vector<int> medians = initial_medians;
    std::vector<int> assignments(n, -1);

    // Early-stop por custo estavel: na pratica, o loop alterna regret <->
    // recompute e o conjunto de medianas pode oscilar entre duas configuracoes
    // equivalentes, nunca disparando a saida por igualdade. Monitoramos o
    // custo e saimos quando estabiliza (tolerancia relativa 1e-9).
    double previous_cost = -1.0;

    for (int iteration = 0; iteration < kConstructionIterationLimit; ++iteration) {
        if (!assignClientsByRegret(medians, assignments, error)) {
            return false;
        }

        double current_cost = 0.0;
        for (int client = 0; client < n; ++client) {
            current_cost += distance_matrix_.at(client, assignments[client]);
        }

        const bool stable_cost =
            previous_cost >= 0.0 &&
            std::abs(current_cost - previous_cost) <
                1e-9 * std::max(1.0, std::abs(previous_cost));

        if (stable_cost) {
            solution.reset(n);
            solution.setMedians(medians);
            solution.setAssignments(assignments);
            return evaluator_.evaluate(solution, error);
        }
        previous_cost = current_cost;

        const std::vector<int> recomputed_medians =
            recomputeClusterMedians(medians, assignments);

        if (recomputed_medians == medians) {
            solution.reset(n);
            solution.setMedians(medians);
            solution.setAssignments(assignments);
            return evaluator_.evaluate(solution, error);
        }

        medians = recomputed_medians;
    }

    // Se nao estabilizar cedo, validamos a melhor configuracao obtida no limite de iteracoes.
    if (!assignClientsByRegret(medians, assignments, error)) {
        return false;
    }

    solution.reset(n);
    solution.setMedians(medians);
    solution.setAssignments(assignments);
    return evaluator_.evaluate(solution, error);
}

Solution GRASPConstructor::construct() {
    Solution solution(instance_.numNodes());
    last_attempts_ = 0;
    last_error_.clear();

    if (static_cast<int>(restricted_candidate_list_.size()) < instance_.numMedians()) {
        last_error_ =
            "LRC menor que p. Nao ha candidatos suficientes para selecionar as medianas.";
        return solution;
    }

    for (int attempt = 1; attempt <= max_tries_; ++attempt) {
        last_attempts_ = attempt;
        const std::vector<int> medians = selectRandomMedians();

        std::string error;
        if (buildIterativeSolution(medians, solution, &error)) {
            last_error_.clear();
            return solution;
        }

        last_error_ = error;
    }

    solution.reset(instance_.numNodes());
    return solution;
}

bool GRASPConstructor::reconstructFromMedians(const std::vector<int>& medians,
                                               Solution& solution,
                                               std::string* error) {
    if (static_cast<int>(medians.size()) != instance_.numMedians()) {
        if (error != nullptr) {
            *error = "reconstructFromMedians: tamanho do conjunto diferente de p.";
        }
        return false;
    }
    return buildIterativeSolution(medians, solution, error);
}

double GRASPConstructor::alpha() const {
    return alpha_;
}

int GRASPConstructor::maxTries() const {
    return max_tries_;
}

int GRASPConstructor::lastAttempts() const {
    return last_attempts_;
}

const std::string& GRASPConstructor::lastError() const {
    return last_error_;
}

const std::vector<std::pair<double, int>>& GRASPConstructor::rankedCandidates() const {
    return ranked_candidates_;
}

const std::vector<int>& GRASPConstructor::restrictedCandidateList() const {
    return restricted_candidate_list_;
}
