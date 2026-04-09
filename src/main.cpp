#include "distance_matrix.h"
#include "evaluator.h"
#include "grasp_constructor.h"
#include "instance.h"
#include "vnd.h"

#include <chrono>
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

    Solution solution = grasp.construct();

    std::cout << "\nResumo da LRC\n";
    std::cout << "Candidatos ranqueados: " << grasp.rankedCandidates().size() << "\n";
    std::cout << "Tamanho da LRC: " << grasp.restrictedCandidateList().size() << "\n";
    std::cout << "Tentativas usadas: " << grasp.lastAttempts() << "\n";

    if (!solution.feasible()) {
        std::cout << "Erro de construcao: " << grasp.lastError() << "\n";
        return 2;
    }

    std::cout << std::fixed << std::setprecision(4);
    const double grasp_cost = solution.cost();
    std::cout << "\nCusto GRASP: " << grasp_cost << "\n";

    // Busca local VND (M1 -> M2 -> M3 -> M4)
    VND vnd(instance, distance_matrix);
    const auto t_vnd_start = std::chrono::steady_clock::now();
    vnd.run(solution);
    const auto t_vnd_end = std::chrono::steady_clock::now();
    const double vnd_secs = std::chrono::duration<double>(t_vnd_end - t_vnd_start).count();

    const double vnd_cost = solution.cost();
    std::cout << "Custo VND:   " << vnd_cost << "\n";
    std::cout << "Melhoria:    " << (grasp_cost - vnd_cost) << " ("
              << std::setprecision(2)
              << ((grasp_cost - vnd_cost) / grasp_cost * 100.0) << "%)\n";
    std::cout << std::setprecision(4);
    std::cout << "Iteracoes M1: " << vnd.iterationsM1()
              << ", M2: " << vnd.iterationsM2()
              << ", M3: " << vnd.iterationsM3()
              << ", M4: " << vnd.iterationsM4() << "\n";
    // solucao final é viável
    std::cout << "Solucao final: " << (solution.feasible() ? "viavel" : "inviavel") << "\n";
    std::cout << "Tempo VND: " << vnd_secs << "s\n";

    // Validacao final
    std::string error;
    if (!evaluator.validate(solution, &error)) {
        std::cout << "Erro de validacao pos-VND: " << error << "\n";
        return 3;
    }
    std::cout << "Validacao pos-VND: OK\n";

    return 0;
}
