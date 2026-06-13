#pragma once
#include <string>

namespace core {
class Authenticator {
public:
    static bool authenticate();
    
private:
    static bool verify_license(const std::string& license_key, const std::string& username = "");
};
}
