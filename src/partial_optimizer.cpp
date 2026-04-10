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

std::vector<int> collectClusterClients(const Solution& solution, int median) {
    std::vector<int> clients;
    const std::vector<int>& assignments = solution.assignments();
    for (int client = 0; client < static_cast<int>(assignments.size()); ++client) {
        if (assignments[client] == median) {
            clients.push_back(client);
        }
    }
    return clients;
}

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
                                   double time_limit_s)
    : instance_(instance),
      dm_(dm),
      evaluator_(evaluator),
      omega_(omega),
      min_clusters_(min_clusters),
      time_limit_s_(time_limit_s) {}

int PartialOptimizer::selectReferenceCluster(const Solution& solution) const {
    int best_median = -1;
    double best_score = -1.0;

    for (int median : solution.medians()) {
        const std::vector<int> clients = collectClusterClients(solution, median);
        if (clients.empty()) {
            continue;
        }

        double cluster_cost = 0.0;
        for (int client : clients) {
            cluster_cost += dm_.at(client, median);
        }
        const double mean_cost = cluster_cost / static_cast<double>(clients.size());

        if (mean_cost > best_score ||
            (std::abs(mean_cost - best_score) <= 1e-9 && median < best_median)) {
            best_score = mean_cost;
            best_median = median;
        }
    }

    return best_median;
}

std::vector<int> PartialOptimizer::selectNeighborhoodClusters(const Solution& solution,
                                                              int ref_median) const {
    const std::vector<int>& medians = solution.medians();
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

    for (const auto& [distance, median] : ordered) {
        (void)distance;
        selected.push_back(median);
        accumulated_nodes += static_cast<int>(collectClusterClients(solution, median).size());

        if (static_cast<int>(selected.size()) >= min_clusters_ &&
            accumulated_nodes >= omega_) {
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
    const int vars_x = static_cast<int>(free_nodes.size() * free_nodes.size());

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

        IloArray<IloBoolVarArray> x(env, candidate_count);
        for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
            x[i_idx] = IloBoolVarArray(env, candidate_count);
            for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                std::ostringstream name;
                name << "x_" << free_nodes[i_idx] << "_" << free_nodes[j_idx];
                x[i_idx][j_idx].setName(name.str().c_str());
            }
        }

        IloExpr objective(env);
        for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
            const int client = free_nodes[i_idx];
            for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                const int candidate = free_nodes[j_idx];
                objective += dm_.at(client, candidate) * x[i_idx][j_idx];
            }
        }
        model.add(IloMinimize(env, objective));
        objective.end();

        // Cada cliente livre eh alocado exatamente uma vez.
        for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
            IloExpr assign_sum(env);
            for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                assign_sum += x[i_idx][j_idx];
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
            for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                model.add(x[i_idx][j_idx] <= y[j_idx]);
            }
        }

        // Na Etapa A a subregiao eh fechada, entao a capacidade pode usar Q[j] diretamente.
        for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
            IloExpr load_expr(env);
            const int candidate = free_nodes[j_idx];
            for (int i_idx = 0; i_idx < candidate_count; ++i_idx) {
                load_expr += instance_.demand(free_nodes[i_idx]) * x[i_idx][j_idx];
            }
            model.add(load_expr <= instance_.capacity(candidate) * y[j_idx]);
            load_expr.end();
        }

        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());
        cplex.setWarning(env.getNullStream());
        cplex.setParam(IloCplex::Param::TimeLimit, time_limit_s_);
        cplex.setParam(IloCplex::Param::Threads, 1);
        cplex.setParam(IloCplex::Param::MIP::Limits::TreeMemory, 2000);

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
                for (int j_idx = 0; j_idx < candidate_count; ++j_idx) {
                    if (cplex.getValue(x[i_idx][j_idx]) > 0.5) {
                        merged_assignments[free_nodes[i_idx]] = free_nodes[j_idx];
                        break;
                    }
                }
            }

            candidate_solution.setMedians(merged_medians);
            candidate_solution.setAssignments(merged_assignments);

            std::string error;
            if (evaluator_.evaluate(candidate_solution, &error) &&
                candidate_solution.cost() < solution.cost() - 1e-9) {
                objective_after = candidate_solution.cost();
                solution = candidate_solution;
                improved = true;
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
    const int ref_median = selectReferenceCluster(solution);
    if (ref_median < 0) {
        return false;
    }

    const std::vector<int> free_medians = selectNeighborhoodClusters(solution, ref_median);
    const std::vector<int> free_nodes = collectFreeNodes(solution, free_medians);
    return solveSubproblem(solution, ref_median, free_medians, free_nodes);
}
