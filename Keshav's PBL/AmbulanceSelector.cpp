
#include "AmbulanceSelector.h"
#include <algorithm>
#include <stdexcept>

std::vector<AmbulanceCandidate>
AmbulanceSelector::findNearestFour(
    const std::vector<Ambulance>& ambulances,
    const std::vector<int>& locations,
    int patientLocation,
    RouteFinder& routes
) {
    if (ambulances.size() != locations.size()) {
        throw std::invalid_argument(
            "Each ambulance must have a location."
        );
    }

    std::vector<AmbulanceCandidate> candidates;

    for (std::size_t i = 0; i < ambulances.size(); i++) {
        if (ambulances[i].status != AmbulanceStatus::Available) {
            continue;
        }

        int distance = routes.shortestDistance(
            locations[i], patientLocation
        );

        if (distance >= 0) {
            candidates.push_back({ambulances[i], distance});
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const AmbulanceCandidate& a,
           const AmbulanceCandidate& b) {
            return a.distance < b.distance;
        }
    );

    if (candidates.size() > 4) {
        candidates.resize(4);
    }

    return candidates;
}