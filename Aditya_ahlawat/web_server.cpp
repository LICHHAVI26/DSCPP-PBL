// PulsePath v2.0 — C++ POSIX HTTP server for the Emergency Dispatch System.
// Serves static files from public/ and provides REST-like API endpoints.
// Features: ambulance fleet tracking, emergency management with timelines,
//           dispatch logging, and full DSA pipeline integration.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Auth.h"
#include "Models.h"
#include "EmergencyQueue.h"
#include "RouteFinder.h"
#include "AmbulanceSelector.h"
#include "DispatchManager.h"
#include "HospitalSelector.h"

using namespace std;

// ===========================================================================
// Constants
// ===========================================================================
static const string DISPATCH_LOG = "dispatch_log.csv";
static const string CSV_HEADER =
    "id,timestamp,patientName,phone,location,emergency,severity,"
    "ambulanceId,ambulanceDistance,hospitalName,hospitalDistance,status";

// ===========================================================================
// Emergency Tracking Structures
// ===========================================================================
struct EmergencyTimelineEvent {
    string timestamp;
    string status;
    string message;
};

struct ActiveEmergency {
    int id;
    string patientName;
    string phone;
    int location;
    string emergency;
    string severity;
    int ambulanceId;
    int ambulanceDistance;
    string hospitalName;
    int hospitalDistance;
    int hospitalLocationIdx;
    string currentStatus;  // Received, Dispatched, En-Route, Arrived, Completed
    vector<EmergencyTimelineEvent> timeline;
};

// ===========================================================================
// Live Tracking Structures
// ===========================================================================
struct TrackingSession {
    int emergencyId;
    int ambulanceId;
    string ambulanceReg;
    string patientName;
    string emergency;
    string severity;

    int startLocation;        // ambulance origin node
    int patientLocation;      // patient node
    int hospitalLocation;     // hospital node
    string hospitalName;

    int distanceToPatient;    // km (Dijkstra shortest path)
    int distanceToHospital;   // km (Dijkstra patient→hospital)

    string currentPhase;      // "to_patient", "at_patient", "to_hospital", "completed"
    time_t phaseStartTime;    // when current phase began
    time_t dispatchTime;      // when originally dispatched

    static constexpr double SPEED_KM_PER_SEC = 0.25; // 1 km per 4 seconds (demo speed)
    static constexpr double PICKUP_DURATION  = 8.0;   // seconds at patient location
};

// SVG node positions (matching the frontend map layout, 500x500 viewport)
static const double nodeX[] = {250, 250, 430, 250, 70, 380, 430, 70, 70};
static const double nodeY[] = {250, 60, 190, 440, 190, 80, 370, 80, 370};

// ===========================================================================
// Global In-Memory State
// ===========================================================================
static vector<Ambulance> globalFleet = {
    {1, 201, "DEMO-AMB-001", "Location 1", AmbulanceStatus::Available},
    {2, 202, "DEMO-AMB-002", "Location 2", AmbulanceStatus::Available},
    {3, 203, "DEMO-AMB-003", "Location 3", AmbulanceStatus::Available},
    {4, 204, "DEMO-AMB-004", "Location 4", AmbulanceStatus::Available},
    {5, 205, "DEMO-AMB-005", "Location 5", AmbulanceStatus::OnTrip}
};
static vector<int> globalFleetLocations = {1, 2, 3, 4, 5};

static vector<ActiveEmergency> activeEmergencies;
static int nextEmergencyId = 1;
static vector<TrackingSession> trackingSessions;

// Hospitals (global so fleet reset doesn't lose them)
static vector<Hospital> globalHospitals = {
    {1, "City Hospital",    "Location 6", 5,  true},
    {2, "General Hospital", "Location 7", 10, true},
    {3, "Small Clinic",     "Location 8", 0,  true}
};
static vector<int> globalHospitalLocations = {6, 7, 8};

// ===========================================================================
// Helper: build the 9-node Dijkstra road network
// ===========================================================================
static RouteFinder buildRouteNetwork() {
    RouteFinder routes(9);
    routes.addRoad(0, 1, 4);
    routes.addRoad(0, 2, 5);
    routes.addRoad(0, 3, 8);
    routes.addRoad(0, 4, 10);
    routes.addRoad(0, 5, 2);
    routes.addRoad(0, 6, 3);
    routes.addRoad(0, 7, 5);
    routes.addRoad(0, 8, 7);
    return routes;
}

// ===========================================================================
// Helper: human-readable location names
// ===========================================================================
static string locationName(int loc) {
    switch (loc) {
        case 0: return "Central HQ";
        case 1: return "North District";
        case 2: return "East District";
        case 3: return "South District";
        case 4: return "West District";
        case 5: return "Zone 5";
        case 6: return "Zone 6 (City Hospital)";
        case 7: return "Zone 7 (General Hospital)";
        case 8: return "Zone 8 (Small Clinic)";
        default: return "Unknown";
    }
}

// ===========================================================================
// Helper: AmbulanceStatus ↔ string conversion
// ===========================================================================
static string ambulanceStatusStr(AmbulanceStatus s) {
    switch (s) {
        case AmbulanceStatus::Available:   return "Available";
        case AmbulanceStatus::Pending:     return "Pending";
        case AmbulanceStatus::Assigned:    return "Assigned";
        case AmbulanceStatus::OnTrip:      return "OnTrip";
        case AmbulanceStatus::Unavailable: return "Unavailable";
        default: return "Unknown";
    }
}

static AmbulanceStatus parseAmbulanceStatus(const string& s) {
    if (s == "Available")   return AmbulanceStatus::Available;
    if (s == "Pending")     return AmbulanceStatus::Pending;
    if (s == "Assigned")    return AmbulanceStatus::Assigned;
    if (s == "OnTrip")      return AmbulanceStatus::OnTrip;
    return AmbulanceStatus::Unavailable;
}

