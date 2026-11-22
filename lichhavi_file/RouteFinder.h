
#ifndef ROUTE_FINDER_H
#define ROUTE_FINDER_H

#include <vector>
#include <utility>

class RouteFinder {
private:
    int vertices;
    std::vector<std::vector<std::pair<int, int>>> graph;

public:
    RouteFinder(int vertices);

    void addRoad(int from, int to, int distance);

    int shortestDistance(int start, int destination);
};

#endif