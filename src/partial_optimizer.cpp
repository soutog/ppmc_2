#include "partial_optimizer.h"

#include <ilcplex/ilocplex.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>

ILOSTLBEGIN

namespace {
constexpr double kAcceptImprovementEps = 1e-6;

std::string cplexStatusToString(IloAlgorithm::Status status) {
    switch (status) {
    case IloAlgorithm::Unknown:
        return "Unknown";
    case IloAlgorithm::Feasible:
        return "Feasible";
    case IloAlgorithm::Optimal:
        return "Optimal";
    case IloAlgorithm::Infeasible:
        return "Infeasible";
    case IloAlgorithm::Unbounded:
        return "Unbounded";
    case IloAlgorithm::InfeasibleOrUnbounded:
        return "InfOrUnbd";
    case IloAlgorithm::Error:
        return "Error";
    default:
        return "Other";
    }
}

}  // namespace

PartialOptimizer::PartialOptimizer(const Instance& instance,
                                   const DistanceMatrix& dm,
                                   const Evaluator& evaluator,
                                   const R1Filter* r1_filter,
                                   int omega,
                                   int min_clusters,
                                   int max_clusters_free,
                                   double time_limit_s,
                                   int beta_r2,
                                   int r2_min_nodes,
                                   double r2_min_ratio,
                                   int max_calls,
                                   int max_no_improve,
                                   double total_time_budget_s,
                                   unsigned int seed)
    : instance_(instance),
      dm_(dm),
      evaluator_(evaluator),
      r1_filter_(r1_filter),
      omega_(omega),
      min_clusters_(min_clusters),
      max_clusters_free_(max_clusters_free),
      time_limit_s_(time_limit_s),
      beta_r2_(beta_r2),
      r2_min_nodes_(r2_min_nodes),
      r2_min_ratio_(r2_min_ratio),
      max_calls_(max_calls),
      max_no_improve_(max_no_improve),
      total_time_budget_s_(total_time_budget_s),
      rng_(seed) {}

int PartialOptimizer::selectReferenceCluster(const Solution& solution) const {
    const int n = instance_.numNodes();
    std::vector<int> cluster_size(n, 0);
    std::vector<double> cluster_cost(n, 0.0);
    const std::vector<int>& assignments = solution.assignments();
    for (int client = 0; client < n; ++client) {
        const int m = assignments[client];
        cluster_size[m] += 1;
        cluster_cost[m] += dm_.at(client, m);
    }

    // Score hibrido: 0.7 * custo_total + 0.3 * custo_medio. Coleta todos os
    // medianas com score e sorteia entre os top-3 para diversificar chamadas
    // sucessivas do PO sobre solucoes muito parecidas.
    std::vector<std::pair<double, int>> scored;
    scored.reserve(solution.medians().size());
    for (int median : solution.medians()) {
        if (cluster_size[median] == 0) {
            continue;
        }
        const double total = cluster_cost[median];
        const double mean = total / static_cast<double>(cluster_size[median]);
        const double score = 0.7 * total + 0.3 * mean;
        scored.push_back({score, median});
    }

    if (scored.empty()) {
        return -1;
    }

    std::sort(scored.begin(), scored.end(),
              [](const std::pair<double, int>& lhs,
                 const std::pair<double, int>& rhs) {
                  if (lhs.first != rhs.first) return lhs.first > rhs.first;
                  return lhs.second < rhs.second;
              });

    const int pool_size =
        std::min(3, static_cast<int>(scored.size()));
    std::uniform_int_distribution<int> dist(0, pool_size - 1);
    return scored[dist(rng_)].second;
}

