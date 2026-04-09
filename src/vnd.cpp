#include "vnd.h"
#include "neighborhoods.h"

VND::VND(const Instance& instance, const DistanceMatrix& distance_matrix)
    : instance_(instance), distance_matrix_(distance_matrix),
      iterations_m1_(0), iterations_m4_(0) {}

void VND::run(Solution& solution) {
    iterations_m1_ = 0;
    iterations_m4_ = 0;

    int k = 0;  // 0 = M1, 1 = M4

    while (k < 2) {
        if (k == 0) {
            const MoveM1 move = bestM1(solution, instance_, distance_matrix_);
            if (move.found) {
                applyMove(solution, move, instance_);
                ++iterations_m1_;
                k = 0;  // reinicia em M1
            } else {
                k = 1;  // avanca para M4
            }
        } else {
            const MoveM4 move = bestM4(solution, instance_, distance_matrix_);
            if (move.found) {
                applyMove(solution, move, instance_);
                ++iterations_m4_;
                k = 0;  // reinicia em M1
            } else {
                k = 2;  // encerra
            }
        }
    }
}

int VND::iterationsM1() const { return iterations_m1_; }
int VND::iterationsM4() const { return iterations_m4_; }
