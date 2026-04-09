#include "distance_matrix.h"
#include "evaluator.h"
#include "grasp_constructor.h"
#include "instance.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0]
                  << " <arquivo_instancia> [seed] [alpha] [construction_max_tries]\n";
        return 1;
    }

    const std::string instance_path = argv[1];
    const unsigned int seed =
        (argc >= 3 ? static_cast<unsigned int>(std::strtoul(argv[2], nullptr, 10))
                   : 123456u);
    const double alpha = (argc >= 4 ? std::strtod(argv[3], nullptr) : 0.6);
    const int construction_max_tries =
        (argc >= 5 ? std::atoi(argv[4]) : 1000);

    Instance instance;
    if (!instance.read(instance_path)) {
        return 1;
    }

    DistanceMatrix distance_matrix(instance);
    Evaluator evaluator(instance, distance_matrix);
    GRASPConstructor grasp(instance,
                           distance_matrix,
                           evaluator,
                           alpha,
                           construction_max_tries,
                           seed);

    instance.printSummary();
    std::cout << "\nParametros do GRASP\n";
    std::cout << "seed=" << seed
              << ", alpha=" << alpha
              << ", construction_max_tries=" << construction_max_tries
              << "\n";

    const Solution solution = grasp.construct();

    std::cout << "\nResumo da LRC\n";
    std::cout << "Candidatos ranqueados: " << grasp.rankedCandidates().size() << "\n";
    std::cout << "Tamanho da LRC: " << grasp.restrictedCandidateList().size() << "\n";
    std::cout << "Tentativas usadas: " << grasp.lastAttempts() << "\n"; // tentativas usadas na construcao para encontrar a solucao viável

    std::cout << "\nTeste da construcao inicial GRASP\n";
    solution.printSummary(instance);   

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Distancia pre-computada d(0,0): " << distance_matrix.at(0, 0) << "\n";
    if (instance.numNodes() > 1) {
        std::cout << "Distancia pre-computada d(0,1): " << distance_matrix.at(0, 1) << "\n";
    }

    if (!solution.feasible()) {
        std::cout << "Erro de construcao: " << grasp.lastError() << "\n";
        return 2;
    }

    std::string error;
    if (!evaluator.validate(solution, &error)) {
        std::cout << "Erro de validacao final: " << error << "\n";
        return 3;
    }

    return 0;
}
