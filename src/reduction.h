#ifndef REDUCTION_H
#define REDUCTION_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <vector>

class R1Filter {
private:
    std::vector<std::vector<int>> allowed_;
    std::vector<std::vector<char>> allowed_mask_;
    std::vector<std::vector<char>> capacity_ok_mask_;
    double alpha_;

public:
    R1Filter(const Instance& instance,
             const DistanceMatrix& dm,
             double alpha = 2.0);

    const std::vector<int>& allowed(int i) const;
    bool inKi(int i, int j) const;
    bool trivialCapacityOk(int i, int j) const;
    bool keepsX(int i, int j) const;
    double alpha() const;
};

class R2Filter {
public:
    static std::vector<char> allowedOpenMaskForSubproblem(
        const Solution& solution,
        const std::vector<int>& free_nodes,
        const std::vector<int>& free_medians,
        const DistanceMatrix& dm,
        int beta);
};

#endif
