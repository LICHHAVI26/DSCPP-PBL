
#ifndef MODELS_H
#define MODELS_H

#include <string>
using namespace std;

enum class Severity {
    Low = 1,
    Medium = 2,
    High = 3,
    Critical = 4
};

enum class AmbulanceStatus {
    Available,
    Pending,
    Assigned,
    OnTrip,
    Unavailable
};

enum class ResponseStatus {
    Pending,
    Accepted,
    Rejected,
    TimedOut
};

struct Patient {
    int id;
    string name;
    string location;
    string emergencyDetails;
    Severity severity;
};

struct Driver {
    int id;
    string name;
    string licenseNumber;
    bool licenseVerified;
    double rating;
};

struct Ambulance {
    int id;
    int driverId;
    string currentLocation;
    AmbulanceStatus status;
};

struct Hospital {
    int id;
    string name;
    string location;
    int availableBeds;
    bool suitableFacilities;
};

struct EmergencyRequest {
    int id;
    int patientId;
    Severity severity;
    string patientLocation;
    ResponseStatus status;
};

#endif