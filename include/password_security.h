
#pragma once

#include <string>

namespace hms::security {

bool is_password_strong(const std::string& password);

bool hash_password(
    const std::string& password,
    std::string& encoded_hash
);

bool verify_password(
    const std::string& password,
    const std::string& encoded_hash
);

} // namespace hms::security
