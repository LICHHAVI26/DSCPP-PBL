
#ifndef AUTH_H
#define AUTH_H

#include <string>
#include "Models.h"

// Existing admin login
bool login(const std::string& username,
           const std::string& password);

// Password hashing
std::string hashPassword(const std::string& password);

// Driver file handling
void loadDrivers();
bool saveDrivers();

// Driver registration
bool registerDriver(const Driver& driver);

// Admin driver approval
bool updateDriverApproval(int driverId, bool approve);

// Driver login
bool driverLogin(const std::string& phone,
                 const std::string& password);

// Driver lookup
int findDriverByPhone(const std::string& phone);
int findDriverById(int driverId);

#endif
