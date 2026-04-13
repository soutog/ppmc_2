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
#include <unordered_map>

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
                                   int omega,
                                   int min_clusters,
                                   int max_clusters_free,
                                   double time_limit_s,
                                   double alpha_r1,
                                   int top_t)
    : instance_(instance),
      dm_(dm),
      evaluator_(evaluator),
      omega_(omega),
      min_clusters_(min_clusters),
      max_clusters_free_(max_clusters_free),
      time_limit_s_(time_limit_s),
      alpha_r1_(alpha_r1),
      top_t_(top_t) {}

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

    int best_median = -1;
    double best_score = -std::numeric_limits<double>::infinity();

    // Score hibrido do plano: 0.7 * custo_total + 0.3 * custo_medio.
    for (int median : solution.medians()) {
        if (cluster_size[median] == 0) {
            continue;
        }
        const double total = cluster_cost[median];
        const double mean = total / static_cast<double>(cluster_size[median]);
        const double score = 0.7 * total + 0.3 * mean;

        if (score > best_score + 1e-9 ||
            (std::abs(score - best_score) <= 1e-9 &&
             (best_median < 0 || median < best_median))) {
            best_score = score;
            best_median = median;
        }
    }

    return best_median;
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

std::vector<std::vector<int>> PartialOptimizer::buildCandidateLists(
    const Solution& solution, const std::vector<int>& free_nodes) const {
    std::unordered_map<int, int> node_to_local;
    node_to_local.reserve(free_nodes.size());
    for (int idx = 0; idx < static_cast<int>(free_nodes.size()); ++idx) {
        node_to_local[free_nodes[idx]] = idx;
    }

    std::vector<std::vector<int>> candidate_lists(free_nodes.size());
    for (int i_idx = 0; i_idx < static_cast<int>(free_nodes.size()); ++i_idx) {
        const int client = free_nodes[i_idx];
        std::vector<std::pair<double, int>> ordered;
        ordered.reserve(free_nodes.size());

        for (int j_idx = 0; j_idx < static_cast<int>(free_nodes.size()); ++j_idx) {
            ordered.push_back({dm_.at(client, free_nodes[j_idx]), j_idx});
        }

        std::sort(ordered.begin(), ordered.end(),
                  [](const std::pair<double, int>& lhs,
                     const std::pair<double, int>& rhs) {
                      if (lhs.first != rhs.first) {
                          return lhs.first < rhs.first;
                      }
                      return lhs.second < rhs.second;
                  });

        // R1 combinado: top-t simplificado intersectado com o filtro de
        // capacidade acumulada inspirado no Stefanello. Top-t garante tamanho
        // maximo do modelo; alpha_r1 garante que candidatos saturados saiam.
        const int effective_top =
            std::min(top_t_, static_cast<int>(ordered.size()));
        double accumulated_demand = instance_.demand(client);
        candidate_lists[i_idx].reserve(effective_top);
        int taken = 0;
        for (const auto& [distance, j_idx] : ordered) {
            (void)distance;
            if (taken >= effective_top) {
                break;
            }
            const int candidate = free_nodes[j_idx];
            if (accumulated_demand <= alpha_r1_ * instance_.capacity(candidate) + 1e-9) {
                candidate_lists[i_idx].push_back(j_idx);
                ++taken;
            }
            accumulated_demand += instance_.demand(candidate);
        }

        // Preserva a mediana atual do cliente se ela estiver na subregiao fechada.
        const int current_median = solution.assignments()[client];
        auto current_it = node_to_local.find(current_median);
        if (current_it != node_to_local.end() &&
            std::find(candidate_lists[i_idx].begin(),
                      candidate_lists[i_idx].end(),
                      current_it->second) == candidate_lists[i_idx].end()) {
            candidate_lists[i_idx].push_back(current_it->second);
        }

        // Garante pelo menos um candidato mesmo se o filtro por capacidade for agressivo.
        if (candidate_lists[i_idx].empty() && !ordered.empty()) {
            candidate_lists[i_idx].push_back(ordered.front().second);
        }
    }

    return candidate_lists;
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
    const std::vector<std::vector<int>> candidate_lists =
        buildCandidateLists(solution, free_nodes);

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
        }

        // Modelo esparso: x[i_idx] tem apenas |candidate_lists[i_idx]|
        // variaveis, indexadas por k local. O j_idx global esta em
        // candidate_lists[i_idx][k]. Essa e a reducao efetiva de R1.
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

        // Warm start: a solucao atual eh viavel para o subproblema por
        // construcao (buildCandidateLists preserva a mediana atual na lista).
        // Passar ela como MIP start tipicamente acelera a prova de otimalidade.
        {
            std::vector<int> global_to_local(instance_.numNodes(), -1);
            for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
                global_to_local[free_nodes[i_idx]] = i_idx;
            }
            std::vector<bool> is_currently_free_median(instance_.numNodes(), false);
            for (int m : free_medians) {
                is_currently_free_median[m] = true;
            }

            IloNumVarArray start_vars(env);
            IloNumArray start_vals(env);

            for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                start_vars.add(y[j_idx]);
                start_vals.add(is_currently_free_median[free_nodes[j_idx]] ? 1.0 : 0.0);
            }

            const std::vector<int>& current_assignments = solution.assignments();
            for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
                const int client = free_nodes[i_idx];
                const int current_med_global = current_assignments[client];
                const int current_med_local = global_to_local[current_med_global];
                const int lsize = static_cast<int>(candidate_lists[i_idx].size());
                for (int k = 0; k < lsize; ++k) {
                    start_vars.add(x[i_idx][k]);
                    const double v =
                        (candidate_lists[i_idx][k] == current_med_local) ? 1.0 : 0.0;
                    start_vals.add(v);
                }
            }

            cplex.addMIPStart(start_vars, start_vals,
                              IloCplex::MIPStartCheckFeas);
            start_vals.end();
            start_vars.end();
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
                  << " | candidate_medians=" << candidate_count
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
    const int p = static_cast<int>(solution.medians().size());
    const int n = instance_.numNodes();

    const int ref_median = selectReferenceCluster(solution);
    if (ref_median < 0) {
        return false;
    }

    // Adaptacao automatica para instancias de p pequeno: evita que a subregiao
    // colapse para o problema inteiro (visto em lin318_005 / ali535_005 / rl1304_010).
    const int eff_min =
        std::min(min_clusters_, std::max(2, p / 3));
    const int eff_max =
        std::min(max_clusters_free_, std::max(eff_min, p - 1));

    const std::vector<int> free_medians =
        selectNeighborhoodClusters(solution, ref_median, eff_min, eff_max);
    const std::vector<int> free_nodes = collectFreeNodes(solution, free_medians);

    // Detecta degeneracao: se a subregiao cobre todas as medianas ou quase
    // todos os nos, o PO se torna o problema inteiro e nao deve rodar.
    const bool covers_all_medians = static_cast<int>(free_medians.size()) >= p;
    const bool covers_too_many_nodes =
        static_cast<int>(free_nodes.size()) > static_cast<int>(0.8 * n);
    if (covers_all_medians || covers_too_many_nodes) {
        std::cout << "[PARTIAL_OPT] skipped reason="
                  << (covers_all_medians ? "psub_ge_p" : "free_nodes_gt_0.8n")
                  << " p=" << p
                  << " free_clusters=" << free_medians.size()
                  << " free_nodes=" << free_nodes.size()
                  << " n=" << n << "\n";
        return false;
    }

    return solveSubproblem(solution, ref_median, free_medians, free_nodes);
}