std::vector<int> PartialOptimizer::selectNeighborhoodClusters(
    const Solution& solution,
    int ref_median,
    int effective_min_clusters,
    int effective_max_clusters) const {
    const std::vector<int>& medians = solution.medians();
    const int n = instance_.numNodes();

    // Precomputa tamanho de cada cluster uma unica vez (O(n)).
    std::vector<int> cluster_size(n, 0);
    for (int a : solution.assignments()) {
        cluster_size[a] += 1;
    }

    std::vector<std::pair<double, int>> ordered;
    ordered.reserve(medians.size());
    for (int median : medians) {
        ordered.push_back({dm_.at(ref_median, median), median});
    }

    std::sort(ordered.begin(), ordered.end(),
              [](const std::pair<double, int>& lhs,
                 const std::pair<double, int>& rhs) {
                  if (lhs.first != rhs.first) {
                      return lhs.first < rhs.first;
                  }
                  return lhs.second < rhs.second;
              });

    std::vector<int> selected;
    selected.reserve(medians.size());
    int accumulated_nodes = 0;
    effective_min_clusters =
        std::min(std::max(1, effective_min_clusters), static_cast<int>(medians.size()));
    effective_max_clusters =
        std::min(std::max(effective_min_clusters, effective_max_clusters),
                 static_cast<int>(medians.size()));

    for (const auto& [distance, median] : ordered) {
        (void)distance;
        selected.push_back(median);
        accumulated_nodes += cluster_size[median];

        const bool reached_min = static_cast<int>(selected.size()) >= effective_min_clusters;
        const bool reached_omega = accumulated_nodes >= omega_;
        const bool reached_max_clusters =
            static_cast<int>(selected.size()) >= effective_max_clusters;

        if (reached_min && (reached_omega || reached_max_clusters)) {
            break;
        }
    }

    return selected;
}

std::vector<int> PartialOptimizer::collectFreeNodes(
    const Solution& solution, const std::vector<int>& free_medians) const {
    std::vector<bool> is_free_median(instance_.numNodes(), false);
    for (int median : free_medians) {
        is_free_median[median] = true;
    }

    std::vector<int> free_nodes;
    const std::vector<int>& assignments = solution.assignments();
    free_nodes.reserve(instance_.numNodes());
    for (int client = 0; client < instance_.numNodes(); ++client) {
        if (is_free_median[assignments[client]]) {
            free_nodes.push_back(client);
        }
    }

    return free_nodes;
}

std::vector<std::vector<int>> PartialOptimizer::buildReducedCandidateLists(
    const std::vector<int>& free_nodes,
    const std::vector<char>& allowed_open_mask) const {
    std::vector<std::vector<int>> candidate_lists(free_nodes.size());
    for (int i_idx = 0; i_idx < static_cast<int>(free_nodes.size()); ++i_idx) {
        const int client = free_nodes[i_idx];
        for (int j_idx = 0; j_idx < static_cast<int>(free_nodes.size()); ++j_idx) {
            if (allowed_open_mask[j_idx] == 0) {
                continue;
            }

            const int candidate = free_nodes[j_idx];
            if (r1_filter_ != nullptr && !r1_filter_->keepsX(client, candidate)) {
                continue;
            }

            candidate_lists[i_idx].push_back(j_idx);
        }
    }

    return candidate_lists;
}

bool PartialOptimizer::useR2() const {
    return instance_.numNodes() > r2_min_nodes_ &&
           static_cast<double>(instance_.numNodes()) /
                   std::max(1, instance_.numMedians()) >
               r2_min_ratio_;
}

