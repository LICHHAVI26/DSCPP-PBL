
#include "DispatchManager.h"
#include <stdexcept>

AmbulanceCandidate DispatchManager::selectAmbulance(
    const std::vector<AmbulanceCandidate>& candidates,
    const std::vector<AmbulanceResponse>& responses,
    int distanceTolerance
) {
    if (candidates.empty()) {
        throw std::runtime_error("No available ambulances found.");
    }

    if (distanceTolerance < 0) {
        throw std::invalid_argument(
            "Distance tolerance cannot be negative."
        );
    }

    for (const auto& candidate : candidates) {
        const AmbulanceResponse* candidateResponse = nullptr;

        for (const auto& response : responses) {
            if (response.ambulanceId == candidate.ambulance.id) {
                candidateResponse = &response;
                break;
            }
        }

        if (candidateResponse == nullptr ||
            candidateResponse->status != AcceptanceStatus::Accepted) {
            continue;
        }

        int nearestDistance = candidates.front().distance;

        if (candidate.distance - nearestDistance <= distanceTolerance) {
            return candidate;
        }

        bool closerStillPending = false;
        bool closerAccepted = false;

        for (const auto& other : candidates) {
            if (other.distance >= candidate.distance) {
                continue;
            }

            for (const auto& response : responses) {
                if (response.ambulanceId != other.ambulance.id) {
                    continue;
                }

                if (response.status == AcceptanceStatus::Pending) {
                    closerStillPending = true;
                } else if (response.status == AcceptanceStatus::Accepted) {
                    closerAccepted = true;
                }

                break;
            }

            if (closerStillPending || closerAccepted) {
                break;
            }
        }

        if (!closerStillPending && !closerAccepted) {
            return candidate;
        }
    }

    throw std::runtime_error(
        "No ambulance can be assigned yet."
    );
}

bool DispatchManager::assignAmbulance(
    std::vector<Ambulance>& ambulances,
    int ambulanceId
) {
    for (auto& ambulance : ambulances) {
        if (ambulance.id == ambulanceId) {
            if (ambulance.status != AmbulanceStatus::Available) {
                return false;
            }

            ambulance.status = AmbulanceStatus::Assigned;
            return true;
        }
    }

    return false;
}