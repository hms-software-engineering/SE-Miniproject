#pragma once

#include "services/rbac_service.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

enum class AuditRole {
    Unknown,
    Patient,
    Receptionist,
    Doctor,
    BillingOfficer,
    Administrator
};

enum class AuditAction {
    CreateUser,
    LoginFailed,
    Login,
    Logout,
    ViewAccount,
    ViewAuditLog,
    GenerateDailyReport,
    ViewPatientHistory,
    ViewPatient,
    CreatePatientProfile,
    ViewOwnSchedule,
    ViewFreeSlots,
    CreateAppointment,
    CancelAppointment,
    RescheduleAppointment,
    CreateVisit,
    CreatePrescription,
    CreatePatient,
    UpdatePatient,
    DeactivateUser,
    CreateBill,
    RecordPayment,
    ViewReceipt,
    UpdatePatientAllergies,
    OverrideAllergyWarning,
    ViewVisitHistory,
    ViewOwnHistory,
    ViewOwnBills
};

class AuditRecordId {
public:
    static std::optional<AuditRecordId> parse(const std::string& value);
    const std::string& value() const noexcept { return value_; }

private:
    explicit AuditRecordId(std::string value) : value_(std::move(value)) {}
    std::string value_;
};

class AuditEvent {
public:
    AuditEvent(const std::string& user_id,
               const std::string& role,
               const std::string& action,
               const std::string& record_id,
               const std::string& source_ip);

    const std::optional<std::uint64_t>& user_id() const noexcept { return user_id_; }
    AuditRole role() const noexcept { return role_; }
    AuditAction action() const noexcept { return action_; }
    const std::optional<AuditRecordId>& record_id() const noexcept { return record_id_; }
    const std::string& source_ip() const noexcept { return source_ip_; }

private:
    std::optional<std::uint64_t> user_id_;
    AuditRole role_ = AuditRole::Unknown;
    AuditAction action_ = AuditAction::Login;
    std::optional<AuditRecordId> record_id_;
    std::string source_ip_;
};

struct AuditRecord {
    std::optional<std::uint64_t> user_id;
    AuditRole role = AuditRole::Unknown;
    AuditAction action = AuditAction::Login;
    std::optional<AuditRecordId> record_id;
    std::string timestamp;
    std::string source_ip;
    std::string username;
};

enum class AuditReadStatus {
    Success,
    AuthenticationRequired,
    AccessDenied
};

struct AuditReadResult {
    AuditReadStatus status = AuditReadStatus::AuthenticationRequired;
    std::vector<AuditRecord> records;
};

struct AuditFilter {
    std::optional<AuditAction> action;
    std::optional<AuditRecordId> record_id;
    std::optional<std::string> username;
    std::optional<std::string> from_date;  // YYYY-MM-DD, inclusive
    std::optional<std::string> to_date;    // YYYY-MM-DD, inclusive
};

AuditRole audit_role(Role role) noexcept;
const char* audit_role_name(AuditRole role) noexcept;
const char* audit_action_name(AuditAction action) noexcept;
std::optional<AuditRole> parse_audit_role(const std::string& value);
std::optional<AuditAction> parse_audit_action(const std::string& value);
std::optional<std::uint64_t> parse_audit_user_id(const std::string& value);
AuditEvent make_audit_event(const std::string& user_id,
                            AuditRole role,
                            AuditAction action,
                            const std::string& record_id,
                            const std::string& source_ip);

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

private:
    friend class AuthService;
    virtual std::vector<AuditRecord> read_audit_events(
        const AuditFilter& filter) const = 0;
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
    AuditReadResult read_audit_events(
        const std::string& token,
        const AuditFilter& filter = AuditFilter{}) const;

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
    std::vector<AuditRecord> read_audit_events(
        const AuditFilter& filter) const override;
    struct Connection;
    Connection* connection_;
};

} // namespace hms