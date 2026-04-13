#include "clustering_search.h"
#include "distance_matrix.h"
#include "evaluator.h"
#include "grasp_constructor.h"
#include "ils.h"
#include "instance.h"
#include "partial_optimizer.h"
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
                     " [num_iter_max] [time_limit_s]\n";
        return 1;
    }

    const std::string instance_path = argv[1];
    const unsigned int seed =
        (argc >= 3 ? static_cast<unsigned int>(std::strtoul(argv[2], nullptr, 10))
                   : 42u);
    const double alpha = (argc >= 4 ? std::strtod(argv[3], nullptr) : 0.6);
    const int construction_max_tries =
        (argc >= 5 ? std::atoi(argv[4]) : 1000);
    const int num_iter_max =
        (argc >= 6 ? std::atoi(argv[5]) : 60);
    const double cli_time_limit_s =
        (argc >= 7 ? std::strtod(argv[6], nullptr) : -1.0);

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

    // Orcamento de tempo por default depende do tamanho da instancia;
    // pode ser sobrescrito pela CLI. num_iter_max continua valendo como
    // segundo criterio (o que primeiro atingir encerra o ILS).
    double time_limit_s;
    if (cli_time_limit_s >= 0.0) {
        time_limit_s = cli_time_limit_s;
    } else {
        const int n = instance.numNodes();
        if (n <= 200) time_limit_s = 30.0;
        else if (n <= 500) time_limit_s = 60.0;
        else if (n <= 1000) time_limit_s = 120.0;
        else time_limit_s = 300.0;
    }

    instance.printSummary();
    std::cout << "\nParametros\n";
    std::cout << "seed=" << seed
              << ", alpha=" << alpha
              << ", construction_max_tries=" << construction_max_tries
              << ", NumIterMax=" << num_iter_max
              << ", time_limit_s=" << time_limit_s
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
    // === Partial Optimizer ===
    PartialOptimizer partial_optimizer(instance, distance_matrix, evaluator);
    ClusteringSearch clustering_search(instance,
                                       distance_matrix,
                                       evaluator,
                                       &partial_optimizer,
                                       &grasp);
    ILS ils(instance, distance_matrix, num_iter_max, time_limit_s);
    const auto t_ils_start = std::chrono::steady_clock::now();
    Solution best = ils.run(solution, rng, &clustering_search);
    const auto t_ils_end = std::chrono::steady_clock::now();
    const double ils_secs = std::chrono::duration<double>(t_ils_end - t_ils_start).count();

    const double final_cost = best.cost();
    std::cout << "\nCusto FINAL: " << final_cost << "\n";
    std::cout << "Melhoria sobre VND: " << (vnd_cost - final_cost) << " ("
              << std::setprecision(2)
              << ((vnd_cost - final_cost) / vnd_cost * 100.0) << "%)\n";
    std::cout << std::setprecision(4);
    std::cout << "Iteracoes ILS: " << ils.totalIterations()
              << ", melhorias: " << ils.improvements() << "\n";
    const ClusteringSearchStats& cs_stats = clustering_search.stats();
    std::cout << "CS observacoes: " << cs_stats.observations
              << ", clusters ativos: " << cs_stats.active_clusters
              << ", novos clusters: " << cs_stats.new_clusters
              << ", updates de centro: " << cs_stats.center_updates << "\n";
    std::cout << "CS gatilhos PO: " << cs_stats.po_triggers
              << ", melhorias PO: " << cs_stats.po_improvements
              << ", ganho total PO: " << cs_stats.po_total_gain << "\n";
    std::cout << "CS destroy/repair: " << cs_stats.destroy_repair_calls
              << ", com melhoria: " << cs_stats.destroy_repair_improvements
              << ", promocoes ILS: " << cs_stats.ils_current_promotions << "\n";
    std::cout << "Tempo ILS: " << ils_secs << "s\n";

    const auto t_total_end = std::chrono::steady_clock::now();
    const double total_secs = std::chrono::duration<double>(t_total_end - t_total_start).count();
    std::cout << "Tempo total: " << total_secs << "s\n";

    // === Validacao final ===
    std::string error;
    if (!evaluator.validate(best, &error)) {
        std::cout << "Erro de validacao final: " << error << "\n";
        return 3;
    }
    std::cout << "Validacao final: OK\n";

    return 0;
}
