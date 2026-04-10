#ifndef DISTANCE_MATRIX_H
#define DISTANCE_MATRIX_H

#include "instance.h"

#include <vector>

class DistanceMatrix {
private:
    int n_;
    std::vector<double> distances_;

public:
    DistanceMatrix();
    explicit DistanceMatrix(const Instance& instance);

    void build(const Instance& instance);

    int size() const;
    double at(int i, int j) const;
    double operator()(int i, int j) const { return distances_[i * n_ + j]; }
};

#endif
