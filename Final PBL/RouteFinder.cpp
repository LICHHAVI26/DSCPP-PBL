
#include "RouteFinder.h"
#include <queue>
#include <climits>
#include <functional>
#include <stdexcept>

RouteFinder::RouteFinder(int vertices) {
    if (vertices <= 0) {
        throw std::invalid_argument("Invalid number of locations.");
    }

    this->vertices = vertices;
    graph.resize(vertices);
}

void RouteFinder::addRoad(int from, int to, int distance) {
    if (from < 0 || to < 0 ||
        from >= vertices || to >= vertices ||
        distance < 0) {
        throw std::invalid_argument("Invalid road details.");
    }

    graph[from].push_back({to, distance});
    graph[to].push_back({from, distance});
}

int RouteFinder::shortestDistance(int start, int destination) {
    if (start < 0 || destination < 0 ||
        start >= vertices || destination >= vertices) {
        throw std::invalid_argument("Invalid location.");
    }

    std::vector<int> distances(vertices, INT_MAX);

    using Node = std::pair<int, int>;

    std::priority_queue<
        Node,
        std::vector<Node>,
        std::greater<Node>
    > pq;

    distances[start] = 0;
    pq.push({0, start});

    while (!pq.empty()) {
        int currentDistance = pq.top().first;
        int currentNode = pq.top().second;
        pq.pop();

        if (currentDistance != distances[currentNode]) {
            continue;
        }

        if (currentNode == destination) {
            return currentDistance;
        }

        for (const auto& road : graph[currentNode]) {
            int nextNode = road.first;
            int roadDistance = road.second;

            if (currentDistance <= INT_MAX - roadDistance &&
                currentDistance + roadDistance < distances[nextNode]) {
                distances[nextNode] = currentDistance + roadDistance;
                pq.push({distances[nextNode], nextNode});
            }
        }
    }

    return -1;
}
