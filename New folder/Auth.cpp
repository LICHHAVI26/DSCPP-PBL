#include "Auth.h"

bool login(const std::string& username,
           const std::string& password) {
    return username == "admin" && password == "1234";
}