#pragma once

#include "services/auth_service.h"

#include <string>

namespace hms {

struct AuditLogResponse {
    int status = 500;
    std::string body;
};

AuditLogResponse handle_audit_log(const std::string& method,
                                  const std::string& path,
                                  const std::string& query,
                                  const std::string& authorization,
                                  const std::string& source_ip,
                                  const std::string& request_id,
                                  AuthService& auth_service);

} // namespace hms
