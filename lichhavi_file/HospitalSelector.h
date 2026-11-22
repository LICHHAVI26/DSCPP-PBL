
#ifndef HOSPITAL_SELECTOR_H
#define HOSPITAL_SELECTOR_H

#include "Models.h"
#include "RouteFinder.h"
#include <vector>

struct HospitalCandidate {
    Hospital hospital;
    int distance;
};

class HospitalSelector {
public:
    HospitalCandidate findNearestSuitableHospital(
        const std::vector<Hospital>& hospitals,
        const std::vector<int>& locations,
        int patientLocation,
        RouteFinder& routes
    );
};

#endif