#pragma once

#include "services/auth_service.h"

#include <string>

namespace hms {

struct UserManagementResponse {
    int status = 500;
    std::string body;
};

UserManagementResponse handle_user_deactivation(
    const std::string& method,
    const std::string& path,
    const std::string& authorization,
    const std::string& content_type,
    const std::string& body,
    bool has_content_length,
    const std::string& source_ip,
    const std::string& request_id,
    AuthService& auth_service);

} // namespace hms
