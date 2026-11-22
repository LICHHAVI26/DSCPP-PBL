
#include <iostream>
#include <vector>
#include <exception>
#include "Models.h"
#include "EmergencyQueue.h"
#include "RouteFinder.h"
#include "AmbulanceSelector.h"
#include "DispatchManager.h"
#include "HospitalSelector.h"

using namespace std;

int main() {
    cout << "====================================\n";
    cout << " AMBULANCE & EMERGENCY DISPATCH SYSTEM\n";
    cout << "====================================\n\n";

    Patient patient = {
        101, "Test Patient", "9876543210", 30, "Male",
        "Location 0", "Medical Emergency", Severity::Critical
    };

    cout << "Patient: " << patient.name << '\n';
    cout << "Emergency: " << patient.emergencyDetails << "\n\n";

    EmergencyQueue emergencyQueue;

    EmergencyRequest request = {
        1, patient.id, patient.severity,
        patient.location, ResponseStatus::Pending
    };

    emergencyQueue.addEmergency(request);
    EmergencyRequest nextEmergency =
        emergencyQueue.getNextEmergency();

    cout << "Step 1: Emergency Prioritization\n";
    cout << "Priority Level: "
         << static_cast<int>(nextEmergency.severity) << '\n';
    cout << "Critical emergency processed first.\n\n";

    RouteFinder routes(9);

    routes.addRoad(0, 1, 4);
    routes.addRoad(0, 2, 5);
    routes.addRoad(0, 3, 8);
    routes.addRoad(0, 4, 10);
    routes.addRoad(0, 5, 2);
    routes.addRoad(0, 6, 3);
    routes.addRoad(0, 7, 5);
    routes.addRoad(0, 8, 7);

    cout << "Step 2: Route Calculation\n";
    cout << "Sample road network created using Dijkstra.\n\n";

    
vector<Ambulance> ambulances = {
    {1, 201, "DEMO-AMB-001", "Location 1", AmbulanceStatus::Available},
    {2, 202, "DEMO-AMB-002", "Location 2", AmbulanceStatus::Available},
    {3, 203, "DEMO-AMB-003", "Location 3", AmbulanceStatus::Available},
    {4, 204, "DEMO-AMB-004", "Location 4", AmbulanceStatus::Available},
    {5, 205, "DEMO-AMB-005", "Location 5", AmbulanceStatus::OnTrip}
};

    vector<int> ambulanceLocations = {1, 2, 3, 4, 5};

    AmbulanceSelector ambulanceSelector;

    vector<AmbulanceCandidate> nearest =
        ambulanceSelector.findNearestFour(
            ambulances, ambulanceLocations, 0, routes
        );

    cout << "Step 3: Four Nearest Available Ambulances\n";

    for (const auto& candidate : nearest) {
        cout << "Ambulance " << candidate.ambulance.id
             << " - " << candidate.distance << " km\n";
    }

    vector<AmbulanceResponse> responses = {
        {1, AcceptanceStatus::Pending},
        {2, AcceptanceStatus::Accepted},
        {3, AcceptanceStatus::Pending},
        {4, AcceptanceStatus::Accepted}
    };

    DispatchManager dispatchManager;

    cout << "\nStep 4: Ambulance Assignment\n";

    try {
        AmbulanceCandidate selected =
            dispatchManager.selectAmbulance(
                nearest, responses, 1
            );

        if (dispatchManager.assignAmbulance(
                ambulances, selected.ambulance.id)) {

            cout << "Ambulance Assigned Successfully\n";
            cout << "Assigned Ambulance ID: "
                 << selected.ambulance.id << '\n';
            cout << "Distance: " << selected.distance << " km\n";
            cout << "Status: Assigned\n";

            bool secondAssignment =
                dispatchManager.assignAmbulance(
                    ambulances, selected.ambulance.id
                );

            if (!secondAssignment) {
                cout << "Double assignment blocked successfully.\n";
            } else {
                cout << "Warning: Double assignment occurred.\n";
            }

        } else {
            cout << "Assignment failed: Ambulance is not available.\n";
        }

    } catch (const exception& e) {
        cout << e.what() << '\n';
    }

    cout << "\nStep 5: Hospital Selection\n";

    vector<Hospital> hospitals = {
        {1, "City Hospital", "Location 6", 5, true},
        {2, "General Hospital", "Location 7", 10, true},
        {3, "Small Clinic", "Location 8", 0, true}
    };

    vector<int> hospitalLocations = {6, 7, 8};

    HospitalSelector hospitalSelector;

    try {
        HospitalCandidate hospital =
            hospitalSelector.findNearestSuitableHospital(
                hospitals, hospitalLocations, 0, routes
            );

        cout << "Hospital: " << hospital.hospital.name << '\n';
        cout << "Distance: " << hospital.distance << " km\n";
        cout << "Available Beds: "
             << hospital.hospital.availableBeds << '\n';

    } catch (const exception& e) {
        cout << e.what() << '\n';
    }

    cout << "\n====================================\n";
    cout << " Emergency Dispatch Demo Completed\n";
    cout << "====================================\n";

    return 0;
}