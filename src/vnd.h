#ifndef VND_H
#define VND_H

#include "candidate_lists.h"
#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

class VND {
private:
    const Instance& instance_;
    const DistanceMatrix& distance_matrix_;
    const CandidateLists* r1_filter_;
    int iterations_m1_;
    int iterations_m2_;
    int iterations_m3_;
    int iterations_m4_;

public:
    VND(const Instance& instance,
        const DistanceMatrix& distance_matrix,
        const CandidateLists* r1_filter = nullptr);

    void run(Solution& solution);

    int iterationsM1() const;
    int iterationsM2() const;
    int iterationsM3() const;
    int iterationsM4() const;
};

#endif