// ===========================================================================
// Utility: read an entire file into a string
// ===========================================================================
static string readFile(const string& path) {
    ifstream file(path, ios::binary);
    if (!file) return "";
    ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// ===========================================================================
// JSON helpers (hand-written, no external library)
// ===========================================================================
static string jsonEscape(const string& input) {
    string out;
    for (unsigned char c : input) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) out += ' ';
                else out += static_cast<char>(c);
        }
    }
    return out;
}

static string jsonString(const string& body, const string& key) {
    const string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == string::npos) return "";
    p = body.find(':', p + needle.size());
    if (p == string::npos) return "";
    p = body.find('"', p + 1);
    if (p == string::npos) return "";
    ++p;
    string value;
    bool escaped = false;
    for (; p < body.size(); ++p) {
        char c = body[p];
        if (escaped) {
            if (c == 'n') value += '\n';
            else if (c == 'r') value += '\r';
            else if (c == 't') value += '\t';
            else value += c;
            escaped = false;
        } else if (c == '\\') escaped = true;
        else if (c == '"') break;
        else value += c;
    }
    return value;
}

static int jsonInt(const string& body, const string& key, int fallback) {
    const string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == string::npos) return fallback;
    p = body.find(':', p + needle.size());
    if (p == string::npos) return fallback;
    ++p;
    while (p < body.size() && isspace(static_cast<unsigned char>(body[p]))) ++p;
    try { return stoi(body.substr(p)); } catch (...) { return fallback; }
}

