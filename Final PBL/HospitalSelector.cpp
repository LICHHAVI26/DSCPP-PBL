
#include "HospitalSelector.h"
#include <algorithm>
#include <stdexcept>

HospitalCandidate HospitalSelector::findNearestSuitableHospital(
    const std::vector<Hospital>& hospitals,
    const std::vector<int>& locations,
    int patientLocation,
    RouteFinder& routes
) {
    if (hospitals.size() != locations.size()) {
        throw std::invalid_argument(
            "Each hospital must have a location."
        );
    }

    std::vector<HospitalCandidate> candidates;

    for (std::size_t i = 0; i < hospitals.size(); i++) {
        if (hospitals[i].availableBeds <= 0 ||
            !hospitals[i].suitableFacilities) {
            continue;
        }

        int distance = routes.shortestDistance(
            locations[i], patientLocation
        );

        if (distance >= 0) {
            candidates.push_back({hospitals[i], distance});
        }
    }

    if (candidates.empty()) {
        throw std::runtime_error(
            "No suitable hospital is currently available."
        );
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const HospitalCandidate& a,
           const HospitalCandidate& b) {
            return a.distance < b.distance;
        }
    );

    return candidates.front();
}
