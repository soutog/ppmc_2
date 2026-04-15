#include "clustering_search.h"
#include "vnd.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>

ClusteringSearch::ClusteringSearch(const Instance& instance,
                                   const DistanceMatrix& dm,
                                   const NeighborhoodCache& nh_cache,
                                   const Evaluator& evaluator,
                                   PartialOptimizer* partial_optimizer,
                                   GRASPConstructor* grasp,
                                   int gamma,
                                   int max_volume,
                                   int max_inef,
                                   int distance_threshold,
                                   unsigned int seed)
    : instance_(instance),
      dm_(dm),
      nh_cache_(nh_cache),
      evaluator_(evaluator),
      partial_optimizer_(partial_optimizer),
      grasp_(grasp),
      gamma_(std::max(1, gamma)),
      max_volume_(std::max(1, max_volume)),
      max_inef_(std::max(1, max_inef)),
      distance_threshold_(distance_threshold >= 0
                              ? distance_threshold
                              : std::max(1, instance.numMedians() / 4)),
      destroy_k_(std::max(1, std::min(3, instance.numMedians() / 5))),
      rng_(seed),
      clusters_(gamma_) {}

int ClusteringSearch::distanceBetween(const Solution& a, const Solution& b) const {
    const int n = instance_.numNodes();
    std::vector<bool> in_b(n, false);
    for (int median : b.medians()) {
        in_b[median] = true;
    }

    int intersection = 0;
    for (int median : a.medians()) {
        if (in_b[median]) {
            ++intersection;
        }
    }

    return static_cast<int>(a.medians().size()) - intersection;
}

int ClusteringSearch::findBestCluster(const Solution& s, int* best_distance) const {
    int best_idx = -1;
    int best_dist = 0;

    for (int idx = 0; idx < gamma_; ++idx) {
        if (!clusters_[idx].initialized) {
            continue;
        }

        const int dist = distanceBetween(s, clusters_[idx].center);
        if (best_idx < 0 || dist < best_dist ||
            (dist == best_dist && clusters_[idx].center.cost() < clusters_[best_idx].center.cost())) {
            best_idx = idx;
            best_dist = dist;
        }
    }

    if (best_distance != nullptr) {
        *best_distance = best_dist;
    }

    return best_idx;
}

int ClusteringSearch::selectClusterFor(const Solution& s,
                                       int* best_distance,
                                       bool* created_new) const {
    const int nearest_idx = findBestCluster(s, best_distance);

    int empty_idx = -1;
    for (int idx = 0; idx < gamma_; ++idx) {
        if (!clusters_[idx].initialized) {
            empty_idx = idx;
            break;
        }
    }

    const bool should_create =
        empty_idx >= 0 && (nearest_idx < 0 || *best_distance > distance_threshold_);

    if (created_new != nullptr) {
        *created_new = should_create;
    }

    if (should_create) {
        if (best_distance != nullptr) {
            *best_distance = -1;
        }
        return empty_idx;
    }

    return nearest_idx;
}

void ClusteringSearch::updateCenter(ClusterInfo& cluster,
                                    const Solution& s,
                                    int iter) {
    if (!cluster.initialized) {
        cluster.center = s;
        cluster.volume = 1;
        cluster.hits = 1;
        cluster.last_update_iter = iter;
        cluster.initialized = true;
        stats_.max_cluster_volume =
            std::max(stats_.max_cluster_volume, cluster.volume);
        return;
    }

    ++cluster.volume;
    ++cluster.hits;
    cluster.last_update_iter = iter;
    stats_.max_cluster_volume =
        std::max(stats_.max_cluster_volume, cluster.volume);

    if (s.cost() < cluster.center.cost()) {
        cluster.center = s;
        cluster.ineff_count = 0;
        ++stats_.center_updates;
    }
}

void ClusteringSearch::intensifyCluster(ClusterInfo& cluster,
                                        Solution& best_global,
                                        Solution* ils_current) {
    if (partial_optimizer_ == nullptr || !cluster.initialized) {
        return;
    }

    ++stats_.po_triggers;

    Solution candidate = cluster.center;
    const double center_before = cluster.center.cost();

    partial_optimizer_->run(candidate);
    VND vnd(instance_, dm_, nh_cache_);
    vnd.run(candidate);

    std::string error;
    if (!evaluator_.evaluate(candidate, &error)) {
        cluster.volume = 0;
        ++cluster.ineff_count;
        return;
    }

    const double gain = center_before - candidate.cost();
    if (candidate.cost() < cluster.center.cost()) {
        cluster.center = candidate;
        cluster.volume = 0;
        cluster.ineff_count = 0;
        ++stats_.po_improvements;
        stats_.po_total_gain += gain;
        stats_.po_best_gain = std::max(stats_.po_best_gain, gain);
        ++stats_.center_updates;

        if (candidate.cost() < best_global.cost()) {
            best_global = candidate;
        }

        // Promove a nova solucao como ponto de partida do ILS se for melhor
        // do que a solucao corrente do ILS. Isso acopla o PO ao loop principal
        // e evita que melhorias do PO fiquem "presas" apenas em best_global.
        if (ils_current != nullptr &&
            candidate.cost() < ils_current->cost() - 1e-9) {
            *ils_current = candidate;
            ++stats_.ils_current_promotions;
        }
    } else {
        cluster.volume = 0;
        ++cluster.ineff_count;
    }
}