bool PartialOptimizer::solveSubproblem(Solution& solution,
                                       int ref_median,
                                       const std::vector<int>& free_medians,
                                       const std::vector<int>& free_nodes) const {
    if (free_medians.empty() || free_nodes.empty()) {
        return false;
    }

    const double objective_before = solution.cost();
    const int p_sub = static_cast<int>(free_medians.size());
    const int candidate_count = static_cast<int>(free_nodes.size());
    std::vector<char> allowed_open_mask(candidate_count, 1);
    if (useR2()) {
        allowed_open_mask = R2Filter::allowedOpenMaskForSubproblem(
            solution, free_nodes, free_medians, dm_, beta_r2_);
    }

    int allowed_open_count = 0;
    for (char allowed : allowed_open_mask) {
        allowed_open_count += (allowed != 0 ? 1 : 0);
    }
    if (allowed_open_count < p_sub) {
        std::cout << "[PARTIAL_OPT] skipped reason=r2_open_lt_psub"
                  << " ref_cluster=" << ref_median
                  << " free_clusters=" << free_medians.size()
                  << " free_nodes=" << free_nodes.size()
                  << " r2_candidates=" << allowed_open_count
                  << " p_sub=" << p_sub << "\n";
        return false;
    }

    const std::vector<std::vector<int>> candidate_lists =
        buildReducedCandidateLists(free_nodes, allowed_open_mask);
    for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
        if (candidate_lists[i_idx].empty()) {
            std::cout << "[PARTIAL_OPT] skipped reason=r1_empty_assignment"
                      << " ref_cluster=" << ref_median
                      << " client=" << free_nodes[i_idx]
                      << " free_nodes=" << free_nodes.size()
                      << " r2_candidates=" << allowed_open_count << "\n";
            return false;
        }
    }

    // Mapeamento reverso: para cada candidato j, os pares (i_idx, k_local)
    // de clientes que podem ser atribuidos a ele. Isso elimina o std::find
    // quadratico da restricao de capacidade.
    std::vector<std::vector<std::pair<int, int>>> clients_of_candidate(candidate_count);
    int vars_x = 0;
    for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
        const int lsize = static_cast<int>(candidate_lists[i_idx].size());
        vars_x += lsize;
        for (int k = 0; k < lsize; ++k) {
            clients_of_candidate[candidate_lists[i_idx][k]].push_back({i_idx, k});
        }
    }

    IloEnv env;
    IloAlgorithm::Status status = IloAlgorithm::Unknown;
    double objective_after = objective_before;
    bool improved = false;

    try {
        const auto t_start = std::chrono::steady_clock::now();

        IloModel model(env);

        IloBoolVarArray y(env, candidate_count);
        for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
            std::ostringstream name;
            name << "y_" << free_nodes[j_idx];
            y[j_idx].setName(name.str().c_str());
            if (allowed_open_mask[j_idx] == 0) {
                model.add(y[j_idx] == 0);
            }
        }

        // R2 restringe quais nos livres podem abrir mediana (y_j) e R1
        // restringe as atribuicoes x_ij por cliente da subregiao.
        IloArray<IloBoolVarArray> x(env, candidate_count);
        for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
            const int lsize = static_cast<int>(candidate_lists[i_idx].size());
            x[i_idx] = IloBoolVarArray(env, lsize);
            for (int k = 0; k < lsize; ++k) {
                const int j_idx = candidate_lists[i_idx][k];
                std::ostringstream name;
                name << "x_" << free_nodes[i_idx] << "_" << free_nodes[j_idx];
                x[i_idx][k].setName(name.str().c_str());
            }
        }

        IloExpr objective(env);
        for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
            const int client = free_nodes[i_idx];
            const int lsize = static_cast<int>(candidate_lists[i_idx].size());
            for (int k = 0; k < lsize; ++k) {
                const int candidate = free_nodes[candidate_lists[i_idx][k]];
                objective += dm_.at(client, candidate) * x[i_idx][k];
            }
        }
        model.add(IloMinimize(env, objective));
        objective.end();

        // Cada cliente livre eh alocado exatamente uma vez.
        for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
            IloExpr assign_sum(env);
            const int lsize = static_cast<int>(candidate_lists[i_idx].size());
            for (int k = 0; k < lsize; ++k) {
                assign_sum += x[i_idx][k];
            }
            model.add(assign_sum == 1);
            assign_sum.end();
        }

        // Exatamente p_sub medianas abertas na subregiao fechada.
        IloExpr open_sum(env);
        for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
            open_sum += y[j_idx];
        }
        model.add(open_sum == p_sub);
        open_sum.end();

        // Ligacao entre atribuicao e abertura da mediana candidata.
        for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
            const int lsize = static_cast<int>(candidate_lists[i_idx].size());
            for (int k = 0; k < lsize; ++k) {
                const int j_idx = candidate_lists[i_idx][k];
                model.add(x[i_idx][k] <= y[j_idx]);
            }
        }

        // Capacidade via mapa reverso: percorre apenas pares validos.
        for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
            IloExpr load_expr(env);
            const int candidate = free_nodes[j_idx];
            for (const auto& [i_idx, k] : clients_of_candidate[j_idx]) {
                load_expr += instance_.demand(free_nodes[i_idx]) * x[i_idx][k];
            }
            model.add(load_expr <= instance_.capacity(candidate) * y[j_idx]);
            load_expr.end();
        }

        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());
        cplex.setWarning(env.getNullStream());
        cplex.setParam(IloCplex::Param::TimeLimit, time_limit_s_);

        // Warm start apenas quando a solucao corrente cabe no modelo reduzido.
        {
            std::vector<int> global_to_local(instance_.numNodes(), -1);
            for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
                global_to_local[free_nodes[i_idx]] = i_idx;
            }
            std::vector<bool> is_currently_free_median(instance_.numNodes(), false);
            for (int m : free_medians) {
                is_currently_free_median[m] = true;
            }

            const std::vector<int>& current_assignments = solution.assignments();
            bool warm_start_representable = true;
            for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
                const int client = free_nodes[i_idx];
                const int current_med_global = current_assignments[client];
                const int current_med_local = global_to_local[current_med_global];
                if (current_med_local < 0 ||
                    allowed_open_mask[current_med_local] == 0 ||
                    !std::binary_search(candidate_lists[i_idx].begin(),
                                        candidate_lists[i_idx].end(),
                                        current_med_local)) {
                    warm_start_representable = false;
                    break;
                }
            }

            for (int median : free_medians) {
                const int local_idx = global_to_local[median];
                if (local_idx < 0 || allowed_open_mask[local_idx] == 0) {
                    warm_start_representable = false;
                    break;
                }
            }

            if (warm_start_representable) {
                IloNumVarArray start_vars(env);
                IloNumArray start_vals(env);

                for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                    start_vars.add(y[j_idx]);
                    start_vals.add(is_currently_free_median[free_nodes[j_idx]] ? 1.0
                                                                               : 0.0);
                }

                for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
                    const int client = free_nodes[i_idx];
                    const int current_med_global = current_assignments[client];
                    const int current_med_local = global_to_local[current_med_global];
                    const int lsize = static_cast<int>(candidate_lists[i_idx].size());
                    for (int k = 0; k < lsize; ++k) {
                        start_vars.add(x[i_idx][k]);
                        const double v =
                            (candidate_lists[i_idx][k] == current_med_local) ? 1.0
                                                                             : 0.0;
                        start_vals.add(v);
                    }
                }

                cplex.addMIPStart(start_vars, start_vals,
                                  IloCplex::MIPStartCheckFeas);
                start_vals.end();
                start_vars.end();
            }
        }

        const bool solved = cplex.solve();
        status = cplex.getStatus();

        if (solved && (status == IloAlgorithm::Optimal || status == IloAlgorithm::Feasible)) {
            Solution candidate_solution = solution;
            std::vector<int> new_medians;
            new_medians.reserve(p_sub);
            for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                if (cplex.getValue(y[j_idx]) > 0.5) {
                    new_medians.push_back(free_nodes[j_idx]);
                }
            }

            std::vector<int> merged_medians;
            merged_medians.reserve(solution.medians().size() - free_medians.size() + new_medians.size());
            std::vector<bool> is_free_median(instance_.numNodes(), false);
            for (int median : free_medians) {
                is_free_median[median] = true;
            }
            for (int median : solution.medians()) {
                if (!is_free_median[median]) {
                    merged_medians.push_back(median);
                }
            }
            merged_medians.insert(merged_medians.end(), new_medians.begin(), new_medians.end());

            std::vector<int> merged_assignments = solution.assignments();
            for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
                const int lsize = static_cast<int>(candidate_lists[i_idx].size());
                for (int k = 0; k < lsize; ++k) {
                    if (cplex.getValue(x[i_idx][k]) > 0.5) {
                        const int j_idx = candidate_lists[i_idx][k];
                        merged_assignments[free_nodes[i_idx]] = free_nodes[j_idx];
                        break;
                    }
                }
            }

            candidate_solution.setMedians(merged_medians);
            candidate_solution.setAssignments(merged_assignments);

            std::string error;
            if (evaluator_.evaluate(candidate_solution, &error)) {
                objective_after = candidate_solution.cost();
                if (candidate_solution.cost() < solution.cost() - kAcceptImprovementEps) {
                    solution = candidate_solution;
                    improved = true;
                }
            } else if (!error.empty()) {
                std::cout << "[PARTIAL_OPT] integration_error=" << error << "\n";
            }
        }

        const auto t_end = std::chrono::steady_clock::now();
        const double elapsed_s =
            std::chrono::duration<double>(t_end - t_start).count();

        std::cout << std::fixed << std::setprecision(4)
                  << "[PARTIAL_OPT] ref_cluster=" << ref_median
                  << " | free_clusters=" << free_medians.size()
                  << " | free_nodes=" << free_nodes.size()
                  << " | candidate_medians=" << allowed_open_count
                  << " | vars_x=" << vars_x
                  << " | fo_before=" << objective_before
                  << " | fo_after=" << objective_after
                  << " | delta=" << (objective_after - objective_before)
                  << " | time=" << elapsed_s
                  << "s | status=" << cplexStatusToString(status)
                  << "\n";
    } catch (const IloException& ex) {
        std::cout << "[PARTIAL_OPT] cplex_exception=" << ex.getMessage() << "\n";
    } catch (const std::exception& ex) {
        std::cout << "[PARTIAL_OPT] exception=" << ex.what() << "\n";
    }

    env.end();
    return improved;
}

