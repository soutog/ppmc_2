#ifndef INSTANCE_H
#define INSTANCE_H

#include <string>
#include <vector>

struct Node {
    double x;
    double y;
    double capacity;
    double demand;
};

class Instance {
private:
    int n_;                 // numero de pontos
    int p_;                 // numero de medianas a abrir
    std::vector<Node> nodes_;

public:
    Instance();

    // Le a instancia do arquivo
    bool read(const std::string& filepath);

    // Getters basicos
    int numNodes() const;
    int numMedians() const;

    double x(int i) const;
    double y(int i) const;
    double capacity(int i) const;
    double demand(int i) const;

    const Node& node(int i) const;
    const std::vector<Node>& nodes() const;

    // Distancia euclidiana entre dois pontos
    double distance(int i, int j) const;

    // Impressao resumida
    void printSummary() const;
};

#endif