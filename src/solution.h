#ifndef SOLUTION_H
#define SOLUTION_H

#include "instance.h"

#include <string>
#include <vector>

class Solution {
private:
    std::vector<int> vm_;          // indices das medianas abertas
    std::vector<int> va_;          // va[j] = mediana que atende o cliente j
    std::vector<double> load_;     // carga acumulada por vertice/mediana
    double cost_;
    bool feasible_;

public:
    Solution();
    explicit Solution(int n);

    void reset(int n);
    void clear();

    void setMedians(const std::vector<int>& medians);
    void setAssignments(const std::vector<int>& assignments);
    void setEvaluationState(const std::vector<double>& load,
                            double cost,
                            bool feasible);

    const std::vector<int>& medians() const;
    const std::vector<int>& assignments() const;
    const std::vector<double>& load() const;

    double cost() const;
    bool feasible() const;

    bool isMedian(int node) const;
    void printSummary(const Instance& instance) const;
};

#endif
