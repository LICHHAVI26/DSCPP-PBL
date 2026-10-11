
#include <iostream>
#include <vector>
#include "DispatchManager.h"

using namespace std;

int main() {
    vector<AmbulanceCandidate> candidates = {
        {{1, 201, "Location A", AmbulanceStatus::Available}, 4},
        {{2, 202, "Location B", AmbulanceStatus::Available}, 5},
        {{3, 203, "Location C", AmbulanceStatus::Available}, 8},
        {{4, 204, "Location D", AmbulanceStatus::Available}, 10}
    };


vector<AmbulanceResponse> responses = {
    {1, AcceptanceStatus::Rejected},
    {2, AcceptanceStatus::TimedOut},
    {3, AcceptanceStatus::Rejected},
    {4, AcceptanceStatus::Accepted}
};

    DispatchManager manager;

    try {
        AmbulanceCandidate selected =
            manager.selectAmbulance(candidates, responses, 1);

        cout << "Ambulance Assigned Successfully\n";
        cout << "Ambulance ID: "
             << selected.ambulance.id << '\n';
        cout << "Distance: "
             << selected.distance << " km\n";
    } catch (const exception& e) {
        cout << e.what() << '\n';
    }

    return 0;
}