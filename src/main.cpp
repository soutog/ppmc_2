#include "distance_matrix.h"
#include "evaluator.h"
#include "instance.h"
#include "solution.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <arquivo_instancia>\n";
        return 1;
    }

    const std::string instance_path = argv[1];

    Instance instance;
    if (!instance.read(instance_path)) {
        return 1;
    }

    DistanceMatrix distance_matrix(instance);
    Evaluator evaluator(instance, distance_matrix);

    instance.printSummary();

    Solution solution(instance.numNodes());
    
    // create a simple solution by opening the first p medians and assigning clients to them in order
    std::vector<int> medians;
    medians.reserve(instance.numMedians());
    for (int i = 0; i < instance.numMedians(); ++i) {
        medians.push_back(i);
    }
    
    solution.setMedians(medians);

    std::vector<int> assignments(instance.numNodes(), -1);
    std::vector<double> residual_capacity(instance.numNodes(), 0.0);

    for (int median : medians) {
        residual_capacity[median] = instance.capacity(median);
    }

    for (int client = 0; client < instance.numNodes(); ++client) {
        for (int median : medians) {
            if (residual_capacity[median] >= instance.demand(client)) {
                assignments[client] = median;
                residual_capacity[median] -= instance.demand(client);
                break;
            }
        }
    }

    solution.setAssignments(assignments);

    // print vm and va for debugging
    // std::cout << "Medianas abertas (vm): ";
    // for (int m : solution.medians()) {
    //     std::cout << m << " ";
    // }
    // std::cout << "\nAtribuicoes (va): ";
    // for (int a : solution.assignments()) {
    //     std::cout << a << " ";
    // }
    // std::cout << "\n";

    std::string error;
    const bool valid = evaluator.evaluate(solution, &error);

    std::cout << "\nTeste minimo com solucao manual usando as primeiras p medianas\n";
    solution.printSummary(instance);
    std::cout << "Distancia pre-computada d(0,0): " << distance_matrix.at(0, 0) << "\n";
    if (instance.numNodes() > 1) {
        std::cout << "Distancia pre-computada d(0,1): " << distance_matrix.at(0, 1) << "\n";
    }

    if (!valid) {
        std::cout << "Erro de validacao: " << error << "\n";
        return 2;
    }

    return 0;
}
