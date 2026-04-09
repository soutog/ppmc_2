#include "distance_matrix.h"
#include "evaluator.h"
#include "grasp_constructor.h"
#include "ils.h"
#include "instance.h"
#include "vnd.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0]
                  << " <arquivo_instancia> [seed] [alpha] [construction_max_tries]"
                     " [num_iter_max]\n";
        return 1;
    }

    const std::string instance_path = argv[1];
    const unsigned int seed =
        (argc >= 3 ? static_cast<unsigned int>(std::strtoul(argv[2], nullptr, 10))
                   : 123456u);
    const double alpha = (argc >= 4 ? std::strtod(argv[3], nullptr) : 0.6);
    const int construction_max_tries =
        (argc >= 5 ? std::atoi(argv[4]) : 1000);
    const int num_iter_max =
        (argc >= 6 ? std::atoi(argv[5]) : 20);

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
    std::cout << "\nParametros\n";
    std::cout << "seed=" << seed
              << ", alpha=" << alpha
              << ", construction_max_tries=" << construction_max_tries
              << ", NumIterMax=" << num_iter_max
              << "\n";

    const auto t_total_start = std::chrono::steady_clock::now();

    // === GRASP ===
    Solution solution = grasp.construct();

    if (!solution.feasible()) {
        std::cout << "Erro de construcao: " << grasp.lastError() << "\n";
        return 2;
    }

    std::cout << std::fixed << std::setprecision(4);
    const double grasp_cost = solution.cost();
    std::cout << "\nCusto GRASP: " << grasp_cost << "\n";

    // === VND inicial ===
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
    std::cout << "Tempo VND inicial: " << vnd_secs << "s\n";

    // === ILS ===
    std::mt19937 rng(seed);
    ILS ils(instance, distance_matrix, num_iter_max);
    const auto t_ils_start = std::chrono::steady_clock::now();
    Solution best = ils.run(solution, rng);
    const auto t_ils_end = std::chrono::steady_clock::now();
    const double ils_secs = std::chrono::duration<double>(t_ils_end - t_ils_start).count();

    const double ils_cost = best.cost();
    std::cout << "\nCusto ILS:   " << ils_cost << "\n";
    std::cout << "Melhoria sobre VND: " << (vnd_cost - ils_cost) << " ("
              << std::setprecision(2)
              << ((vnd_cost - ils_cost) / vnd_cost * 100.0) << "%)\n";
    std::cout << std::setprecision(4);
    std::cout << "Iteracoes ILS: " << ils.totalIterations()
              << ", melhorias: " << ils.improvements() << "\n";
    std::cout << "Tempo ILS: " << ils_secs << "s\n";

    const auto t_total_end = std::chrono::steady_clock::now();
    const double total_secs = std::chrono::duration<double>(t_total_end - t_total_start).count();
    std::cout << "Tempo total: " << total_secs << "s\n";

    // === Validacao final ===
    std::string error;
    if (!evaluator.validate(best, &error)) {
        std::cout << "Erro de validacao pos-ILS: " << error << "\n";
        return 3;
    }
    std::cout << "Validacao pos-ILS: OK\n";

    return 0;
}
