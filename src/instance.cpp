#include "instance.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

Instance::Instance() : n_(0), p_(0) {}

bool Instance::read(const std::string& filepath) {
    std::ifstream fin(filepath);

    if (!fin.is_open()) {
        std::cerr << "Erro: nao foi possivel abrir a instancia: "
                  << filepath << "\n";
        return false;
    }

    fin >> n_ >> p_;

    if (!fin || n_ <= 0 || p_ <= 0 || p_ > n_) {
        std::cerr << "Erro: cabecalho da instancia invalido.\n";
        return false;
    }

    nodes_.clear();
    nodes_.resize(n_);

    for (int i = 0; i < n_; ++i) {
        fin >> nodes_[i].x
            >> nodes_[i].y
            >> nodes_[i].capacity
            >> nodes_[i].demand;

        if (!fin) {
            std::cerr << "Erro: falha ao ler a linha do ponto " << i << ".\n";
            return false;
        }
    }

    return true;
}

int Instance::numNodes() const {
    return n_;
}

int Instance::numMedians() const {
    return p_;
}

double Instance::x(int i) const {
    return nodes_[i].x;
}

double Instance::y(int i) const {
    return nodes_[i].y;
}

double Instance::capacity(int i) const {
    return nodes_[i].capacity;
}

double Instance::demand(int i) const {
    return nodes_[i].demand;
}

const Node& Instance::node(int i) const {
    return nodes_[i];
}

const std::vector<Node>& Instance::nodes() const {
    return nodes_;
}

double Instance::distance(int i, int j) const {
    const double dx = nodes_[i].x - nodes_[j].x;
    const double dy = nodes_[i].y - nodes_[j].y;
    return std::sqrt(dx * dx + dy * dy);
}

void Instance::printSummary() const {
    std::cout << "Instancia carregada com sucesso.\n";
    std::cout << "Numero de pontos (n): " << n_ << "\n";
    std::cout << "Numero de medianas (p): " << p_ << "\n";

    if (!nodes_.empty()) {
        std::cout << "\nPrimeiros pontos da instancia:\n";
        std::cout << std::fixed << std::setprecision(2);

        const int limit = (n_ < 5 ? n_ : 5);
        for (int i = 0; i < limit; ++i) {
            std::cout << "Ponto " << i
                      << " -> x=" << nodes_[i].x
                      << ", y=" << nodes_[i].y
                      << ", cap=" << nodes_[i].capacity
                      << ", dem=" << nodes_[i].demand
                      << "\n";
        }
    }
}