#include "grasp_constructor.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <sstream>

GRASPConstructor::GRASPConstructor(const Instance& instance,
                                   const DistanceMatrix& distance_matrix,
                                   const Evaluator& evaluator,
                                   double alpha,
                                   int max_tries,
                                   unsigned int seed)
    : instance_(instance),
      distance_matrix_(distance_matrix),
      evaluator_(evaluator),
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

bool GRASPConstructor::assignClientsToNearestMedian(const std::vector<int>& medians,
                                                    Solution& solution,
                                                    std::string* error) {
    const int n = instance_.numNodes();
    std::vector<int> assignments(n, -1);
    std::vector<double> residual_capacity(n, 0.0);
    std::vector<int> clients(n);

    for (int median : medians) {
        residual_capacity[median] = instance_.capacity(median);
    }

    std::iota(clients.begin(), clients.end(), 0);
    std::shuffle(clients.begin(), clients.end(), rng_);

    for (int client : clients) {
        int best_median = -1;
        double best_distance = std::numeric_limits<double>::infinity();

        for (int median : medians) {
            if (residual_capacity[median] < instance_.demand(client)) {
                continue;
            }

            const double distance = distance_matrix_.at(client, median);
            if (distance < best_distance ||
                (distance == best_distance && median < best_median)) {
                best_distance = distance;
                best_median = median;
            }
        }

        if (best_median < 0) {
            if (error != nullptr) {
                std::ostringstream oss;
                oss << "Falha de alocacao para o cliente " << client
                    << " durante a construcao inicial.";
                *error = oss.str();
            }
            return false;
        }

        assignments[client] = best_median;
        residual_capacity[best_median] -= instance_.demand(client);
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
        if (assignClientsToNearestMedian(medians, solution, &error)) {
            last_error_.clear();
            return solution;
        }

        last_error_ = error;
    }

    solution.reset(instance_.numNodes());
    return solution;
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
