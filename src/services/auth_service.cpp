
#include "services/auth_service.h"

#include <argon2.h>
#include <windows.h>
#include <bcrypt.h>

#include <cctype>
#include <cstdint>
#include <mutex>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string>

namespace hms {
namespace {

using Clock = AuthService::Clock;

struct StoredUser {
    std::string user_id;
    std::vector<std::uint8_t> salt;
    std::vector<std::uint8_t> hash;
    Role role = Role::Patient;

    unsigned int failed_attempts = 0;
    Clock::time_point locked_until{};
};

std::unordered_map<std::string, StoredUser> users;
std::mutex users_mutex;

constexpr std::size_t SALT_LENGTH = 16;
constexpr std::size_t HASH_LENGTH = 32;
constexpr std::uint32_t ITERATIONS = 3;
constexpr std::uint32_t MEMORY_KIB = 65536;
constexpr std::uint32_t PARALLELISM = 1;
constexpr unsigned int MAX_FAILED_ATTEMPTS = 5;
constexpr auto LOCKOUT_DURATION = std::chrono::minutes(15);
constexpr auto SESSION_TIMEOUT = std::chrono::minutes(10);

bool valid_role(Role role) {
    switch (role) {
    case Role::Patient:
    case Role::Receptionist:
    case Role::Doctor:
    case Role::BillingOfficer:
    case Role::Administrator:
        return true;
    }

    return false;
}

bool valid_password(const std::string& password) {
    if (password.size() < 8) {
        return false;
    }

    bool upper = false;
    bool lower = false;
    bool digit = false;
    bool special = false;

    for (unsigned char ch : password) {
        upper |= std::isupper(ch) != 0;
        lower |= std::islower(ch) != 0;
        digit |= std::isdigit(ch) != 0;
        special |= std::ispunct(ch) != 0;
    }

    return upper && lower && digit && special;
}

std::vector<std::uint8_t> random_bytes(std::size_t length) {
    std::random_device rd;
    std::vector<std::uint8_t> bytes(length);

    for (auto& byte : bytes) {
        byte = static_cast<std::uint8_t>(rd() & 0xFF);
    }

    return bytes;
}

// Generates a cryptographically random 256-bit session token.
std::string generate_session_token() {
    constexpr std::size_t TOKEN_BYTES = 32;
    std::vector<std::uint8_t> bytes(TOKEN_BYTES);

    if (BCryptGenRandom(
            nullptr,
            bytes.data(),
            static_cast<ULONG>(bytes.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return "";
    }

    static constexpr char HEX[] = "0123456789abcdef";
    std::string token;
    token.reserve(TOKEN_BYTES * 2);

    for (std::uint8_t byte : bytes) {
        token.push_back(HEX[byte >> 4]);
        token.push_back(HEX[byte & 0x0F]);
    }

    return token;
}

bool hash_password(const std::string& password,
                   const std::vector<std::uint8_t>& salt,
                   std::vector<std::uint8_t>& hash) {
    hash.resize(HASH_LENGTH);

    return argon2id_hash_raw(
        ITERATIONS,
        MEMORY_KIB,
        PARALLELISM,
        password.data(),
        password.size(),
        salt.data(),
        salt.size(),
        hash.data(),
        hash.size()
    ) == ARGON2_OK;
}

bool hashes_equal(const std::vector<std::uint8_t>& a,
                  const std::vector<std::uint8_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }

    unsigned char difference = 0;

    for (std::size_t i = 0; i < a.size(); ++i) {
        difference |= a[i] ^ b[i];
    }

    return difference == 0;
}

AuthResult invalid_credentials() {
    return {false, "", "Invalid username or password"};
}

} // namespace

AuthService::AuthService(TimeProvider time_provider)
    : time_provider_(
          time_provider
              ? std::move(time_provider)
              : TimeProvider{[] { return Clock::now(); }}) {}

AuthResult AuthService::register_user(const std::string& username,
                                      const std::string& password) {
    if (username.empty()) {
        return {false, "", "Username is required"};
    }

    if (!valid_password(password)) {
        return {
            false, "",
            "Password must be at least 8 characters and include uppercase, "
            "lowercase, digit, and special character"
        };
    }

    std::lock_guard<std::mutex> lock(users_mutex);

    if (users.find(username) != users.end()) {
        return {false, "", "Username already exists"};
    }

    StoredUser user;
    user.user_id = username;
    user.role = Role::Patient;
    user.salt = random_bytes(SALT_LENGTH);

    if (!hash_password(password, user.salt, user.hash)) {
        return {false, "", "Unable to process password"};
    }

    users.emplace(username, std::move(user));

    return {
        true, username, "Registration successful", Role::Patient
    };
}

AuthResult AuthService::authenticate_user(const std::string& username,
                                          const std::string& password) {
    std::lock_guard<std::mutex> lock(users_mutex);

    const auto it = users.find(username);

    if (it == users.end()) {
        return invalid_credentials();
    }

    StoredUser& user = it->second;
    const auto now = time_provider_();

    if (now < user.locked_until) {
        return invalid_credentials();
    }

    if (user.locked_until != Clock::time_point{} &&
        now >= user.locked_until) {
        user.locked_until = Clock::time_point{};
        user.failed_attempts = 0;
    }

    std::vector<std::uint8_t> candidate_hash;

    if (!hash_password(password, user.salt, candidate_hash) ||
        !hashes_equal(candidate_hash, user.hash)) {
        ++user.failed_attempts;

        if (user.failed_attempts >= MAX_FAILED_ATTEMPTS) {
            user.locked_until = now + LOCKOUT_DURATION;
        }

        return invalid_credentials();
    }

    user.failed_attempts = 0;
    user.locked_until = Clock::time_point{};

    return {
        true,
        user.user_id,
        "Authentication successful",
        user.role
    };
}

bool AuthService::assign_role(const std::string& username, Role role) {
    if (!valid_role(role)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(users_mutex);

    const auto it = users.find(username);

    if (it == users.end()) {
        return false;
    }

    it->second.role = role;
    return true;
}

SessionResult AuthService::create_session(const AuthResult& auth_result) {
    if (!auth_result.success || auth_result.user_id.empty()) {
        return {
            false, "", "", "Authentication required", Role::Patient
        };
    }

    std::string token = generate_session_token();

    if (token.empty()) {
        return {
            false, "", "", "Unable to create session", Role::Patient
        };
    }

    const auto now = time_provider_();

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    // A collision is extraordinarily unlikely, but never overwrite a session.
    if (sessions_.find(token) != sessions_.end()) {
        return {
            false, "", "", "Unable to create session", Role::Patient
        };
    }

    sessions_.emplace(token, Session{
        auth_result.user_id,
        auth_result.role,
        now
    });

    return {
        true,
        token,
        auth_result.user_id,
        "Session created",
        auth_result.role
    };
}

SessionValidationResult AuthService::validate_session(
    const std::string& token) {
    if (token.empty()) {
        return {false, "", Role::Patient};
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    const auto it = sessions_.find(token);

    if (it == sessions_.end()) {
        return {false, "", Role::Patient};
    }

    const auto now = time_provider_();

    // Expire at the exact 10-minute inactivity boundary.
    if (now - it->second.last_activity >= SESSION_TIMEOUT) {
        sessions_.erase(it);
        return {false, "", Role::Patient};
    }

    // A valid request counts as activity and refreshes the idle timer.
    it->second.last_activity = now;

    return {
        true,
        it->second.user_id,
        it->second.role
    };
}

bool AuthService::logout(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.erase(token) == 1;
}

} // namespace hms