void ClusteringSearch::destroyRepairCluster(ClusterInfo& cluster,
                                            Solution& best_global) {
    if (grasp_ == nullptr || !cluster.initialized) {
        cluster.ineff_count = 0;
        return;
    }

    ++stats_.destroy_repair_calls;

    const int p = instance_.numMedians();
    const int n = instance_.numNodes();
    const int k = std::min(destroy_k_, std::max(1, p - 1));

    // Random-worst destroy: ordena medianas do centro por custo total do
    // cluster e remove as k piores. Preserva as p-k sobreviventes.
    const std::vector<int>& assignments = cluster.center.assignments();
    std::vector<double> cluster_cost(n, 0.0);
    for (int client = 0; client < n; ++client) {
        const int m = assignments[client];
        if (m >= 0) {
            cluster_cost[m] += dm_.at(client, m);
        }
    }

    std::vector<std::pair<double, int>> ordered;
    ordered.reserve(cluster.center.medians().size());
    for (int m : cluster.center.medians()) {
        ordered.push_back({cluster_cost[m], m});
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const std::pair<double, int>& lhs,
                 const std::pair<double, int>& rhs) {
                  return lhs.first > rhs.first;
              });

    std::unordered_set<int> survivors;
    survivors.reserve(p - k);
    for (std::size_t idx = static_cast<std::size_t>(k); idx < ordered.size(); ++idx) {
        survivors.insert(ordered[idx].second);
    }

    // Repair: seleciona k novas medianas da LRC do GRASP, excluindo
    // sobreviventes. Reconstrucao regret+recompute ja existente cuida do resto.
    std::vector<int> pool;
    pool.reserve(grasp_->restrictedCandidateList().size());
    for (int node : grasp_->restrictedCandidateList()) {
        if (survivors.find(node) == survivors.end()) {
            pool.push_back(node);
        }
    }

    if (static_cast<int>(pool.size()) < k) {
        // LRC muito pequena: amplia com nodes aleatorios do problema inteiro.
        for (int node = 0; node < n && static_cast<int>(pool.size()) < k + 10; ++node) {
            if (survivors.find(node) == survivors.end() &&
                std::find(pool.begin(), pool.end(), node) == pool.end()) {
                pool.push_back(node);
            }
        }
    }

    std::shuffle(pool.begin(), pool.end(), rng_);
    std::vector<int> new_medians(survivors.begin(), survivors.end());
    for (int idx = 0; idx < k && idx < static_cast<int>(pool.size()); ++idx) {
        new_medians.push_back(pool[idx]);
    }

    if (static_cast<int>(new_medians.size()) != p) {
        cluster.ineff_count = 0;
        return;
    }

    Solution rebuilt;
    std::string error;
    const bool ok = grasp_->reconstructFromMedians(new_medians, rebuilt, &error);
    if (!ok || !rebuilt.feasible()) {
        std::cout << "[CS_DESTROY] failed reason=" << error << "\n";
        cluster.ineff_count = 0;
        return;
    }

    const double old_cost = cluster.center.cost();
    cluster.center = rebuilt;
    cluster.volume = 0;
    cluster.ineff_count = 0;

    if (rebuilt.cost() < old_cost - 1e-9) {
        ++stats_.destroy_repair_improvements;
        ++stats_.center_updates;
        if (rebuilt.cost() < best_global.cost() - 1e-9) {
            best_global = rebuilt;
        }
    }

    std::cout << "[CS_DESTROY] k=" << k
              << " center_before=" << old_cost
              << " center_after=" << rebuilt.cost() << "\n";
}

void ClusteringSearch::observe(const Solution& local_opt,
                               int iter,
                               Solution& best_global,
                               Solution* ils_current) {
    ++stats_.observations;

    int best_distance = -1;
    bool created_new = false;
    const int cluster_idx =
        selectClusterFor(local_opt, &best_distance, &created_new);

    if (cluster_idx < 0) {
        return;
    }

    ClusterInfo& cluster = clusters_[cluster_idx];
    if (created_new) {
        ++stats_.new_clusters;
        ++stats_.active_clusters;
    } else {
        ++stats_.assignments_to_existing;
    }

    updateCenter(cluster, local_opt, iter);

    if (cluster.volume >= max_volume_) {
        intensifyCluster(cluster, best_global, ils_current);
    }

    // Destroy/repair desativado: no batch 2026-04-14 nao registrou nenhuma
    // melhoria em 60 runs (CS_DR sempre X/0). Mantemos a funcao para
    // reativacao futura apos investigar a logica de aceitacao.
    (void)best_global;
}

const ClusteringSearchStats& ClusteringSearch::stats() const { return stats_; }

const std::vector<ClusterInfo>& ClusteringSearch::clusters() const {
    return clusters_;
}