// ===========================================================================
// CSV helpers
// ===========================================================================
static string csvEscape(const string& value) {
    string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

static int nextRecordId(const string& path) {
    ifstream file(path);
    string line;
    int highest = 0;
    if (!file) return 1;
    getline(file, line); // skip header
    while (getline(file, line)) {
        size_t comma = line.find(',');
        if (comma == string::npos) continue;
        try {
            highest = max(highest, stoi(line.substr(0, comma)));
        } catch (...) {}
    }
    return highest + 1;
}

static void ensureCsvHeader(const string& path, const string& header) {
    ifstream input(path, ios::binary);
    if (input && input.peek() != ifstream::traits_type::eof()) return;
    ofstream output(path, ios::app);
    if (!output) throw runtime_error("Cannot create storage file: " + path);
    output << header << '\n';
}

static void appendCsvRow(const string& path, const string& row) {
    ofstream file(path, ios::app);
    if (!file) throw runtime_error("Cannot write storage file: " + path);
    file << row << '\n';
    if (!file) throw runtime_error("Failed to save record: " + path);
}

static string currentTimestamp() {
    time_t now = time(nullptr);
    tm local{};
    localtime_r(&now, &local);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    return buffer;
}

// ---------------------------------------------------------------------------
// Parse a single CSV field starting at position `pos`.
// Handles quoted fields (with escaped double-quotes).
// ---------------------------------------------------------------------------
static string parseCsvField(const string& line, size_t& pos) {
    string value;
    if (pos >= line.size()) return value;

    if (line[pos] == '"') {
        ++pos;
        while (pos < line.size()) {
            if (line[pos] == '"') {
                if (pos + 1 < line.size() && line[pos + 1] == '"') {
                    value += '"';
                    pos += 2;
                } else {
                    ++pos;
                    break;
                }
            } else {
                value += line[pos];
                ++pos;
            }
        }
        if (pos < line.size() && line[pos] == ',') ++pos;
    } else {
        size_t comma = line.find(',', pos);
        if (comma == string::npos) {
            value = line.substr(pos);
            pos = line.size();
        } else {
            value = line.substr(pos, comma - pos);
            pos = comma + 1;
        }
    }
    return value;
}

// ===========================================================================
// HTTP response builder (with CORS headers)
// ===========================================================================
static string httpResponse(int status, const string& body,
                           const string& type = "application/json; charset=utf-8") {
    string reason = status == 200 ? "OK"
                  : status == 400 ? "Bad Request"
                  : status == 401 ? "Unauthorized"
                  : "Not Found";
    ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
             << "Content-Type: " << type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "Cache-Control: no-store\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             << "Access-Control-Allow-Headers: Content-Type\r\n"
             << "\r\n" << body;
    return response.str();
}

// ===========================================================================
// API: POST /api/login
// ===========================================================================
static string handleLogin(const string& body) {
    string username = jsonString(body, "username");
    string password = jsonString(body, "password");

    if (username.empty() || password.empty()) {
        return httpResponse(400,
            "{\"ok\":false,\"error\":\"Please enter both username and password.\"}");
    }

    if (login(username, password)) {
        return httpResponse(200,
            "{\"ok\":true,\"username\":\"" + jsonEscape(username) + "\"}");
    } else {
        return httpResponse(401,
            "{\"ok\":false,\"error\":\"Invalid username or password.\"}");
    }
}

// ===========================================================================
// API: GET /api/stats
// ===========================================================================
static string handleStats() {
    ifstream file(DISPATCH_LOG);
    string line;
    int total = 0, successCount = 0;
    double totalDistance = 0.0;

    if (file) {
        getline(file, line); // skip header
        while (getline(file, line)) {
            if (line.empty()) continue;
            ++total;

            size_t pos = 0;
            string fields[12];
            for (int i = 0; i < 12 && pos <= line.size(); ++i) {
                fields[i] = parseCsvField(line, pos);
            }

            string status = fields[11];
            if (status == "Assigned" || status == "Success") {
                ++successCount;
            }

            try {
                double dist = stod(fields[8]);
                if (dist >= 0) totalDistance += dist;
            } catch (...) {}
        }
    }

    double successRate = total > 0 ? (100.0 * successCount / total) : 0.0;
    double avgDistance  = successCount > 0 ? (totalDistance / successCount) : 0.0;

    // Count active emergencies and available ambulances
    int activeCount = 0;
    for (const auto& e : activeEmergencies) {
        if (e.currentStatus != "Completed") ++activeCount;
    }
    int availableCount = 0;
    for (const auto& a : globalFleet) {
        if (a.status == AmbulanceStatus::Available) ++availableCount;
    }

    char rateBuf[16], distBuf[16];
    snprintf(rateBuf, sizeof(rateBuf), "%.0f%%", successRate);
    snprintf(distBuf, sizeof(distBuf), "%.1f", avgDistance);

    ostringstream out;
    out << "{\"ok\":true"
        << ",\"totalDispatches\":" << total
        << ",\"successCount\":" << successCount
        << ",\"successRate\":\"" << rateBuf << "\""
        << ",\"avgDistance\":\"" << distBuf << "\""
        << ",\"activeEmergencies\":" << activeCount
        << ",\"availableAmbulances\":" << availableCount
        << "}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// API: GET /api/history
// ===========================================================================
static string handleHistory() {
    ifstream file(DISPATCH_LOG);
    string line;
    ostringstream out;
    out << "{\"ok\":true,\"records\":[";

    bool first = true;
    if (file) {
        getline(file, line); // skip header
        while (getline(file, line)) {
            if (line.empty()) continue;

            size_t pos = 0;
            string f[12];
            for (int i = 0; i < 12 && pos <= line.size(); ++i) {
                f[i] = parseCsvField(line, pos);
            }

            if (!first) out << ',';
            first = false;

            out << "{"
                << "\"id\":" << (f[0].empty() ? "0" : f[0])
                << ",\"timestamp\":\"" << jsonEscape(f[1]) << "\""
                << ",\"patientName\":\"" << jsonEscape(f[2]) << "\""
                << ",\"phone\":\"" << jsonEscape(f[3]) << "\""
                << ",\"location\":" << (f[4].empty() ? "0" : f[4])
                << ",\"emergency\":\"" << jsonEscape(f[5]) << "\""
                << ",\"severity\":\"" << jsonEscape(f[6]) << "\""
                << ",\"ambulanceId\":" << (f[7].empty() ? "-1" : f[7])
                << ",\"ambulanceDistance\":" << (f[8].empty() ? "-1" : f[8])
                << ",\"hospitalName\":\"" << jsonEscape(f[9]) << "\""
                << ",\"hospitalDistance\":" << (f[10].empty() ? "-1" : f[10])
                << ",\"status\":\"" << jsonEscape(f[11]) << "\""
                << "}";
        }
    }

    out << "]}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// API: GET /api/ambulances  [NEW — Fleet Tracking]
// Returns the current state of the entire ambulance fleet.
// ===========================================================================
static string handleAmbulances() {
    RouteFinder routes = buildRouteNetwork();

    ostringstream out;
    out << "{\"ok\":true,\"ambulances\":[";
    for (size_t i = 0; i < globalFleet.size(); ++i) {
        if (i) out << ',';
        const auto& a = globalFleet[i];
        string status = ambulanceStatusStr(a.status);
        int loc = globalFleetLocations[i];

        // Calculate ETA to Central HQ (location 0) as a reference metric
        int etaToCentral = routes.shortestDistance(loc, 0);

        out << "{\"id\":" << a.id
            << ",\"driverId\":" << a.driverId
            << ",\"registration\":\"" << jsonEscape(a.registrationNumber) << "\""
            << ",\"location\":" << loc
            << ",\"locationName\":\"" << jsonEscape(locationName(loc)) << "\""
            << ",\"status\":\"" << status << "\""
            << ",\"etaToCentral\":" << etaToCentral
            << "}";
    }
    out << "]}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// API: POST /api/ambulance/update  [NEW — Simulate ambulance movement]
// Body: {"ambulanceId":N, "status":"Available"|"OnTrip"|..., "location":0-8}
// ===========================================================================
static string handleAmbulanceUpdate(const string& body) {
    int ambId = jsonInt(body, "ambulanceId", -1);
    string newStatus = jsonString(body, "status");
    int newLocation = jsonInt(body, "location", -1);

    if (ambId < 1)
        throw invalid_argument("Invalid ambulance ID.");

    bool found = false;
    for (size_t i = 0; i < globalFleet.size(); ++i) {
        if (globalFleet[i].id == ambId) {
            found = true;
            if (!newStatus.empty()) {
                globalFleet[i].status = parseAmbulanceStatus(newStatus);
            }
            if (newLocation >= 0 && newLocation <= 8) {
                globalFleetLocations[i] = newLocation;
                globalFleet[i].currentLocation = "Location " + to_string(newLocation);
            }
            break;
        }
    }

    if (!found)
        throw invalid_argument("Ambulance not found in fleet.");

    ostringstream out;
    out << "{\"ok\":true,\"message\":\"Ambulance " << ambId << " updated.\"}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// API: GET /api/emergencies  [NEW — Active Emergency Tracking]
// Returns all active (non-completed) emergencies with their timelines.
// ===========================================================================
static string handleEmergencies() {
    ostringstream out;
    out << "{\"ok\":true,\"emergencies\":[";
    bool first = true;
    for (const auto& e : activeEmergencies) {
        if (!first) out << ',';
        first = false;

        out << "{\"id\":" << e.id
            << ",\"patientName\":\"" << jsonEscape(e.patientName) << "\""
            << ",\"phone\":\"" << jsonEscape(e.phone) << "\""
            << ",\"location\":" << e.location
            << ",\"locationName\":\"" << jsonEscape(locationName(e.location)) << "\""
            << ",\"emergency\":\"" << jsonEscape(e.emergency) << "\""
            << ",\"severity\":\"" << jsonEscape(e.severity) << "\""
            << ",\"ambulanceId\":" << e.ambulanceId
            << ",\"ambulanceDistance\":" << e.ambulanceDistance
            << ",\"hospitalName\":\"" << jsonEscape(e.hospitalName) << "\""
            << ",\"hospitalDistance\":" << e.hospitalDistance
            << ",\"currentStatus\":\"" << jsonEscape(e.currentStatus) << "\""
            << ",\"timeline\":[";
        for (size_t t = 0; t < e.timeline.size(); ++t) {
            if (t) out << ',';
            out << "{\"timestamp\":\"" << jsonEscape(e.timeline[t].timestamp) << "\""
                << ",\"status\":\"" << jsonEscape(e.timeline[t].status) << "\""
                << ",\"message\":\"" << jsonEscape(e.timeline[t].message) << "\""
                << "}";
        }
        out << "]}";
    }
    out << "]}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// API: POST /api/emergency/update  [NEW — Advance emergency status]
// Body: {"emergencyId": N}
// Status progression: Dispatched → En-Route → Arrived → Completed
// Also updates the associated ambulance status and location.
// ===========================================================================
static string handleEmergencyUpdate(const string& body) {
    int emergencyId = jsonInt(body, "emergencyId", -1);
    if (emergencyId < 0)
        throw invalid_argument("Invalid emergency ID.");

    ActiveEmergency* found = nullptr;
    for (auto& e : activeEmergencies) {
        if (e.id == emergencyId) { found = &e; break; }
    }
    if (!found)
        throw invalid_argument("Emergency not found.");

    string newStatus;
    string message;
    string ts = currentTimestamp();

    if (found->currentStatus == "Dispatched") {
        newStatus = "En-Route";
        message = "Ambulance " + to_string(found->ambulanceId) + " is en route to patient";
        // Update ambulance status to OnTrip
        for (auto& a : globalFleet) {
            if (a.id == found->ambulanceId) {
                a.status = AmbulanceStatus::OnTrip;
                break;
            }
        }
    } else if (found->currentStatus == "En-Route") {
        newStatus = "Arrived";
        message = "Ambulance arrived at patient location (Node " + to_string(found->location) + ")";
        // Move ambulance to patient location
        for (size_t i = 0; i < globalFleet.size(); ++i) {
            if (globalFleet[i].id == found->ambulanceId) {
                globalFleetLocations[i] = found->location;
                globalFleet[i].currentLocation = "Location " + to_string(found->location);
                break;
            }
        }
    } else if (found->currentStatus == "Arrived") {
        newStatus = "Completed";
        message = "Patient transferred to " + found->hospitalName + ". Ambulance released.";
        // Move ambulance to hospital location and set Available
        for (size_t i = 0; i < globalFleet.size(); ++i) {
            if (globalFleet[i].id == found->ambulanceId) {
                globalFleetLocations[i] = found->hospitalLocationIdx;
                globalFleet[i].currentLocation = "Location " + to_string(found->hospitalLocationIdx);
                globalFleet[i].status = AmbulanceStatus::Available;
                break;
            }
        }
    } else if (found->currentStatus == "Pending") {
        // Emergency was created without an ambulance — can't advance
        throw invalid_argument("This emergency has no ambulance assigned. Dispatch an ambulance first.");
    } else {
        throw invalid_argument("Emergency has already been completed.");
    }

    found->currentStatus = newStatus;
    found->timeline.push_back({ts, newStatus, message});

    ostringstream out;
    out << "{\"ok\":true"
        << ",\"emergencyId\":" << emergencyId
        << ",\"newStatus\":\"" << jsonEscape(newStatus) << "\""
        << ",\"message\":\"" << jsonEscape(message) << "\""
        << "}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// API: GET /api/tracking  [NEW — Live ambulance tracking with ETA]
// Computes real-time position, distance remaining, and ETA for all active
// tracking sessions based on elapsed time since dispatch.
// ===========================================================================
static string handleTracking() {
    time_t now = time(nullptr);
    RouteFinder routes = buildRouteNetwork();

    ostringstream out;
    out << "{\"ok\":true,\"tracking\":[";
    bool first = true;

    for (auto& s : trackingSessions) {
        if (s.currentPhase == "completed") continue;

        double elapsed = difftime(now, s.phaseStartTime);
        double progress = 0.0;
        double distRemaining = 0.0;
        int etaSec = 0;
        double curX = 0, curY = 0;
        string phaseLabel;
        int fromNode = 0, toNode = 0;

        if (s.currentPhase == "to_patient") {
            double covered = elapsed * TrackingSession::SPEED_KM_PER_SEC;
            if (covered >= s.distanceToPatient) {
                // Auto-advance to at_patient
                s.currentPhase = "at_patient";
                s.phaseStartTime = now;
                elapsed = 0;
                // Move ambulance to patient location
                for (size_t i = 0; i < globalFleet.size(); ++i) {
                    if (globalFleet[i].id == s.ambulanceId) {
                        globalFleetLocations[i] = s.patientLocation;
                        globalFleet[i].currentLocation = "Location " + to_string(s.patientLocation);
                        globalFleet[i].status = AmbulanceStatus::OnTrip;
                        break;
                    }
                }
                // Update emergency to En-Route → Arrived
                for (auto& e : activeEmergencies) {
                    if (e.id == s.emergencyId) {
                        e.currentStatus = "Arrived";
                        e.timeline.push_back({currentTimestamp(), "Arrived",
                            "Ambulance arrived at patient location"});
                        break;
                    }
                }
                // Now in at_patient phase
                progress = 100;
                distRemaining = 0;
                etaSec = 0;
                fromNode = s.patientLocation;
                toNode = s.patientLocation;
                phaseLabel = "At patient — picking up";
                curX = nodeX[s.patientLocation];
                curY = nodeY[s.patientLocation];
            } else {
                progress = (covered / s.distanceToPatient) * 100.0;
                distRemaining = s.distanceToPatient - covered;
                etaSec = static_cast<int>(distRemaining / TrackingSession::SPEED_KM_PER_SEC);
                fromNode = s.startLocation;
                toNode = s.patientLocation;
                phaseLabel = "En route to patient";
                // Interpolate SVG position
                curX = nodeX[fromNode] + (nodeX[toNode] - nodeX[fromNode]) * (progress / 100.0);
                curY = nodeY[fromNode] + (nodeY[toNode] - nodeY[fromNode]) * (progress / 100.0);
            }
        }

        if (s.currentPhase == "at_patient") {
            double remaining = TrackingSession::PICKUP_DURATION - elapsed;
            if (remaining <= 0) {
                // Auto-advance to to_hospital
                s.currentPhase = "to_hospital";
                s.phaseStartTime = now;
                elapsed = 0;
                // Update emergency
                for (auto& e : activeEmergencies) {
                    if (e.id == s.emergencyId) {
                        e.currentStatus = "Arrived";
                        e.timeline.push_back({currentTimestamp(), "En-Route to Hospital",
                            "Patient picked up, heading to " + s.hospitalName});
                        break;
                    }
                }
                progress = 0;
                distRemaining = s.distanceToHospital;
                etaSec = static_cast<int>(distRemaining / TrackingSession::SPEED_KM_PER_SEC);
                fromNode = s.patientLocation;
                toNode = s.hospitalLocation;
                phaseLabel = "En route to hospital";
                curX = nodeX[s.patientLocation];
                curY = nodeY[s.patientLocation];
            } else {
                progress = ((TrackingSession::PICKUP_DURATION - remaining) / TrackingSession::PICKUP_DURATION) * 100.0;
                distRemaining = 0;
                etaSec = static_cast<int>(remaining);
                fromNode = s.patientLocation;
                toNode = s.patientLocation;
                phaseLabel = "At patient — picking up";
                curX = nodeX[s.patientLocation];
                curY = nodeY[s.patientLocation];
            }
        }

        if (s.currentPhase == "to_hospital") {
            double covered = elapsed * TrackingSession::SPEED_KM_PER_SEC;
            if (covered >= s.distanceToHospital) {
                // Auto-advance to completed
                s.currentPhase = "completed";
                // Move ambulance to hospital, set Available
                for (size_t i = 0; i < globalFleet.size(); ++i) {
                    if (globalFleet[i].id == s.ambulanceId) {
                        globalFleetLocations[i] = s.hospitalLocation;
                        globalFleet[i].currentLocation = "Location " + to_string(s.hospitalLocation);
                        globalFleet[i].status = AmbulanceStatus::Available;
                        break;
                    }
                }
                // Update emergency to Completed
                for (auto& e : activeEmergencies) {
                    if (e.id == s.emergencyId) {
                        e.currentStatus = "Completed";
                        e.timeline.push_back({currentTimestamp(), "Completed",
                            "Patient transferred to " + s.hospitalName + ". Ambulance released."});
                        break;
                    }
                }
                continue; // skip this session in output
            } else {
                progress = (covered / s.distanceToHospital) * 100.0;
                distRemaining = s.distanceToHospital - covered;
                etaSec = static_cast<int>(distRemaining / TrackingSession::SPEED_KM_PER_SEC);
                fromNode = s.patientLocation;
                toNode = s.hospitalLocation;
                phaseLabel = "En route to hospital";
                curX = nodeX[fromNode] + (nodeX[toNode] - nodeX[fromNode]) * (progress / 100.0);
                curY = nodeY[fromNode] + (nodeY[toNode] - nodeY[fromNode]) * (progress / 100.0);
            }
        }

        if (!first) out << ',';
        first = false;

        char progBuf[16], distBuf[16];
        snprintf(progBuf, sizeof(progBuf), "%.1f", progress);
        snprintf(distBuf, sizeof(distBuf), "%.1f", distRemaining);

        int totalDist = (s.currentPhase == "to_hospital" || s.currentPhase == "at_patient")
            ? s.distanceToHospital : s.distanceToPatient;

        out << "{\"emergencyId\":" << s.emergencyId
            << ",\"ambulanceId\":" << s.ambulanceId
            << ",\"registration\":\"" << jsonEscape(s.ambulanceReg) << "\""
            << ",\"patientName\":\"" << jsonEscape(s.patientName) << "\""
            << ",\"emergency\":\"" << jsonEscape(s.emergency) << "\""
            << ",\"severity\":\"" << jsonEscape(s.severity) << "\""
            << ",\"currentPhase\":\"" << jsonEscape(s.currentPhase) << "\""
            << ",\"phaseLabel\":\"" << jsonEscape(phaseLabel) << "\""
            << ",\"progress\":" << progBuf
            << ",\"distanceTotal\":" << totalDist
            << ",\"distanceRemaining\":" << distBuf
            << ",\"etaSeconds\":" << etaSec
            << ",\"speedKmH\":" << static_cast<int>(TrackingSession::SPEED_KM_PER_SEC * 3600)
            << ",\"fromNode\":" << fromNode
            << ",\"toNode\":" << toNode
            << ",\"currentX\":" << static_cast<int>(curX)
            << ",\"currentY\":" << static_cast<int>(curY)
            << ",\"hospitalName\":\"" << jsonEscape(s.hospitalName) << "\""
            << ",\"startLocation\":" << s.startLocation
            << ",\"patientLocation\":" << s.patientLocation
            << ",\"hospitalLocation\":" << s.hospitalLocation
            << "}";
    }
    out << "]}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// API: POST /api/fleet/reset  [NEW — Reset fleet to initial state]
// ===========================================================================
static string handleFleetReset() {
    globalFleet = {
        {1, 201, "DEMO-AMB-001", "Location 1", AmbulanceStatus::Available},
        {2, 202, "DEMO-AMB-002", "Location 2", AmbulanceStatus::Available},
        {3, 203, "DEMO-AMB-003", "Location 3", AmbulanceStatus::Available},
        {4, 204, "DEMO-AMB-004", "Location 4", AmbulanceStatus::Available},
        {5, 205, "DEMO-AMB-005", "Location 5", AmbulanceStatus::OnTrip}
    };
    globalFleetLocations = {1, 2, 3, 4, 5};
    activeEmergencies.clear();
    trackingSessions.clear();
    nextEmergencyId = 1;

    return httpResponse(200,
        "{\"ok\":true,\"message\":\"Fleet and emergencies reset to initial state.\"}");
}

// ===========================================================================
// API: POST /api/dispatch  [ENHANCED — uses global fleet + creates emergency]
// ===========================================================================
static string handleDispatch(const string& body) {
    string patientName  = jsonString(body, "patientName");
    string patientPhone = jsonString(body, "patientPhone");
    string emergency    = jsonString(body, "emergency");
    int patientLocation = jsonInt(body, "location", -1);
    int severityValue   = jsonInt(body, "severity", 0);

    if (patientName.empty() || emergency.empty())
        throw invalid_argument("Enter both patient name and emergency description.");
    if (patientLocation < 0 || patientLocation > 8)
        throw invalid_argument("Choose a sample location between 0 and 8.");
    if (severityValue < 1 || severityValue > 4)
        throw invalid_argument("Choose a valid severity level from 1 to 4.");

    // --- DSA Pipeline using global fleet state ---

    Severity severity = static_cast<Severity>(severityValue);
    Patient patient{101, patientName, patientPhone, 0, "",
                    "Location " + to_string(patientLocation),
                    emergency, severity};

    // Step 1: Emergency prioritization
    EmergencyQueue emergencyQueue;
    EmergencyRequest request{1, patient.id, patient.severity,
                             patient.location, ResponseStatus::Pending};
    emergencyQueue.addEmergency(request);
    EmergencyRequest nextEmergency = emergencyQueue.getNextEmergency();

    // Step 2: Build road network (Dijkstra graph)
    RouteFinder routes = buildRouteNetwork();

    // Step 3: Find nearest ambulances from GLOBAL FLEET
    AmbulanceSelector selector;
    vector<AmbulanceCandidate> candidates = selector.findNearestFour(
        globalFleet, globalFleetLocations, patientLocation, routes);

    // Simulated driver responses: even IDs accept, odd IDs are pending
    vector<AmbulanceResponse> responses;
    for (const auto& c : candidates) {
        AcceptanceStatus status = (c.ambulance.id % 2 == 0)
            ? AcceptanceStatus::Accepted
            : AcceptanceStatus::Pending;
        responses.push_back({c.ambulance.id, status});
    }

    // Step 4: Assignment (modifies globalFleet in-place!)
    DispatchManager manager;
    bool assigned = false;
    int ambulanceId = -1, ambulanceDistance = -1;
    string assignmentMessage = "No ambulance can be assigned yet.";
    try {
        AmbulanceCandidate selected =
            manager.selectAmbulance(candidates, responses, 1);
        if (manager.assignAmbulance(globalFleet, selected.ambulance.id)) {
            assigned = true;
            ambulanceId = selected.ambulance.id;
            ambulanceDistance = selected.distance;
            assignmentMessage = "Assignment successful.";
        } else {
            assignmentMessage = "Selected ambulance is no longer available.";
        }
    } catch (const exception& e) {
        assignmentMessage = e.what();
    }

    // Step 5: Hospital selection
    string hospitalName, hospitalMessage;
    int hospitalDistance = -1, availableBeds = 0, hospitalLocIdx = -1;
    try {
        HospitalSelector hospitalSelector;
        HospitalCandidate hospital = hospitalSelector.findNearestSuitableHospital(
            globalHospitals, globalHospitalLocations, patientLocation, routes);
        hospitalName     = hospital.hospital.name;
        hospitalDistance  = hospital.distance;
        availableBeds    = hospital.hospital.availableBeds;
        // Find hospital location index
        for (size_t i = 0; i < globalHospitals.size(); ++i) {
            if (globalHospitals[i].id == hospital.hospital.id) {
                hospitalLocIdx = globalHospitalLocations[i];
                break;
            }
        }
    } catch (const exception& e) {
        hospitalMessage = e.what();
    }

    // --- Persist to CSV ---
    string severityLabel = severityValue == 4 ? "Critical"
                         : severityValue == 3 ? "High"
                         : severityValue == 2 ? "Medium" : "Low";
    string statusLabel = assigned ? "Assigned" : "Pending";

    try {
        ensureCsvHeader(DISPATCH_LOG, CSV_HEADER);
        int recordId = nextRecordId(DISPATCH_LOG);
        ostringstream row;
        row << recordId << ','
            << csvEscape(currentTimestamp()) << ','
            << csvEscape(patientName) << ','
            << csvEscape(patientPhone) << ','
            << patientLocation << ','
            << csvEscape(emergency) << ','
            << csvEscape(severityLabel) << ','
            << ambulanceId << ','
            << ambulanceDistance << ','
            << csvEscape(hospitalName) << ','
            << hospitalDistance << ','
            << csvEscape(statusLabel);
        appendCsvRow(DISPATCH_LOG, row.str());
    } catch (const exception& e) {
        cerr << "Warning: could not save dispatch log: " << e.what() << '\n';
    }

    // --- Create Active Emergency ---
    string ts = currentTimestamp();
    ActiveEmergency newEmergency;
    newEmergency.id = nextEmergencyId++;
    newEmergency.patientName = patientName;
    newEmergency.phone = patientPhone;
    newEmergency.location = patientLocation;
    newEmergency.emergency = emergency;
    newEmergency.severity = severityLabel;
    newEmergency.ambulanceId = ambulanceId;
    newEmergency.ambulanceDistance = ambulanceDistance;
    newEmergency.hospitalName = hospitalName;
    newEmergency.hospitalDistance = hospitalDistance;
    newEmergency.hospitalLocationIdx = hospitalLocIdx;

    if (assigned) {
        newEmergency.currentStatus = "Dispatched";
        newEmergency.timeline.push_back({ts, "Received", "Emergency call received and logged"});
        newEmergency.timeline.push_back({ts, "Dispatched",
            "Ambulance " + to_string(ambulanceId) + " dispatched (" +
            to_string(ambulanceDistance) + " km away)"});
    } else {
        newEmergency.currentStatus = "Pending";
        newEmergency.timeline.push_back({ts, "Received", "Emergency call received and logged"});
        newEmergency.timeline.push_back({ts, "Pending",
            "Waiting for ambulance — " + assignmentMessage});
    }
    activeEmergencies.push_back(newEmergency);

    // --- Create Live Tracking Session ---
    if (assigned) {
        // Find the ambulance's start location
        int ambStartLoc = patientLocation; // fallback
        string ambReg;
        for (size_t i = 0; i < globalFleet.size(); ++i) {
            if (globalFleet[i].id == ambulanceId) {
                ambStartLoc = globalFleetLocations[i];
                ambReg = globalFleet[i].registrationNumber;
                break;
            }
        }
        // Compute distance from patient to hospital
        int distPatToHosp = 0;
        if (hospitalLocIdx >= 0) {
            distPatToHosp = routes.shortestDistance(patientLocation, hospitalLocIdx);
            if (distPatToHosp < 0) distPatToHosp = 0;
        }

        TrackingSession session;
        session.emergencyId = newEmergency.id;
        session.ambulanceId = ambulanceId;
        session.ambulanceReg = ambReg;
        session.patientName = patientName;
        session.emergency = emergency;
        session.severity = severityLabel;
        session.startLocation = ambStartLoc;
        session.patientLocation = patientLocation;
        session.hospitalLocation = hospitalLocIdx >= 0 ? hospitalLocIdx : 0;
        session.hospitalName = hospitalName;
        session.distanceToPatient = ambulanceDistance;
        session.distanceToHospital = distPatToHosp;
        session.currentPhase = "to_patient";
        session.phaseStartTime = time(nullptr);
        session.dispatchTime = session.phaseStartTime;
        trackingSessions.push_back(session);

        // Set ambulance to OnTrip immediately
        for (auto& a : globalFleet) {
            if (a.id == ambulanceId) {
                a.status = AmbulanceStatus::OnTrip;
                break;
            }
        }
        // Update emergency to En-Route
        for (auto& e : activeEmergencies) {
            if (e.id == newEmergency.id) {
                e.currentStatus = "En-Route";
                e.timeline.push_back({ts, "En-Route",
                    "Ambulance " + to_string(ambulanceId) + " en route (" +
                    to_string(ambulanceDistance) + " km, ETA ~" +
                    to_string(static_cast<int>(ambulanceDistance / TrackingSession::SPEED_KM_PER_SEC)) + "s)"});
                break;
            }
        }
    }

    // --- Build JSON response ---
    ostringstream out;
    out << "{\"ok\":true"
        << ",\"emergencyId\":" << newEmergency.id
        << ",\"patientName\":\"" << jsonEscape(patientName) << "\""
        << ",\"emergency\":\"" << jsonEscape(emergency) << "\""
        << ",\"location\":" << patientLocation
        << ",\"severity\":" << static_cast<int>(nextEmergency.severity)
        << ",\"severityLabel\":\"" << severityLabel << "\""
        << ",\"assigned\":" << (assigned ? "true" : "false")
        << ",\"ambulanceId\":" << ambulanceId
        << ",\"ambulanceDistance\":" << ambulanceDistance
        << ",\"assignmentMessage\":\"" << jsonEscape(assignmentMessage) << "\""
        << ",\"hospitalName\":\"" << jsonEscape(hospitalName) << "\""
        << ",\"hospitalDistance\":" << hospitalDistance
        << ",\"availableBeds\":" << availableBeds
        << ",\"hospitalMessage\":\"" << jsonEscape(hospitalMessage) << "\""
        << ",\"logicSummary\":\"Priority Queue ranked severity; Dijkstra calculated "
           "sample-network distances; nearest-four selection filtered unavailable "
           "ambulances; DispatchManager applied acceptance and distance-tolerance "
           "rules; HospitalSelector checked facilities and available beds.\""
        << ",\"candidates\":[";
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i) out << ',';
        out << "{\"id\":" << candidates[i].ambulance.id
            << ",\"distance\":" << candidates[i].distance << '}';
    }
    out << "]}";
    return httpResponse(200, out.str());
}

// ===========================================================================
// Request Router
// ===========================================================================
static void handleClient(int client) {
    // Read the full HTTP request (headers + body)
    string request;
    char buffer[4096];
    while (request.find("\r\n\r\n") == string::npos) {
        ssize_t n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) { close(client); return; }
        request.append(buffer, static_cast<size_t>(n));
        if (request.size() > 1024 * 1024) { close(client); return; }
    }

    size_t headerEnd = request.find("\r\n\r\n");
    string headers = request.substr(0, headerEnd);
    istringstream headerStream(headers);
    string method, path, version;
    headerStream >> method >> path >> version;

    // Parse Content-Length
    size_t contentLength = 0;
    size_t cl = headers.find("Content-Length:");
    if (cl != string::npos) {
        cl += string("Content-Length:").size();
        while (cl < headers.size() &&
               isspace(static_cast<unsigned char>(headers[cl])))
            ++cl;
        try {
            contentLength = static_cast<size_t>(stoul(headers.substr(cl)));
        } catch (...) {}
    }

    // Read remaining body
    string body = request.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        ssize_t n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        body.append(buffer, static_cast<size_t>(n));
    }

    // Request logging
    cout << "[" << currentTimestamp() << "] " << method << " " << path << "\n";

    // Route the request
    string response;

    // CORS preflight
    if (method == "OPTIONS") {
        response = httpResponse(200, "");

    // Static files
    } else if (method == "GET" && (path == "/" || path == "/index.html")) {
        string page = readFile("public/index.html");
        response = page.empty()
            ? httpResponse(404, "{\"ok\":false,\"error\":\"Cannot find public/index.html. Run from the project folder.\"}")
            : httpResponse(200, page, "text/html; charset=utf-8");

    } else if (method == "GET" && path == "/styles.css") {
        response = httpResponse(200, readFile("public/styles.css"),
                                "text/css; charset=utf-8");

    } else if (method == "GET" && path == "/app.js") {
        response = httpResponse(200, readFile("public/app.js"),
                                "application/javascript; charset=utf-8");

    // Auth API
    } else if (method == "POST" && path == "/api/login") {
        response = handleLogin(body);

    // Dispatch API
    } else if (method == "POST" && path == "/api/dispatch") {
        try {
            response = handleDispatch(body);
        } catch (const exception& e) {
            response = httpResponse(400,
                string("{\"ok\":false,\"error\":\"") + jsonEscape(e.what()) + "\"}");
        }

    // History & Stats API
    } else if (method == "GET" && path == "/api/history") {
        response = handleHistory();

    } else if (method == "GET" && path == "/api/stats") {
        response = handleStats();

    // Fleet Tracking API [NEW]
    } else if (method == "GET" && path == "/api/ambulances") {
        response = handleAmbulances();

    } else if (method == "POST" && path == "/api/ambulance/update") {
        try {
            response = handleAmbulanceUpdate(body);
        } catch (const exception& e) {
            response = httpResponse(400,
                string("{\"ok\":false,\"error\":\"") + jsonEscape(e.what()) + "\"}");
        }

    // Emergency Management API [NEW]
    } else if (method == "GET" && path == "/api/emergencies") {
        response = handleEmergencies();

    } else if (method == "POST" && path == "/api/emergency/update") {
        try {
            response = handleEmergencyUpdate(body);
        } catch (const exception& e) {
            response = httpResponse(400,
                string("{\"ok\":false,\"error\":\"") + jsonEscape(e.what()) + "\"}");
        }

    // Live Tracking API [NEW]
    } else if (method == "GET" && path == "/api/tracking") {
        response = handleTracking();

    // Fleet Reset API [NEW]
    } else if (method == "POST" && path == "/api/fleet/reset") {
        response = handleFleetReset();

    // 404
    } else {
        response = httpResponse(404,
            "{\"ok\":false,\"error\":\"Route not found.\"}");
    }

    // Send response
    size_t sent = 0;
    while (sent < response.size()) {
        ssize_t n = send(client, response.data() + sent,
                         response.size() - sent, 0);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
    close(client);
}

// ===========================================================================
// Main: create and run the HTTP server on port 8080
// ===========================================================================
int main() {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        cerr << "Could not create server socket.\n";
        return 1;
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port        = htons(8080);

    if (::bind(server, reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) < 0) {
        cerr << "Could not bind to port 8080. Is another server already using it?\n";
        close(server);
        return 1;
    }

    if (listen(server, 10) < 0) {
        cerr << "Could not start listening.\n";
        close(server);
        return 1;
    }

    cout << "============================================\n";
    cout << "  PulsePath v2.0 — C++ Web Server\n";
    cout << "  Open http://127.0.0.1:8080 in your browser.\n";
    cout << "  Login: admin / 1234\n";
    cout << "  Press Ctrl+C to stop.\n";
    cout << "============================================\n";
    cout << "\n  API Endpoints:\n";
    cout << "    POST /api/login            Auth\n";
    cout << "    POST /api/dispatch          Full DSA pipeline\n";
    cout << "    GET  /api/stats             Aggregate metrics\n";
    cout << "    GET  /api/history           Dispatch log\n";
    cout << "    GET  /api/ambulances        Fleet status\n";
    cout << "    POST /api/ambulance/update  Move/update ambulance\n";
    cout << "    GET  /api/emergencies       Active emergencies\n";
    cout << "    POST /api/emergency/update  Advance emergency status\n";
    cout << "    POST /api/fleet/reset       Reset simulation\n";
    cout << "    GET  /api/tracking          Live ambulance tracking\n";
    cout << "\n============================================\n\n";

    while (true) {
        int client = accept(server, nullptr, nullptr);
        if (client >= 0) handleClient(client);
    }

    close(server);
    return 0;
}
