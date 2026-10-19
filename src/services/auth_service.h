
#pragma once

#include <string>

namespace hms {

struct AuthResult {
    bool success;
    std::string user_id;
    std::string message;
};

class AuthService {
public:
    AuthResult register_user(const std::string& username,
                             const std::string& password);

    AuthResult authenticate_user(const std::string& username,
                                 const std::string& password);
};

} // namespace hms
