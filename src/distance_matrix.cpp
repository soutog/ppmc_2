#include "distance_matrix.h"

#include <stdexcept>

DistanceMatrix::DistanceMatrix() : n_(0) {}

DistanceMatrix::DistanceMatrix(const Instance& instance) : n_(0) {
    build(instance);
}

void DistanceMatrix::build(const Instance& instance) {
    n_ = instance.numNodes();
    distances_.assign(n_ * n_, 0.0);

    for (int i = 0; i < n_; ++i) {
        for (int j = i; j < n_; ++j) {
            const double distance = instance.distance(i, j);
            distances_[i * n_ + j] = distance;
            distances_[j * n_ + i] = distance;
        }
    }
}

int DistanceMatrix::size() const {
    return n_;
}

double DistanceMatrix::at(int i, int j) const {
    if (i < 0 || j < 0 || i >= n_ || j >= n_) {
        throw std::out_of_range("Indice fora do intervalo da matriz de distancias.");
    }

    return distances_[i * n_ + j];
}