bool PartialOptimizer::optimize(Solution& solution) const {
    return run(solution).improved;
}

PartialOptimizerStats PartialOptimizer::run(Solution& solution) const {
    PartialOptimizerStats stats;
    const auto t_phase_start = std::chrono::steady_clock::now();
    const int p = static_cast<int>(solution.medians().size());
    const int n = instance_.numNodes();

    // Adaptacao por p: para instancias de p grande aumentamos orcamento e
    // numero de chamadas porque o PO tem mais margem para achar melhorias
    // estruturais, e o custo relativo de uma chamada extra eh menor.
    const int effective_max_calls =
        (p >= 50 ? std::max(max_calls_, 5) : max_calls_);
    const double effective_time_budget =
        (p >= 50 ? std::max(total_time_budget_s_, 30.0) : total_time_budget_s_);

    while (stats.calls < effective_max_calls) {
        const auto t_now = std::chrono::steady_clock::now();
        stats.total_time_s =
            std::chrono::duration<double>(t_now - t_phase_start).count();
        if (stats.total_time_s >= effective_time_budget) {
            break;
        }

        const int ref_median = selectReferenceCluster(solution);
        if (ref_median < 0) {
            break;
        }

        // Adaptacao automatica da janela de vizinhanca por p:
        //  - p pequeno: evita que a subregiao colapse para o problema inteiro
        //    (visto em lin318_005 / ali535_005 / rl1304_010).
        //  - p grande: garante cobertura >= 20% de p para diversificar a
        //    vizinhanca do PO (combate estagnacao em u724_075).
        const int eff_min =
            std::min(min_clusters_, std::max(2, p / 3));
        const int eff_max =
            std::min(std::max(eff_min, p - 1),
                     std::max(max_clusters_free_, p / 5));

        const std::vector<int> free_medians =
            selectNeighborhoodClusters(solution, ref_median, eff_min, eff_max);
        const std::vector<int> free_nodes = collectFreeNodes(solution, free_medians);

        ++stats.calls;

        // Detecta degeneracao: se a subregiao cobre todas as medianas ou quase
        // todos os nos, o PO se torna o problema inteiro e nao deve rodar.
        const bool covers_all_medians = static_cast<int>(free_medians.size()) >= p;
        const bool covers_too_many_nodes =
            static_cast<int>(free_nodes.size()) > static_cast<int>(0.8 * n);
        if (covers_all_medians || covers_too_many_nodes) {
            ++stats.skipped_calls;
            ++stats.calls_without_improvement;
            std::cout << "[PARTIAL_OPT] skipped reason="
                      << (covers_all_medians ? "psub_ge_p" : "free_nodes_gt_0.8n")
                      << " p=" << p
                      << " free_clusters=" << free_medians.size()
                      << " free_nodes=" << free_nodes.size()
                      << " n=" << n << "\n";
            if (stats.calls_without_improvement >= max_no_improve_) {
                break;
            }
            continue;
        }

        const double objective_before = solution.cost();
        const bool improved =
            solveSubproblem(solution, ref_median, free_medians, free_nodes);
        const double gain = objective_before - solution.cost();

        if (improved) {
            ++stats.improving_calls;
            stats.improved = true;
            stats.total_gain += gain;
            stats.best_gain = std::max(stats.best_gain, gain);
            stats.calls_without_improvement = 0;
        } else {
            ++stats.calls_without_improvement;
        }

        if (stats.calls_without_improvement >= max_no_improve_) {
            break;
        }
    }

    const auto t_phase_end = std::chrono::steady_clock::now();
    stats.total_time_s =
        std::chrono::duration<double>(t_phase_end - t_phase_start).count();

    std::cout << std::fixed << std::setprecision(4)
              << "[PARTIAL_OPT_SUMMARY] calls=" << stats.calls
              << " | improving_calls=" << stats.improving_calls
              << " | skipped_calls=" << stats.skipped_calls
              << " | no_improve_streak=" << stats.calls_without_improvement
              << " | total_gain=" << stats.total_gain
              << " | best_gain=" << stats.best_gain
              << " | total_time=" << stats.total_time_s << "s\n";

    return stats;
}
