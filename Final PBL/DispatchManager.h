
#ifndef DISPATCH_MANAGER_H
#define DISPATCH_MANAGER_H

#include "AmbulanceSelector.h"
#include <vector>

enum class AcceptanceStatus {
    Pending,
    Accepted,
    Rejected,
    TimedOut
};

struct AmbulanceResponse {
    int ambulanceId;
    AcceptanceStatus status;
};

class DispatchManager {
public:
    AmbulanceCandidate selectAmbulance(
        const std::vector<AmbulanceCandidate>& candidates,
        const std::vector<AmbulanceResponse>& responses,
        int distanceTolerance
    );

    bool assignAmbulance(
        std::vector<Ambulance>& ambulances,
        int ambulanceId
    );

};

#endif