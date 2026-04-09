#include "solution.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

namespace {

bool containsNode(const std::vector<int>& nodes, int value) {
    return std::find(nodes.begin(), nodes.end(), value) != nodes.end();
}

}  // namespace

Solution::Solution() : cost_(0.0), feasible_(false) {}

Solution::Solution(int n) : vm_(), va_(n, -1), load_(n, 0.0), cost_(0.0), feasible_(false) {}

void Solution::reset(int n) {
    vm_.clear();
    va_.assign(n, -1);
    load_.assign(n, 0.0);
    cost_ = 0.0;
    feasible_ = false;
}

void Solution::clear() {
    vm_.clear();
    va_.clear();
    load_.clear();
    cost_ = 0.0;
    feasible_ = false;
}

void Solution::setMedians(const std::vector<int>& medians) {
    vm_ = medians;
}

void Solution::setAssignments(const std::vector<int>& assignments) {
    va_ = assignments;
}

void Solution::setEvaluationState(const std::vector<double>& load,
                                  double cost,
                                  bool feasible) {
    load_ = load;
    cost_ = cost;
    feasible_ = feasible;
}

const std::vector<int>& Solution::medians() const {
    return vm_;
}

const std::vector<int>& Solution::assignments() const {
    return va_;
}

const std::vector<double>& Solution::load() const {
    return load_;
}

double Solution::cost() const {
    return cost_;
}

bool Solution::feasible() const {
    return feasible_;
}

bool Solution::isMedian(int node) const {
    return containsNode(vm_, node);
}

void Solution::printSummary(const Instance& instance) const {
    std::cout << "Resumo da solucao\n";
    std::cout << "Medianas abertas: " << vm_.size()
              << " / p=" << instance.numMedians() << "\n";
    std::cout << "Viavel: " << (feasible_ ? "sim" : "nao") << "\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Custo total: " << cost_ << "\n";

    if (!vm_.empty()) {
        std::cout << "Lista de medianas: ";
        for (std::size_t i = 0; i < vm_.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << vm_[i];
        }
        // std::cout << "\n";

        // std::cout << "Carga por mediana:\n";
        // for (int median : vm_) {
        //     std::cout << "  mediana " << median
        //               << " -> carga=" << load_[median]
        //               << " / capacidade=" << instance.capacity(median)
        //               << "\n";
        // }
    }
}

void Solution::applyReallocation(int j, int old_median, int new_median,
                                 double delta, double demand_j) {
    va_[j] = new_median;
    load_[old_median] -= demand_j;
    load_[new_median] += demand_j;
    cost_ += delta;
}

void Solution::applySwap(int j1, int j2, double delta,
                         double demand_j1, double demand_j2) {
    const int r1 = va_[j1];
    const int r2 = va_[j2];
    va_[j1] = r2;
    va_[j2] = r1;
    load_[r1] += demand_j2 - demand_j1;
    load_[r2] += demand_j1 - demand_j2;
    cost_ += delta;
}

void Solution::replaceMedian(int old_med, int new_med) {
    for (auto& m : vm_) {
        if (m == old_med) {
            m = new_med;
            return;
        }
    }
}
