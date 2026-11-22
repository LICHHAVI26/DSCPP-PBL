
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
    string phone;
    int age;
    string gender;
    string location;
    string emergencyDetails;
    Severity severity;
};

struct Driver {
    int id = 0;
    string name;
    string licenseNumber;
    bool licenseVerified = false;
    double rating = 0.0;

    // Driver account details
    string phone;
    string passwordHash;

    // Driver and ambulance connection
    int ambulanceId = -1;
    int currentLocation = 0;

    // Driver account and availability
    bool approved = false;
    bool rejected = false;
    bool isAvailable = false;
};

struct Ambulance {
    int id;
    int driverId;
    string registrationNumber;
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
