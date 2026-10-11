// Small macOS/POSIX HTTP server for the RapidAid demo.
// It reuses the existing DSA modules and leaves main.cpp unchanged.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Models.h"
#include "EmergencyQueue.h"
#include "RouteFinder.h"
#include "AmbulanceSelector.h"
#include "DispatchManager.h"
#include "HospitalSelector.h"

using namespace std;

static string readFile(const string& path) {
    ifstream file(path, ios::binary);
    if (!file) return "";
    ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

static string jsonEscape(const string& input) {
    string out;
    for (unsigned char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
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

static string jsonResponse(int status, const string& body, const string& type = "application/json; charset=utf-8") {
    string reason = status == 200 ? "OK" : status == 400 ? "Bad Request" : "Not Found";
    ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
             << "Content-Type: " << type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "Cache-Control: no-store\r\n\r\n" << body;
    return response.str();
}

static string dispatchJson(const string& body) {
    string patientName = jsonString(body, "patientName");
    string emergency = jsonString(body, "emergency");
    int patientLocation = jsonInt(body, "location", -1);
    int severityValue = jsonInt(body, "severity", 0);

    if (patientName.empty() || emergency.empty())
        throw invalid_argument("Enter both patient name and emergency description.");
    if (patientLocation < 0 || patientLocation > 8)
        throw invalid_argument("Choose a sample location between 0 and 8.");
    if (severityValue < 1 || severityValue > 4)
        throw invalid_argument("Choose a valid severity level from 1 to 4.");

    Severity severity = static_cast<Severity>(severityValue);
    Patient patient{101, patientName, "Location " + to_string(patientLocation), emergency, severity};

    EmergencyQueue emergencyQueue;
    EmergencyRequest request{1, patient.id, patient.severity, patient.location, ResponseStatus::Pending};
    emergencyQueue.addEmergency(request);
    EmergencyRequest nextEmergency = emergencyQueue.getNextEmergency();

    RouteFinder routes(9);
    routes.addRoad(0, 1, 4);
    routes.addRoad(0, 2, 5);
    routes.addRoad(0, 3, 8);
    routes.addRoad(0, 4, 10);
    routes.addRoad(0, 5, 2);
    routes.addRoad(0, 6, 3);
    routes.addRoad(0, 7, 5);
    routes.addRoad(0, 8, 7);

    // For non-central demo nodes, connect the node distances to the chosen patient node.
    // This is a simulated topology for evaluation, not a real road map.
    vector<Ambulance> ambulances = {
        {1, 201, "Location 1", AmbulanceStatus::Available},
        {2, 202, "Location 2", AmbulanceStatus::Available},
        {3, 203, "Location 3", AmbulanceStatus::Available},
        {4, 204, "Location 4", AmbulanceStatus::Available},
        {5, 205, "Location 5", AmbulanceStatus::OnTrip}
    };
    vector<int> ambulanceLocations = {1, 2, 3, 4, 5};
    AmbulanceSelector selector;
    vector<AmbulanceCandidate> candidates = selector.findNearestFour(
        ambulances, ambulanceLocations, patientLocation, routes);

    vector<AmbulanceResponse> responses;
    for (const auto& c : candidates) {
        AcceptanceStatus status = AcceptanceStatus::Rejected;
        if (c.ambulance.id == 2 || c.ambulance.id == 4)
            status = AcceptanceStatus::Accepted;
        else if (c.ambulance.id == 1 || c.ambulance.id == 3)
            status = AcceptanceStatus::Pending;
        responses.push_back({c.ambulance.id, status});
    }

    DispatchManager manager;
    bool assigned = false;
    int ambulanceId = -1, ambulanceDistance = -1;
    string assignmentMessage = "No ambulance can be assigned yet.";
    try {
        AmbulanceCandidate selected = manager.selectAmbulance(candidates, responses, 1);
        if (manager.assignAmbulance(ambulances, selected.ambulance.id)) {
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

    vector<Hospital> hospitals = {
        {1, "City Hospital", "Location 6", 5, true},
        {2, "General Hospital", "Location 7", 10, true},
        {3, "Small Clinic", "Location 8", 0, true}
    };
    vector<int> hospitalLocations = {6, 7, 8};
    string hospitalName, hospitalMessage;
    int hospitalDistance = -1, availableBeds = 0;
    try {
        HospitalSelector hospitalSelector;
        HospitalCandidate hospital = hospitalSelector.findNearestSuitableHospital(
            hospitals, hospitalLocations, patientLocation, routes);
        hospitalName = hospital.hospital.name;
        hospitalDistance = hospital.distance;
        availableBeds = hospital.hospital.availableBeds;
    } catch (const exception& e) {
        hospitalMessage = e.what();
    }

    string severityLabel = severityValue == 4 ? "Critical" :
                           severityValue == 3 ? "High" :
                           severityValue == 2 ? "Medium" : "Low";

    ostringstream out;
    out << "{\"ok\":true"
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
        << ",\"logicSummary\":\"Priority Queue ranked severity; Dijkstra calculated sample-network distances; nearest-four selection filtered unavailable ambulances; DispatchManager applied acceptance and distance-tolerance rules; HospitalSelector checked facilities and available beds.\""
        << ",\"candidates\":[";
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i) out << ',';
        out << "{\"id\":" << candidates[i].ambulance.id
            << ",\"distance\":" << candidates[i].distance << '}';
    }
    out << "]}";
    return out.str();
}

static void handleClient(int client) {
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

    size_t contentLength = 0;
    size_t cl = headers.find("Content-Length:");
    if (cl != string::npos) {
        cl += string("Content-Length:").size();
        while (cl < headers.size() && isspace(static_cast<unsigned char>(headers[cl]))) ++cl;
        try { contentLength = static_cast<size_t>(stoul(headers.substr(cl))); } catch (...) {}
    }

    string body = request.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        ssize_t n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        body.append(buffer, static_cast<size_t>(n));
    }

    string response;
    if (method == "GET" && (path == "/" || path == "/index.html")) {
        string page = readFile("public/index.html");
        response = page.empty()
            ? jsonResponse(404, "{\"ok\":false,\"error\":\"Cannot find public/index.html. Run from the project folder.\"}")
            : jsonResponse(200, page, "text/html; charset=utf-8");
    } else if (method == "GET" && path == "/styles.css") {
        response = jsonResponse(200, readFile("public/styles.css"), "text/css; charset=utf-8");
    } else if (method == "GET" && path == "/app.js") {
        response = jsonResponse(200, readFile("public/app.js"), "application/javascript; charset=utf-8");
    } else if (method == "POST" && path == "/api/dispatch") {
        try {
            response = jsonResponse(200, dispatchJson(body));
        } catch (const exception& e) {
            response = jsonResponse(400, string("{\"ok\":false,\"error\":\"") + jsonEscape(e.what()) + "\"}");
        }
    } else {
        response = jsonResponse(404, "{\"ok\":false,\"error\":\"Route not found.\"}");
    }

    size_t sent = 0;
    while (sent < response.size()) {
        ssize_t n = send(client, response.data() + sent, response.size() - sent, 0);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
    close(client);
}

int main() {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { cerr << "Could not create server socket.\n"; return 1; }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(8080);

   if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        cerr << "Could not bind to port 8080. Is another server already using it?\n";
        close(server);
        return 1;
    }
    if (listen(server, 10) < 0) {
        cerr << "Could not start listening.\n";
        close(server);
        return 1;
    }

    cout << "RapidAid C++ web server is running.\n";
    cout << "Open http://127.0.0.1:8080 in your browser.\n";
    cout << "Press Ctrl+C to stop.\n";

    while (true) {
        int client = accept(server, nullptr, nullptr);
        if (client >= 0) handleClient(client);
    }
    close(server);
    return 0;
}
