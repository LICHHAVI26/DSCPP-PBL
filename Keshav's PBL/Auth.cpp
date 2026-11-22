#include <iostream>
#include "Auth.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <CommonCrypto/CommonDigest.h>

using namespace std;

static vector<Driver> registeredDrivers;
static const string DRIVER_FILE = "drivers.csv";

// Existing admin login
bool login(const string& username, const string& password) {
    return username == "admin" && password == "1234";
}

// SHA-256 password hashing
string hashPassword(const string& password) {
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];

    CC_SHA256(
        password.data(),
        static_cast<CC_LONG>(password.size()),
        hash
    );

    stringstream result;
    result << hex << setfill('0');

    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++) {
        result << setw(2) << static_cast<int>(hash[i]);
    }

    return result.str();
}

// Escape CSV values
static string escapeCSV(const string& value) {
    string result = "\"";

    for (char ch : value) {
        if (ch == '"') {
            result += "\"\"";
        } else {
            result += ch;
        }
    }

    result += "\"";
    return result;
}

// Parse CSV rows safely
static vector<string> parseCSVRow(const string& line) {
    vector<string> fields;
    string field;
    bool insideQuotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char ch = line[i];

        if (ch == '"') {
            if (insideQuotes && i + 1 < line.size()
                && line[i + 1] == '"') {
                field += '"';
                i++;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (ch == ',' && !insideQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += ch;
        }
    }

    fields.push_back(field);
    return fields;
}

// Save driver records to CSV
bool saveDrivers() {
    ofstream file(DRIVER_FILE);

    if (!file.is_open()) {
        return false;
    }

    file << "id,name,licenseNumber,licenseVerified,rating,"
         << "phone,passwordHash,ambulanceId,currentLocation,"
         << "approved,isAvailable,rejected\n";

    for (const Driver& d : registeredDrivers) {
        file << d.id << ","
             << escapeCSV(d.name) << ","
             << escapeCSV(d.licenseNumber) << ","
             << d.licenseVerified << ","
             << d.rating << ","
             << escapeCSV(d.phone) << ","
             << escapeCSV(d.passwordHash) << ","
             << d.ambulanceId << ","
             << d.currentLocation << ","
             << d.approved << ","
             << d.isAvailable << ","
             << d.rejected << "\n";
    }
cout << "Saving drivers with 12-column format" << endl;
    return file.good();
}

// Load driver records when server starts
void loadDrivers() {
    registeredDrivers.clear();

    ifstream file(DRIVER_FILE);

    if (!file.is_open()) {
        saveDrivers();
        return;
    }

    string line;

    // Skip CSV header
    getline(file, line);

    while (getline(file, line)) {
        vector<string> fields = parseCSVRow(line);

        if (fields.size() != 11 && fields.size() != 12) {
          continue;
        }

        try {
            Driver d;

            d.id = stoi(fields[0]);
            d.name = fields[1];
            d.licenseNumber = fields[2];
            d.licenseVerified = (fields[3] == "1");
            d.rating = stod(fields[4]);
            d.phone = fields[5];
            d.passwordHash = fields[6];
            d.ambulanceId = stoi(fields[7]);
            d.currentLocation = stoi(fields[8]);
            d.approved = (fields[9] == "1");
            d.isAvailable = (fields[10] == "1");
            d.rejected = (fields.size() == 12 && fields[11] == "1");

            registeredDrivers.push_back(d);

        } catch (...) {
            // Skip invalid records
        }
    }
}

// Find driver by phone
int findDriverByPhone(const string& phone) {
    for (int i = 0; i < static_cast<int>(registeredDrivers.size()); i++) {
        if (registeredDrivers[i].phone == phone) {
            return i;
        }
    }

    return -1;
}

// Find driver by ID
int findDriverById(int driverId) {
    for (int i = 0; i < static_cast<int>(registeredDrivers.size()); i++) {
        if (registeredDrivers[i].id == driverId) {
            return i;
        }
    }

    return -1;
}

// Register a driver
bool registerDriver(const Driver& driver) {
    if (driver.name.empty() ||
        driver.phone.empty() ||
        driver.licenseNumber.empty() ||
        driver.passwordHash.empty()) {
        return false;
    }

    if (findDriverByPhone(driver.phone) != -1) {
        return false;
    }

    Driver newDriver = driver;

    int highestId = 0;

    for (const Driver& existing : registeredDrivers) {
        highestId = max(highestId, existing.id);
    }

    if (newDriver.id <= 0) {
        newDriver.id = highestId + 1;
    }

    registeredDrivers.push_back(newDriver);

    if (!saveDrivers()) {
        registeredDrivers.pop_back();
        return false;
    }

    return true;
}

// Driver login
bool driverLogin(const string& phone, const string& password) {
    int index = findDriverByPhone(phone);

    if (index == -1) {
        return false;
    }

    return registeredDrivers[index].passwordHash == hashPassword(password);
}


// Update driver approval or rejection
bool updateDriverApproval(int driverId, bool approve) {
    int index = findDriverById(driverId);

    if (index == -1) {
        return false;
    }

    Driver oldDriver = registeredDrivers[index];

    if (approve) {
        registeredDrivers[index].approved = true;
        registeredDrivers[index].rejected = false;
    } else {
        registeredDrivers[index].approved = false;
        registeredDrivers[index].rejected = true;
        registeredDrivers[index].isAvailable = false;
    }

    if (!saveDrivers()) {
        registeredDrivers[index] = oldDriver;
        return false
    }

    return true;
}

