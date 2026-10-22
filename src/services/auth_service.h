#pragma once

#include "services/rbac_service.h"

#include <cstdint>
#include <optional>
#include <string>

namespace hms {

struct AuthUser {
    std::string user_id;
    std::string username;
    std::string password_hash;
    Role role = Role::Patient;
    bool active = true;
};

struct SessionPrincipal {
    std::string user_id;
    Role role = Role::Patient;
};

enum class CreateUserStatus {
    Created,
    AlreadyExists
};

class AuthRepository {
public:
    virtual ~AuthRepository() = default;

    virtual CreateUserStatus create_user(const std::string& username,
                                         const std::string& password_hash,
                                         Role role,
                                         std::string& user_id) = 0;
    virtual std::optional<AuthUser> find_user(
        const std::string& username) const = 0;
    virtual void create_session(const std::string& token_hash,
                                const std::string& user_id) = 0;
    virtual std::optional<SessionPrincipal> find_session(
        const std::string& token_hash) const = 0;
    virtual void delete_session(const std::string& token_hash) = 0;
};

struct AuthResult {
    bool success = false;
    std::string user_id;
    std::string token;
    std::string message;
    Role role = Role::Patient;
};

class AuthService {
public:
    explicit AuthService(AuthRepository& repository);

    AuthResult register_patient(const std::string& username,
                                const std::string& password);
    AuthResult create_user(const std::string& username,
                           const std::string& password,
                           Role role);
    AuthResult authenticate_user(const std::string& username,
                                 const std::string& password);
    std::optional<SessionPrincipal> authenticate_token(
        const std::string& token) const;
    void logout(const std::string& token);

private:
    AuthRepository& repository_;
};

class PostgresAuthRepository final : public AuthRepository {
public:
    explicit PostgresAuthRepository(const char* connection_info = nullptr);
    ~PostgresAuthRepository() override;

    PostgresAuthRepository(const PostgresAuthRepository&) = delete;
    PostgresAuthRepository& operator=(const PostgresAuthRepository&) = delete;

    CreateUserStatus create_user(const std::string& username,
                                 const std::string& password_hash,
                                 Role role,
                                 std::string& user_id) override;
    std::optional<AuthUser> find_user(
        const std::string& username) const override;
    void create_session(const std::string& token_hash,
                        const std::string& user_id) override;
    std::optional<SessionPrincipal> find_session(
        const std::string& token_hash) const override;
    void delete_session(const std::string& token_hash) override;

private:
    struct Connection;
    Connection* connection_;
};

} // namespace hms
