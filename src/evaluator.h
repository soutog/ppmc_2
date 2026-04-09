#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "distance_matrix.h"
#include "instance.h"
#include "solution.h"

#include <string>
#include <vector>

class Evaluator {
private:
    const Instance& instance_;
    const DistanceMatrix& distance_matrix_;

public:
    Evaluator(const Instance& instance, const DistanceMatrix& distance_matrix);

    double computeTotalCost(const Solution& solution) const;
    std::vector<double> computeLoads(const Solution& solution) const;

    bool validate(const Solution& solution, std::string* error = nullptr) const;
    bool evaluate(Solution& solution, std::string* error = nullptr) const;
};

#endif
