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
    bool account_locked = false;
};

struct SessionPrincipal {
    std::string user_id;
    Role role = Role::Patient;
};

struct AuditEvent {
    std::string user_id;
    std::string role;
    std::string action;
    std::string record_id;
    std::string source_ip;
};

enum class CreateUserStatus {
    Created,
    AlreadyExists
};

enum class DeactivateUserStatus {
    Deactivated,
    NotFound,
    ActorNotAuthorized,
    SelfDeactivation,
    LastAdministrator
};

class AuthRepository {
public:
    virtual ~AuthRepository() = default;

    virtual CreateUserStatus create_user(const std::string& username,
                                         const std::string& password_hash,
                                         Role role,
                                         std::string& user_id) = 0;
    virtual DeactivateUserStatus deactivate_user(
        const std::string& actor_user_id,
        const std::string& user_id,
        const AuditEvent& audit_event) = 0;
    virtual std::optional<AuthUser> find_user(
        const std::string& username) const = 0;
    virtual bool record_failed_login(const std::string& username) = 0;
    virtual void reset_failed_logins(const std::string& username) = 0;
    virtual void update_password_hash(const std::string& username,
                                      const std::string& password_hash) = 0;
    virtual void create_session(const std::string& token_hash,
                                const std::string& user_id) = 0;
    virtual std::optional<SessionPrincipal> find_session(
        const std::string& token_hash) const = 0;
    virtual void delete_session(const std::string& token_hash) = 0;
    virtual void record_audit_event(const AuditEvent& event) = 0;
};

struct AuthResult {
    bool success = false;
    std::string user_id;
    std::string token;
    std::string message;
    Role role = Role::Patient;
    bool account_locked = false;
};

class AuthService {
public:
    explicit AuthService(AuthRepository& repository);

    AuthResult register_patient(const std::string& username,
                                const std::string& password);
    AuthResult create_user(const std::string& username,
                           const std::string& password,
                           Role role);
    DeactivateUserStatus deactivate_user(
        const SessionPrincipal& actor,
        const std::string& user_id,
        const std::string& source_ip);
    AuthResult authenticate_user(const std::string& username,
                                 const std::string& password);
    std::optional<SessionPrincipal> authenticate_token(
        const std::string& token) const;
    void logout(const std::string& token);
    void record_audit_event(const AuditEvent& event);

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
    DeactivateUserStatus deactivate_user(
        const std::string& actor_user_id,
        const std::string& user_id,
        const AuditEvent& audit_event) override;
    std::optional<AuthUser> find_user(
        const std::string& username) const override;
    bool record_failed_login(const std::string& username) override;
    void reset_failed_logins(const std::string& username) override;
    void update_password_hash(const std::string& username,
                              const std::string& password_hash) override;
    void create_session(const std::string& token_hash,
                        const std::string& user_id) override;
    std::optional<SessionPrincipal> find_session(
        const std::string& token_hash) const override;
    void delete_session(const std::string& token_hash) override;
    void record_audit_event(const AuditEvent& event) override;
    bool health_check() const noexcept;

private:
    struct Connection;
    Connection* connection_;
};

} // namespace hms
