#include "vnd.h"
#include "neighborhood_cache.h"
#include "neighborhoods.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {
constexpr double kImprovementEps = 1e-9;
}  // namespace

VND::VND(const Instance& instance,
         const DistanceMatrix& distance_matrix,
         const NeighborhoodCache& nh_cache)
    : instance_(instance), distance_matrix_(distance_matrix),
      nh_cache_(nh_cache),
      iterations_m1_(0), iterations_m2_(0),
      iterations_m3_(0), iterations_m4_(0) {}

void VND::run(Solution& solution) {
    iterations_m1_ = 0;
    iterations_m2_ = 0;
    iterations_m3_ = 0;
    iterations_m4_ = 0;

    const bool trace = std::getenv("VND_TRACE") != nullptr;
    double t_m1 = 0.0, t_m2 = 0.0, t_m3 = 0.0, t_m4 = 0.0;
    int n_m1 = 0, n_m2 = 0, n_m3 = 0, n_m4 = 0;

    int k = 0;  // 0=M1, 1=M2, 2=M3, 3=M4

    while (k < 4) {
        switch (k) {
        case 0: {
            const auto t0 = std::chrono::steady_clock::now();
            const MoveM1 move = bestM1(solution, instance_, distance_matrix_);
            t_m1 += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            ++n_m1;
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
            const auto t0 = std::chrono::steady_clock::now();
            const MoveM2 move = bestM2(solution, instance_, distance_matrix_);
            t_m2 += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            ++n_m2;
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
            const auto t0 = std::chrono::steady_clock::now();
            const MoveM3 move = bestM3(solution, instance_, distance_matrix_, nh_cache_);
            t_m3 += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            ++n_m3;
            if (move.found) {
                applyMove(solution, move, instance_, distance_matrix_);
                if (move.delta < -kImprovementEps) {
                    ++iterations_m3_;
                    k = 0;
                } else {
                    k = 3;
                }
            } else {
                k = 3;
            }
            break;
        }
        case 3: {
            const auto t0 = std::chrono::steady_clock::now();
            const MoveM4 move = bestM4(solution, instance_, distance_matrix_);
            t_m4 += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            ++n_m4;
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

    if (trace) {
        std::cerr << "[VND] M1 scans=" << n_m1 << " t=" << t_m1
                  << "s | M2 scans=" << n_m2 << " t=" << t_m2
                  << "s | M3 scans=" << n_m3 << " t=" << t_m3
                  << "s | M4 scans=" << n_m4 << " t=" << t_m4 << "s\n" << std::flush;
    }
}

int VND::iterationsM1() const { return iterations_m1_; }
int VND::iterationsM2() const { return iterations_m2_; }
int VND::iterationsM3() const { return iterations_m3_; }
int VND::iterationsM4() const { return iterations_m4_; }
