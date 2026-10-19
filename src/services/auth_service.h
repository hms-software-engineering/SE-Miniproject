
#pragma once

#include "services/rbac_service.h"

#include <string>

namespace hms {

struct AuthResult {
    bool success;
    std::string user_id;
    std::string message;
    Role role = Role::Patient;
};

class AuthService {
public:
    // Public registration always creates a Patient account.
    AuthResult register_user(const std::string& username,
                             const std::string& password);

    // Authentication returns the role stored for the account.
    AuthResult authenticate_user(const std::string& username,
                                 const std::string& password);

    // Administrative role assignment for trusted internal use.
    // Do not expose this directly through public registration.
    bool assign_role(const std::string& username, Role role);
};

} // namespace hms
