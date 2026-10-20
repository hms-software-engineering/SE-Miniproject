
#pragma once

#include "services/rbac_service.h"

#include <chrono>
#include <functional>
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
    using Clock = std::chrono::steady_clock;
    using TimeProvider = std::function<Clock::time_point()>;

    // The optional clock makes lockout expiry deterministic in tests.
    explicit AuthService(TimeProvider time_provider = {});

    // Public registration always creates a Patient account.
    AuthResult register_user(const std::string& username,
                             const std::string& password);

    // Authentication returns the role stored for the account.
    AuthResult authenticate_user(const std::string& username,
                                 const std::string& password);

    // Administrative role assignment for trusted internal use.
    bool assign_role(const std::string& username, Role role);

private:
    TimeProvider time_provider_;
};

} // namespace hms
