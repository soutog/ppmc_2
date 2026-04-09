#include "evaluator.h"

#include <algorithm>
#include <sstream>

Evaluator::Evaluator(const Instance& instance,
                     const DistanceMatrix& distance_matrix)
    : instance_(instance), distance_matrix_(distance_matrix) {}

double Evaluator::computeTotalCost(const Solution& solution) const {
    const std::vector<int>& assignments = solution.assignments();
    double total_cost = 0.0;

    for (int client = 0; client < instance_.numNodes(); ++client) {
        const int median = assignments[client];
        total_cost += distance_matrix_.at(client, median);
    }

    return total_cost;
}

std::vector<double> Evaluator::computeLoads(const Solution& solution) const {
    std::vector<double> loads(instance_.numNodes(), 0.0);
    const std::vector<int>& assignments = solution.assignments();

    for (int client = 0; client < instance_.numNodes(); ++client) {
        const int median = assignments[client];
        if (median >= 0 && median < instance_.numNodes()) {
            loads[median] += instance_.demand(client);
        }
    }

    return loads;
}

bool Evaluator::validate(const Solution& solution, std::string* error) const {
    const int n = instance_.numNodes();
    const int p = instance_.numMedians();
    const std::vector<int>& medians = solution.medians();
    const std::vector<int>& assignments = solution.assignments();

    if (distance_matrix_.size() != n) {
        if (error != nullptr) {
            *error = "Matriz de distancias inconsistente com a instancia.";
        }
        return false;
    }

    if (static_cast<int>(medians.size()) != p) {
        if (error != nullptr) {
            *error = "Numero de medianas diferente de p.";
        }
        return false;
    }

    if (static_cast<int>(assignments.size()) != n) {
        if (error != nullptr) {
            *error = "Vetor de alocacoes com tamanho incorreto.";
        }
        return false;
    }

    {
        std::vector<int> sorted = medians;
        std::sort(sorted.begin(), sorted.end());
        const auto unique_end = std::unique(sorted.begin(), sorted.end());
        if (unique_end != sorted.end()) {
            if (error != nullptr) {
                *error = "Ha medianas repetidas na solucao.";
            }
            return false;
        }
    }

    for (int median : medians) {
        if (median < 0 || median >= n) {
            if (error != nullptr) {
                *error = "Indice de mediana fora do intervalo da instancia.";
            }
            return false;
        }
    }

    std::vector<double> loads(n, 0.0);

    for (int client = 0; client < n; ++client) {
        const int median = assignments[client];

        if (median < 0 || median >= n) {
            if (error != nullptr) {
                *error = "Cliente sem alocacao valida.";
            }
            return false;
        }

        if (!solution.isMedian(median)) {
            if (error != nullptr) {
                *error = "Cliente alocado a um vertice que nao e mediana aberta.";
            }
            return false;
        }

        loads[median] += instance_.demand(client);
    }

    for (int median : medians) {
        if (loads[median] > instance_.capacity(median)) {
            if (error != nullptr) {
                std::ostringstream oss;
                oss << "Capacidade violada na mediana " << median
                    << ": carga=" << loads[median]
                    << ", capacidade=" << instance_.capacity(median) << ".";
                *error = oss.str();
            }
            return false;
        }
    }

    if (error != nullptr) {
        error->clear();
    }

    return true;
}

bool Evaluator::evaluate(Solution& solution, std::string* error) const {
    const bool is_valid = validate(solution, error);

    if (!is_valid) {
        solution.setEvaluationState(std::vector<double>(instance_.numNodes(), 0.0),
                                    0.0,
                                    false);
        return false;
    }

    const std::vector<double> loads = computeLoads(solution);
    const double total_cost = computeTotalCost(solution);
    solution.setEvaluationState(loads, total_cost, true);

    if (error != nullptr) {
        error->clear();
    }

    return true;
}
