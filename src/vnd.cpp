#include "vnd.h"
#include "neighborhoods.h"

namespace {
constexpr double kImprovementEps = 1e-9;
}  // namespace

VND::VND(const Instance& instance, const DistanceMatrix& distance_matrix)
    : instance_(instance), distance_matrix_(distance_matrix),
      iterations_m1_(0), iterations_m2_(0),
      iterations_m3_(0), iterations_m4_(0) {}

void VND::run(Solution& solution) {
    iterations_m1_ = 0;
    iterations_m2_ = 0;
    iterations_m3_ = 0;
    iterations_m4_ = 0;

    int k = 0;  // 0=M1, 1=M2, 2=M3, 3=M4

    while (k < 4) {
        switch (k) {
        case 0: {
            const MoveM1 move = bestM1(solution, instance_, distance_matrix_);
            if (move.found) {
                applyMove(solution, move, instance_);
                ++iterations_m1_;
                k = 0;
            } else {
                k = 1;
            }
            break;
        }
        case 1: {
            const MoveM2 move = bestM2(solution, instance_, distance_matrix_);
            if (move.found) {
                applyMove(solution, move, instance_, distance_matrix_);
                ++iterations_m2_;
                k = 0;
            } else {
                k = 2;
            }
            break;
        }
        case 2: {
            const MoveM3 move = bestM3(solution, instance_, distance_matrix_);
            if (move.found) {
                // bestM3 avalia sem capacidade; aplicar com capacidade
                // pode dar resultado diferente. Backup para restaurar se nao melhorar.
                const double cost_before = solution.cost();
                Solution backup = solution;
                applyMove(solution, move, instance_, distance_matrix_);
                if (solution.cost() < cost_before - kImprovementEps) {
                    ++iterations_m3_;
                    k = 0;
                } else {
                    solution = backup;
                    k = 3;
                }
            } else {
                k = 3;
            }
            break;
        }
        case 3: {
            const MoveM4 move = bestM4(solution, instance_, distance_matrix_);
            if (move.found) {
                applyMove(solution, move, instance_);
                ++iterations_m4_;
                k = 0;
            } else {
                k = 4;
            }
            break;
        }
        }
    }
}

int VND::iterationsM1() const { return iterations_m1_; }
int VND::iterationsM2() const { return iterations_m2_; }
int VND::iterationsM3() const { return iterations_m3_; }
int VND::iterationsM4() const { return iterations_m4_; }
