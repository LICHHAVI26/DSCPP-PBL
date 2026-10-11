
#ifndef AMBULANCE_SELECTOR_H
#define AMBULANCE_SELECTOR_H

#include "Models.h"
#include "RouteFinder.h"
#include <vector>

struct AmbulanceCandidate {
    Ambulance ambulance;
    int distance;
};

class AmbulanceSelector {
public:
    std::vector<AmbulanceCandidate> findNearestFour(
        const std::vector<Ambulance>& ambulances,
        const std::vector<int>& locations,
        int patientLocation,
        RouteFinder& routes
    );
};

#endif