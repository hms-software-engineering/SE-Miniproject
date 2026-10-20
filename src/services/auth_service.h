
#pragma once

#include "services/rbac_service.h"

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>

namespace hms {

struct AuthResult {
    bool success;
    std::string user_id;
    std::string message;
    Role role = Role::Patient;
};

struct SessionResult {
    bool success;
    std::string token;
    std::string user_id;
    std::string message;
    Role role = Role::Patient;
};

struct SessionValidationResult {
    bool valid;
    std::string user_id;
    Role role = Role::Patient;
};

class AuthService {
public:
    using Clock = std::chrono::steady_clock;
    using TimeProvider = std::function<Clock::time_point()>;

    explicit AuthService(TimeProvider time_provider = {});

    AuthResult register_user(const std::string& username,
                             const std::string& password);

    AuthResult authenticate_user(const std::string& username,
                                 const std::string& password);

    bool assign_role(const std::string& username, Role role);

    // Create a session after successful authentication.
    SessionResult create_session(const AuthResult& auth_result);

    // Validate a token and refresh its inactivity timer.
    SessionValidationResult validate_session(const std::string& token);

    // Invalidate a session immediately.
    bool logout(const std::string& token);

private:
    struct Session {
        std::string user_id;
        Role role = Role::Patient;
        Clock::time_point last_activity;
    };

    TimeProvider time_provider_;

    std::unordered_map<std::string, Session> sessions_;
    std::mutex sessions_mutex_;
};

} // namespace hms
