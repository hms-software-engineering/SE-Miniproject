#include "services/auth_service.h"
#include "api/audit_log_handler.h"
#include "api/user_management_handler.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#define CHECK(condition)                                                     \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << "FAILED: " #condition " (line " << __LINE__      \
                      << ")\n";                                               \
            std::exit(1);                                                     \
        }                                                                     \
    } while (false)

namespace {

class InMemoryAuthRepository final : public hms::AuthRepository {
public:
    hms::CreateUserStatus create_user(
        const std::string& username,
        const std::string& password_hash,
        hms::Role role,
        std::string& user_id) override
    {
        if (users_.find(username) != users_.end()) {
            return hms::CreateUserStatus::AlreadyExists;
        }

        user_id = std::to_string(next_id_++);
        users_.emplace(
            username,
            hms::AuthUser{user_id, username, password_hash, role, true});

        return hms::CreateUserStatus::Created;
    }

    hms::DeactivateUserStatus deactivate_user(
        const std::string& actor_user_id,
        const std::string& user_id,
        const hms::AuditEvent& audit_event) override
    {
        hms::AuthUser* actor = nullptr;
        std::size_t active_admins = 0;
        for (auto& entry : users_) {
            if (entry.second.user_id == actor_user_id) actor = &entry.second;
            if (entry.second.role == hms::Role::Administrator &&
                entry.second.active) ++active_admins;
        }
        if (actor == nullptr || !actor->active ||
            actor->role != hms::Role::Administrator)
            return hms::DeactivateUserStatus::ActorNotAuthorized;
        if (actor_user_id == user_id)
            return hms::DeactivateUserStatus::SelfDeactivation;
        for (auto& entry : users_) {
            if (entry.second.user_id == user_id) {
                if (entry.second.role == hms::Role::Administrator &&
                    entry.second.active && active_admins <= 1)
                    return hms::DeactivateUserStatus::LastAdministrator;
                entry.second.active = false;
                audit_events.push_back(audit_event);
                return hms::DeactivateUserStatus::Deactivated;
            }
        }
        return hms::DeactivateUserStatus::NotFound;
    }

    std::optional<hms::AuthUser> find_user(
        const std::string& username) const override
    {
        const auto found = users_.find(username);

        if (found == users_.end()) {
            return std::nullopt;
        }

        hms::AuthUser user = found->second;
        user.account_locked = locked_users_.find(username) !=
                              locked_users_.end();
        return user;
    }

    bool record_failed_login(const std::string& username) override
    {
        const auto found = users_.find(username);
        if (found == users_.end() || !found->second.active) {
            return false;
        }
        const std::size_t failures = ++failed_attempts_[username];
        if (failures >= 5) {
            locked_users_.insert(username);
            return true;
        }
        return false;
    }

    void reset_failed_logins(const std::string& username) override
    {
        failed_attempts_.erase(username);
        locked_users_.erase(username);
    }

    void update_password_hash(const std::string& username,
                              const std::string& password_hash) override
    {
        users_.at(username).password_hash = password_hash;
    }

    void create_session(const std::string& token_hash,
                        const std::string& user_id) override
    {
        sessions_[token_hash] = user_id;
        if (expire_next_session_) {
            expired_sessions_.insert(token_hash);
            expire_next_session_ = false;
        }
    }

    std::optional<hms::SessionPrincipal> find_session(
        const std::string& token_hash) const override
    {
        if (auth_lookup_fails) throw std::runtime_error("auth database detail");
        if (expired_sessions_.find(token_hash) != expired_sessions_.end()) {
            return std::nullopt;
        }
        const auto found = sessions_.find(token_hash);

        if (found == sessions_.end()) {
            return std::nullopt;
        }

        for (const auto& entry : users_) {
            if (entry.second.user_id == found->second &&
                entry.second.active) {
                return hms::SessionPrincipal{
                    entry.second.user_id, entry.second.role};
            }
        }

        return std::nullopt;
    }

    void delete_session(const std::string& token_hash) override
    {
        sessions_.erase(token_hash);
    }

    void record_audit_event(const hms::AuditEvent& event) override
    {
        audit_events.push_back(event);
        audit_records.push_back({event.user_id(), event.role(), event.action(),
                                 event.record_id(),
                                 "2026-10-29T00:00:00.000000Z",
                                 event.source_ip()});
    }

    std::vector<hms::AuditRecord> read_audit_events(
        std::optional<hms::AuditAction> action,
        std::optional<hms::AuditRecordId> record_id) const override
    {
        if (audit_read_fails) throw std::runtime_error("database detail");
        std::vector<hms::AuditRecord> result;
        for (const auto& record : audit_records) {
            if (action && record.action != *action) continue;
            if (record_id && (!record.record_id ||
                              record.record_id->value() != record_id->value()))
                continue;
            result.push_back(record);
        }
        return result;
    }

    std::vector<hms::AuditEvent> audit_events;
    std::vector<hms::AuditRecord> audit_records;
    bool audit_read_fails = false;
    bool auth_lookup_fails = false;

    std::size_t session_count() const
    {
        return sessions_.size();
    }

    void expire_next_session() { expire_next_session_ = true; }

    std::string stored_password_hash(const std::string& username) const
    {
        return users_.at(username).password_hash;
    }

    void set_password_hash(const std::string& username,
                           const std::string& password_hash)
    {
        users_.at(username).password_hash = password_hash;
    }

private:
    std::size_t next_id_ = 1;
    std::unordered_map<std::string, hms::AuthUser> users_;
    std::unordered_map<std::string, std::string> sessions_;
    std::set<std::string> expired_sessions_;
    bool expire_next_session_ = false;
    std::unordered_map<std::string, std::size_t> failed_attempts_;
    std::set<std::string> locked_users_;
};

void test_role_permissions()
{
    using hms::Permission;
    using hms::Role;

    const Role roles[] = {
        Role::Patient,
        Role::Receptionist,
        Role::Doctor,
        Role::BillingOfficer,
        Role::Administrator
    };

    for (Role role : roles) {
        CHECK(hms::is_permitted(role, Permission::LoginLogout));
    }

    constexpr std::array<Permission, 16> permissions = {
        Permission::RegisterPatient,
        Permission::CreateOwnPatientProfile,
        Permission::SearchPatients,
        Permission::ViewPatientRecord,
        Permission::UpdatePatientData,
        Permission::ViewFreeSlots,
        Permission::BookAppointment,
        Permission::RescheduleCancelAppointment,
        Permission::ViewOwnSchedule,
        Permission::RecordVisitPrescribe,
        Permission::ViewPatientHistory,
        Permission::ViewOwnHistoryBills,
        Permission::BillingPaymentsReceipts,
        Permission::GenerateReports,
        Permission::ManageUsers,
        Permission::ViewAuditLog
    };

    // Columns: Patient, Receptionist, Doctor, Billing Officer, Administrator.
    constexpr bool expected[16][5] = {
        {false, true,  false, false, false}, // RegisterPatient
        {true,  false, false, false, false}, // CreateOwnPatientProfile
        {false, true,  true,  false, false}, // SearchPatients
        {true,  true,  true,  false, false}, // ViewPatientRecord
        {false, true,  false, false, false}, // UpdatePatientData
        {true,  true,  false, false, false}, // ViewFreeSlots
        {true,  true,  false, false, false}, // BookAppointment
        {true,  true,  false, false, false}, // RescheduleCancelAppointment
        {false, false, true,  false, false}, // ViewOwnSchedule
        {false, false, true,  false, false}, // RecordVisitPrescribe
        {false, true,  true,  false, false}, // ViewPatientHistory
        {true,  false, false, false, false}, // ViewOwnHistoryBills
        {false, false, false, true,  false}, // BillingPaymentsReceipts
        {false, false, false, true,  true }, // GenerateReports
        {false, false, false, false, true }, // ManageUsers
        {false, false, false, false, true }  // ViewAuditLog
    };

    for (std::size_t permission = 0;
         permission < permissions.size(); ++permission) {
        for (std::size_t role = 0; role < std::size(roles); ++role) {
            CHECK(
                hms::is_permitted(roles[role], permissions[permission]) ==
                expected[permission][role]);
        }
    }

    CHECK(hms::is_permitted(
        Role::Receptionist, Permission::RegisterPatient));
    CHECK(!hms::is_permitted(
        Role::Patient, Permission::RegisterPatient));
    CHECK(!hms::is_permitted(
        Role::Doctor, Permission::RegisterPatient));
    CHECK(!hms::is_permitted(
        Role::BillingOfficer, Permission::RegisterPatient));
    CHECK(!hms::is_permitted(
        Role::Administrator, Permission::RegisterPatient));

    CHECK(hms::is_permitted(
        Role::Receptionist, Permission::SearchPatients));
    CHECK(hms::is_permitted(
        Role::Doctor, Permission::SearchPatients));
    CHECK(!hms::is_permitted(
        Role::Patient, Permission::SearchPatients));
    CHECK(!hms::is_permitted(
        Role::BillingOfficer, Permission::SearchPatients));
    CHECK(!hms::is_permitted(
        Role::Administrator, Permission::SearchPatients));

    CHECK(hms::is_permitted(
        Role::Patient, Permission::ViewPatientRecord));
    CHECK(hms::is_permitted(
        Role::Receptionist, Permission::ViewPatientRecord));
    CHECK(hms::is_permitted(
        Role::Doctor, Permission::ViewPatientRecord));
    CHECK(!hms::is_permitted(
        Role::BillingOfficer, Permission::ViewPatientRecord));
    CHECK(!hms::is_permitted(
        Role::Administrator, Permission::ViewPatientRecord));

    CHECK(hms::is_permitted(Role::Patient, Permission::ViewFreeSlots));
    CHECK(hms::is_permitted(Role::Receptionist, Permission::ViewFreeSlots));
    CHECK(!hms::is_permitted(Role::Doctor, Permission::ViewFreeSlots));
    CHECK(!hms::is_permitted(Role::BillingOfficer, Permission::ViewFreeSlots));
    CHECK(!hms::is_permitted(Role::Administrator, Permission::ViewFreeSlots));

    CHECK(hms::is_permitted(Role::Patient, Permission::BookAppointment));
    CHECK(hms::is_permitted(Role::Receptionist, Permission::BookAppointment));
    CHECK(!hms::is_permitted(Role::Doctor, Permission::BookAppointment));
    CHECK(!hms::is_permitted(
        Role::BillingOfficer, Permission::BookAppointment));
    CHECK(!hms::is_permitted(
        Role::Administrator, Permission::BookAppointment));

    CHECK(hms::is_permitted(Role::Doctor, Permission::ViewOwnSchedule));
    CHECK(hms::is_permitted(Role::Doctor, Permission::RecordVisitPrescribe));
    CHECK(hms::is_permitted(
        Role::Receptionist, Permission::ViewPatientHistory));
    CHECK(hms::is_permitted(Role::Doctor, Permission::ViewPatientHistory));
    CHECK(hms::is_permitted(Role::Patient, Permission::ViewOwnHistoryBills));

    CHECK(hms::is_permitted(
        Role::BillingOfficer, Permission::BillingPaymentsReceipts));
    CHECK(hms::is_permitted(
        Role::BillingOfficer, Permission::GenerateReports));
    CHECK(hms::is_permitted(
        Role::Administrator, Permission::GenerateReports));
    CHECK(hms::is_permitted(Role::Administrator, Permission::ManageUsers));
    CHECK(hms::is_permitted(Role::Administrator, Permission::ViewAuditLog));

    std::string message;

    CHECK(!hms::authorize(
        Role::Receptionist, Permission::ManageUsers, message));
    CHECK(message == "Access denied");
    CHECK(hms::authorize(
        Role::Administrator, Permission::ManageUsers, message));
    CHECK(message.empty());

    CHECK(!hms::authorize(
        Role::Patient, Permission::RegisterPatient, message));
    CHECK(message == "Access denied");

    CHECK(hms::authorize(
        Role::Receptionist, Permission::RegisterPatient, message));
    CHECK(message.empty());

    // Patients may access their own records.
    CHECK(hms::authorize_record_access(
        Role::Patient,
        Permission::ViewPatientRecord,
        "patient-1",
        "patient-1",
        message));
    CHECK(message.empty());

    // Patients must not access another patient's record.
    CHECK(!hms::authorize_record_access(
        Role::Patient,
        Permission::ViewPatientRecord,
        "patient-1",
        "patient-2",
        message));
    CHECK(message == "Access denied");

    // Missing owner information must fail closed for patients.
    CHECK(!hms::authorize_record_access(
        Role::Patient,
        Permission::ViewPatientRecord,
        "patient-1",
        "",
        message));
    CHECK(message == "Access denied");

    // Missing requester identity must also fail closed.
    CHECK(!hms::authorize_record_access(
        Role::Patient,
        Permission::ViewPatientRecord,
        "",
        "patient-1",
        message));
    CHECK(message == "Access denied");

    // Preserve existing appointment ownership checks.
    CHECK(hms::authorize_record_access(
        Role::Patient,
        Permission::RescheduleCancelAppointment,
        "patient-1",
        "patient-1",
        message));

    CHECK(!hms::authorize_record_access(
        Role::Patient,
        Permission::RescheduleCancelAppointment,
        "patient-1",
        "patient-2",
        message));
    CHECK(message == "Access denied");

    // Receptionists are allowed to manage appointment records for patients.
    CHECK(hms::authorize_record_access(
        Role::Receptionist,
        Permission::RescheduleCancelAppointment,
        "receptionist-1",
        "patient-2",
        message));

    Role parsed = Role::Patient;
    CHECK(hms::parse_role("Billing Officer", parsed));
    CHECK(parsed == Role::BillingOfficer);
    CHECK(!hms::parse_role("Root", parsed));
    CHECK(!hms::is_permitted(
        static_cast<Role>(999), Permission::LoginLogout));
}

void test_authentication()
{
    InMemoryAuthRepository repository;
    hms::AuthService service(repository);

    auto weak = service.register_patient("patient", "weak");
    CHECK(!weak.success);
    CHECK(repository.find_user("patient") == std::nullopt);

    auto registered =
        service.register_patient("patient", "GoodPass1!");
    CHECK(registered.success);
    CHECK(registered.role == hms::Role::Patient);
    CHECK(registered.token.empty());

    const std::string encoded = repository.stored_password_hash("patient");
    CHECK(encoded.find("$argon2id$v=19$m=65536,t=3,p=1$") == 0);
    CHECK(encoded != "GoodPass1!");

    const auto legacy_user =
        service.register_patient("legacy", "GoodPass1!");
    CHECK(legacy_user.success);
    repository.set_password_hash(
        "legacy",
        "pbkdf2-sha256$600000$000102030405060708090a0b0c0d0e0f$"
        "dc359d39624353945baa85dbde4fe47d7ea9b9db0f9639ce53e1f6fbfde4dbe3");
    const auto upgraded_login =
        service.authenticate_user("legacy", "GoodPass1!");
    CHECK(upgraded_login.success);
    CHECK(repository.stored_password_hash("legacy").find("$argon2id$") == 0);
    service.logout(upgraded_login.token);

    auto duplicate =
        service.register_patient("patient", "GoodPass1!");
    CHECK(!duplicate.success);
    CHECK(duplicate.message == "Username already exists.");

    auto invalid =
        service.authenticate_user("patient", "WrongPass1!");
    CHECK(!invalid.success);
    CHECK(invalid.message == "Invalid username or password.");

    CHECK(invalid.user_id == registered.user_id);
    CHECK(invalid.role == hms::Role::Patient);
    auto unknown = service.authenticate_user("unknown", "WrongPass1!");
    CHECK(!unknown.success);
    CHECK(unknown.message == invalid.message);
    CHECK(unknown.user_id.empty());

    for (int attempt = 0; attempt < 3; ++attempt) {
        auto failed = service.authenticate_user("patient", "WrongPass1!");
        CHECK(!failed.success);
        CHECK(failed.message == "Invalid username or password.");
    }
    auto fifth_failure =
        service.authenticate_user("patient", "WrongPass1!");
    CHECK(!fifth_failure.success);
    CHECK(fifth_failure.account_locked);
    CHECK(fifth_failure.message == "Account locked.");
    auto locked_login =
        service.authenticate_user("patient", "GoodPass1!");
    CHECK(!locked_login.success);
    CHECK(locked_login.account_locked);

    repository.reset_failed_logins("patient");
    auto login = service.authenticate_user("patient", "GoodPass1!");
    CHECK(login.success);
    CHECK(login.user_id == registered.user_id);
    CHECK(login.role == hms::Role::Patient);
    CHECK(login.token.size() == 64);
    CHECK(repository.session_count() == 1);

    const hms::AuditEvent audit_event{
        login.user_id, "Patient", "LOGIN", login.user_id, "127.0.0.1"
    };
    service.record_audit_event(audit_event);
    CHECK(repository.audit_events.size() == 1);
    CHECK(repository.audit_events.front().user_id() ==
          hms::parse_audit_user_id(login.user_id));
    CHECK(repository.audit_events.front().role() == hms::AuditRole::Patient);
    CHECK(repository.audit_events.front().action() == hms::AuditAction::Login);
    CHECK(repository.audit_events.front().record_id()->value() == login.user_id);
    CHECK(repository.audit_events.front().source_ip() == "127.0.0.1");

    auto principal = service.authenticate_token(login.token);
    CHECK(principal.has_value());
    CHECK(principal->user_id == login.user_id);
    CHECK(principal->role == hms::Role::Patient);
    CHECK(!service.authenticate_token("invalid").has_value());

    service.logout(login.token);
    CHECK(repository.session_count() == 0);
    CHECK(!service.authenticate_token(login.token).has_value());
}

void test_postgres_authentication_when_enabled()
{
    const char* f060_connection_info =
        std::getenv("HMS_F060_TEST_DATABASE_URL");
    const bool f060_integration_enabled =
        f060_connection_info != nullptr &&
        !std::string(f060_connection_info).empty();
    const char* coverage_enabled = std::getenv("HMS_AUTH_TEST_POSTGRES");
    if (!f060_integration_enabled &&
        (coverage_enabled == nullptr || std::string(coverage_enabled) != "1")) {
        return;
    }
    if (f060_integration_enabled) {
        CHECK(std::string(f060_connection_info).find("hms_f060_test") !=
              std::string::npos);
    }
    const char* connection_info =
        f060_integration_enabled ? f060_connection_info : nullptr;

    hms::PostgresAuthRepository repository(connection_info);
    hms::AuthService service(repository);
    CHECK(repository.health_check());

    std::string doctor_id;
    const auto doctor_created = repository.create_user(
        "coverage_doctor_" + std::to_string(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch().count()),
        "test-hash", hms::Role::Doctor, doctor_id);
    CHECK(doctor_created == hms::CreateUserStatus::Created);
    CHECK(!doctor_id.empty());

    const auto stamp =
        std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count();

    const std::string username =
        "auth_test_" + std::to_string(stamp);

    const auto registered =
        service.register_patient(username, "GoodPass1!");
    CHECK(registered.success);

    const auto login =
        service.authenticate_user(username, "GoodPass1!");
    CHECK(login.success);
    CHECK(login.role == hms::Role::Patient);

    service.record_audit_event(
        {registered.user_id, "Patient", "LOGIN", registered.user_id,
         "127.0.0.1"});
    const auto failed_login =
        service.authenticate_user(username, "WrongPass1!");
    CHECK(!failed_login.success);
    const auto login_again =
        service.authenticate_user(username, "GoodPass1!");
    CHECK(login_again.success);

    const auto principal = service.authenticate_token(login_again.token);
    CHECK(principal.has_value());
    CHECK(principal->user_id == registered.user_id);

    const auto admin = service.create_user(
        username + "_admin", "GoodPass1!", hms::Role::Administrator);
    CHECK(admin.success);
    const hms::SessionPrincipal actor{admin.user_id, hms::Role::Administrator};
    bool audit_failure_rolled_back = false;
    try {
        (void)service.deactivate_user(actor, registered.user_id,
                                      "not-an-ip-address");
    } catch (const std::exception&) {
        audit_failure_rolled_back = true;
    }
    CHECK(audit_failure_rolled_back);
    CHECK(service.authenticate_token(login_again.token).has_value());
    CHECK(service.authenticate_user(username, "GoodPass1!").success);
    CHECK(service.deactivate_user(actor, registered.user_id, "127.0.0.1") ==
          hms::DeactivateUserStatus::Deactivated);
    const auto refused_login =
        service.authenticate_user(username, "GoodPass1!");
    CHECK(!refused_login.success);
    CHECK(refused_login.message == "Invalid username or password.");
    CHECK(!service.authenticate_token(login_again.token).has_value());

    service.logout(login_again.token);
    CHECK(!service.authenticate_token(login_again.token).has_value());

    const auto second_admin = service.create_user(
        username + "_admin2", "GoodPass1!", hms::Role::Administrator);
    CHECK(second_admin.success);
    const auto first_admin_login = service.authenticate_user(
        username + "_admin", "GoodPass1!");
    const auto second_admin_login = service.authenticate_user(
        username + "_admin2", "GoodPass1!");
    CHECK(first_admin_login.success && second_admin_login.success);

    const auto persisted_audit_read = hms::handle_audit_log(
        "GET", "/api/v1/audit-log",
        "action=LOGIN&recordId=" + registered.user_id,
        "Bearer " + first_admin_login.token, "127.0.0.1",
        "req-postgres-audit", service);
    CHECK(persisted_audit_read.status == 200);
    CHECK(persisted_audit_read.body.find(registered.user_id) !=
          std::string::npos);
    CHECK(persisted_audit_read.body.find("LOGIN") != std::string::npos);
    CHECK(persisted_audit_read.body.find("127.0.0.1") !=
          std::string::npos);
    const auto persisted_json = nlohmann::json::parse(
        persisted_audit_read.body);
    CHECK(persisted_json.at("entries").size() == 1);
    const auto& persisted_entry = persisted_json.at("entries").front();
    CHECK(persisted_entry.size() == 6);
    CHECK(persisted_entry.at("userId") == std::stoull(registered.user_id));
    CHECK(persisted_entry.at("role") == "Patient");
    CHECK(persisted_entry.at("action") == "LOGIN");
    CHECK(persisted_entry.at("recordId") == registered.user_id);
    CHECK(persisted_entry.at("sourceIp") == "127.0.0.1");
    CHECK(persisted_entry.at("timestamp").get<std::string>().find('Z') !=
          std::string::npos);

    const auto stored_before_rejection = hms::handle_audit_log(
        "GET", "/api/v1/audit-log",
        "action=LOGIN&recordId=" + registered.user_id,
        "Bearer " + first_admin_login.token, "127.0.0.1",
        "req-postgres-audit-before", service);
    CHECK(stored_before_rejection.status == 200);
    for (const std::string& synthetic_sensitive_value : {
             "GoodPass1!", "$argon2id$v=19$m=65536$synthetic-hash",
             "synthetic-session-token", "clinical note: synthetic symptom"}) {
        bool rejected = false;
        try {
            (void)hms::AuditEvent{registered.user_id, "Patient", "LOGIN",
                                  synthetic_sensitive_value, "127.0.0.1"};
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        CHECK(rejected);
    }
    const auto stored_after_rejection = hms::handle_audit_log(
        "GET", "/api/v1/audit-log",
        "action=LOGIN&recordId=" + registered.user_id,
        "Bearer " + first_admin_login.token, "127.0.0.1",
        "req-postgres-audit-after", service);
    CHECK(stored_after_rejection.status == 200);
    CHECK(stored_after_rejection.body == stored_before_rejection.body);

    hms::PostgresAuthRepository second_repository(connection_info);
    hms::AuthService second_service(second_repository);
    hms::DeactivateUserStatus first_status =
        hms::DeactivateUserStatus::NotFound;
    hms::DeactivateUserStatus second_status =
        hms::DeactivateUserStatus::NotFound;
    std::thread first_request([&] {
        first_status = service.deactivate_user(
            hms::SessionPrincipal{first_admin_login.user_id,
                                  hms::Role::Administrator},
            second_admin.user_id, "127.0.0.1");
    });
    std::thread second_request([&] {
        second_status = second_service.deactivate_user(
            hms::SessionPrincipal{second_admin_login.user_id,
                                  hms::Role::Administrator},
            admin.user_id, "127.0.0.1");
    });
    first_request.join();
    second_request.join();
    CHECK((first_status == hms::DeactivateUserStatus::Deactivated) !=
          (second_status == hms::DeactivateUserStatus::Deactivated));
    const bool first_remains_active =
        service.authenticate_token(first_admin_login.token).has_value();
    const bool second_remains_active =
        second_service.authenticate_token(second_admin_login.token).has_value();
    CHECK(first_remains_active != second_remains_active);
}

void test_account_deactivation()
{
    InMemoryAuthRepository repository;
    hms::AuthService service(repository);

    const auto created = service.create_user(
        "nurse1", "GoodPass1!", hms::Role::Receptionist);
    CHECK(created.success);
    CHECK(created.role == hms::Role::Receptionist);

    const auto login = service.authenticate_user("nurse1", "GoodPass1!");
    CHECK(login.success);
    CHECK(service.authenticate_token(login.token).has_value());

    const auto admin = service.create_user(
        "nurse-admin", "GoodPass1!", hms::Role::Administrator);
    CHECK(admin.success);
    const hms::SessionPrincipal actor{admin.user_id, hms::Role::Administrator};
    CHECK(service.deactivate_user(actor, created.user_id, "127.0.0.1") ==
          hms::DeactivateUserStatus::Deactivated);
    const auto stored_user = repository.find_user("nurse1");
    CHECK(stored_user.has_value());
    CHECK(!stored_user->active);

    const auto refused_login =
        service.authenticate_user("nurse1", "GoodPass1!");
    CHECK(!refused_login.success);
    CHECK(refused_login.message == "Invalid username or password.");
    CHECK(!service.authenticate_token(login.token).has_value());
    CHECK(repository.audit_events.back().action() == hms::AuditAction::DeactivateUser);
    CHECK(repository.audit_events.back().user_id() ==
          hms::parse_audit_user_id(admin.user_id));
    CHECK(repository.audit_events.back().record_id()->value() == created.user_id);

    CHECK(service.deactivate_user(actor, "999999", "127.0.0.1") ==
          hms::DeactivateUserStatus::NotFound);
}

void test_user_deactivation_endpoint()
{
    InMemoryAuthRepository repository;
    hms::AuthService service(repository);
    const auto admin = service.create_user(
        "endpoint-admin", "GoodPass1!", hms::Role::Administrator);
    const auto target = service.create_user(
        "endpoint-target", "GoodPass1!", hms::Role::Patient);
    const auto receptionist = service.create_user(
        "endpoint-reception", "GoodPass1!", hms::Role::Receptionist);
    CHECK(admin.success && target.success && receptionist.success);
    const auto admin_login = service.authenticate_user(
        "endpoint-admin", "GoodPass1!");
    const auto receptionist_login = service.authenticate_user(
        "endpoint-reception", "GoodPass1!");
    const auto target_login = service.authenticate_user(
        "endpoint-target", "GoodPass1!");
    CHECK(target_login.success);
    const std::string path = "/api/v1/users/" + target.user_id;
    const auto request = [&](const std::string& token, const std::string& p,
                             const std::string& body) {
        return hms::handle_user_deactivation(
            "PATCH", p, token.empty() ? "" : "Bearer " + token,
            "application/json", body, true, "127.0.0.1", "req-1", service);
    };
    CHECK(request("", path, R"({"action":"deactivate"})").status == 401);
    CHECK(request("invalid-token", path,
                  R"({"action":"deactivate"})").status == 401);
    const auto non_admin_response = request(
        receptionist_login.token, path, R"({"action":"deactivate"})");
    CHECK(non_admin_response.status == 403);
    CHECK(non_admin_response.body.find("\"code\":\"ACCESS_DENIED\"") !=
          std::string::npos);
    CHECK(request(admin_login.token, "/api/v1/users/0",
                  R"({"action":"deactivate"})").status == 400);
    CHECK(request(admin_login.token, "/api/v1/users/1x",
                  R"({"action":"deactivate"})").status == 400);
    CHECK(request(admin_login.token, path, "{").status == 400);
    CHECK(request(admin_login.token, path,
                  R"({"action":"delete"})").status == 400);
    CHECK(request(admin_login.token, path,
                  R"({"action":"deactivate","extra":true})").status == 400);
    CHECK(hms::handle_user_deactivation(
              "PATCH", path, "Bearer " + admin_login.token,
              "text/plain", R"({"action":"deactivate"})", true,
              "127.0.0.1", "req-1", service).status == 415);
    CHECK(hms::handle_user_deactivation(
              "PATCH", path, "Bearer " + admin_login.token,
              "application/json", R"({"action":"deactivate"})", false,
              "127.0.0.1", "req-1", service).status == 400);
    const auto success_response = request(
        admin_login.token, path, R"({"action":"deactivate"})");
    CHECK(success_response.status == 200);
    CHECK(success_response.body.find("\"active\":false") !=
          std::string::npos);
    CHECK(success_response.body.find(target.user_id) != std::string::npos);
    CHECK(!service.authenticate_token(target_login.token).has_value());
    CHECK(!service.authenticate_user("endpoint-target", "GoodPass1!").success);
    CHECK(request(admin_login.token, "/api/v1/users/" + admin.user_id,
                  R"({"action":"deactivate"})").status == 409);
    CHECK(repository.audit_events.back().user_id() ==
          hms::parse_audit_user_id(admin.user_id));
    CHECK(repository.audit_events.back().action() == hms::AuditAction::DeactivateUser);
    CHECK(repository.audit_events.back().record_id()->value() == target.user_id);
    CHECK(repository.audit_events.back().source_ip() == "127.0.0.1");
}

void test_audit_log_endpoint_security()
{
    InMemoryAuthRepository repository;
    hms::AuthService service(repository);
    const auto create_and_login = [&](const std::string& username,
                                      hms::Role role) {
        const auto created = service.create_user(username, "GoodPass1!", role);
        CHECK(created.success);
        const auto login = service.authenticate_user(username, "GoodPass1!");
        CHECK(login.success);
        return login;
    };
    const auto patient = create_and_login("audit_patient", hms::Role::Patient);
    const auto receptionist = create_and_login(
        "audit_reception", hms::Role::Receptionist);
    const auto doctor = create_and_login("audit_doctor", hms::Role::Doctor);
    const auto billing = create_and_login(
        "audit_billing", hms::Role::BillingOfficer);
    const auto admin = create_and_login(
        "audit_admin", hms::Role::Administrator);
    const auto expired_user = service.create_user(
        "audit_expired", "GoodPass1!", hms::Role::Administrator);
    CHECK(expired_user.success);
    repository.expire_next_session();
    const auto expired = service.authenticate_user(
        "audit_expired", "GoodPass1!");
    CHECK(expired.success);

    repository.record_audit_event(hms::AuditEvent{
        admin.user_id, "Administrator", "CREATE_USER", "42", "127.0.0.1"});
    repository.record_audit_event(hms::AuditEvent{
        doctor.user_id, "Doctor", "VIEW_PATIENT_HISTORY", "PID-00000001",
        "192.0.2.20"});

    const auto request = [&](const std::string& token,
                             const std::string& query = std::string()) {
        return hms::handle_audit_log(
            "GET", "/api/v1/audit-log", query,
            token.empty() ? "" : "Bearer " + token, "127.0.0.1",
            "req-audit", service);
    };
    CHECK(request("").status == 401);
    CHECK(request("not-a-token").status == 401);
    CHECK(request(std::string(64, 'a')).status == 401);
    CHECK(request(expired.token).status == 401);
    for (const auto* token : {&patient.token, &receptionist.token,
                              &doctor.token, &billing.token}) {
        CHECK(request(*token).status == 403);
        CHECK(request(*token, "role=Administrator").status == 403);
        CHECK(request(*token, "action=VIEW_PATIENT_HISTORY").status == 403);
        const auto direct_read = service.read_audit_events(*token);
        CHECK(direct_read.status == hms::AuditReadStatus::AccessDenied);
        CHECK(direct_read.records.empty());
    }
    CHECK(service.read_audit_events(std::string(64, 'b')).status ==
          hms::AuditReadStatus::AuthenticationRequired);

    const auto all_records = request(admin.token);
    CHECK(all_records.status == 200);
    CHECK(all_records.body.find("\"userId\"") != std::string::npos);
    CHECK(all_records.body.find("\"role\"") != std::string::npos);
    CHECK(all_records.body.find("\"action\"") != std::string::npos);
    CHECK(all_records.body.find("\"recordId\"") != std::string::npos);
    CHECK(all_records.body.find("\"timestamp\"") != std::string::npos);
    CHECK(all_records.body.find("\"sourceIp\"") != std::string::npos);
    CHECK(all_records.body.find("2026-10-29T00:00:00.000000Z") !=
          std::string::npos);
    CHECK(all_records.body.find("192.0.2.20") != std::string::npos);
    CHECK(all_records.body.find("password") == std::string::npos);
    CHECK(all_records.body.find("symptoms") == std::string::npos);
    CHECK(all_records.body.find("diagnosis") == std::string::npos);
    CHECK(all_records.body.find("notes") == std::string::npos);
    const auto serialized = nlohmann::json::parse(all_records.body);
    CHECK(serialized.size() == 1);
    CHECK(serialized.at("entries").size() == 2);
    const std::set<std::string> allowed_fields = {
        "userId", "role", "action", "recordId", "timestamp", "sourceIp"};
    for (const auto& entry : serialized.at("entries")) {
        CHECK(entry.size() == allowed_fields.size());
        for (const auto& field : entry.items())
            CHECK(allowed_fields.find(field.key()) != allowed_fields.end());
    }

    const auto filtered = request(admin.token,
        "action=VIEW_PATIENT_HISTORY&recordId=PID-00000001");
    CHECK(filtered.status == 200);
    CHECK(filtered.body.find("PID-00000001") != std::string::npos);
    CHECK(filtered.body.find("VIEW_PATIENT_HISTORY") != std::string::npos);
    CHECK(filtered.body.find("CREATE_USER") == std::string::npos);
    CHECK(request(admin.token, "action=LOGIN_FAILED").status == 200);
    CHECK(request(admin.token, "action=not-supported").status == 400);
    CHECK(request(admin.token, "action=LOGIN&").status == 400);
    CHECK(request(admin.token, "recordId=1%27%3Bdrop%20table").status == 400);
    CHECK(request(admin.token, "action=LOGIN&action=LOGOUT").status == 400);
    CHECK(request(admin.token, "role=Administrator").status == 400);
    CHECK(hms::handle_audit_log("POST", "/api/v1/audit-log", "", "",
                                "127.0.0.1", "req-audit", service).status == 405);
    CHECK(hms::handle_audit_log("GET", "/api/v1/audit-log/42", "",
                                "Bearer " + admin.token, "127.0.0.1",
                                "req-audit", service).status == 404);
    repository.audit_read_fails = true;
    const auto database_error = request(admin.token);
    CHECK(database_error.status == 500);
    CHECK(database_error.body.find("database detail") == std::string::npos);
    repository.audit_read_fails = false;
    repository.auth_lookup_fails = true;
    const auto authentication_database_error = request(admin.token);
    CHECK(authentication_database_error.status == 500);
    CHECK(authentication_database_error.body.find("auth database detail") ==
          std::string::npos);
    repository.auth_lookup_fails = false;

    InMemoryAuthRepository empty_repository;
    hms::AuthService empty_service(empty_repository);
    const auto empty_admin = empty_service.create_user(
        "empty_audit_admin", "GoodPass1!", hms::Role::Administrator);
    CHECK(empty_admin.success);
    const auto empty_login = empty_service.authenticate_user(
        "empty_audit_admin", "GoodPass1!");
    CHECK(empty_login.success);
    const auto empty_response = hms::handle_audit_log(
        "GET", "/api/v1/audit-log", "", "Bearer " + empty_login.token,
        "127.0.0.1", "req-empty-audit", empty_service);
    CHECK(empty_response.status == 200);
    CHECK(empty_response.body == "{\"entries\":[]}");

    CHECK(hms::AuditRecordId::parse("42").has_value());
    CHECK(hms::AuditRecordId::parse("PID-00000001").has_value());
    CHECK(!hms::AuditRecordId::parse("0").has_value());
    CHECK(!hms::AuditRecordId::parse("9223372036854775808").has_value());
    for (const std::string& unsafe : {
             "password hash", "session-token", "diagnosis: flu",
             "symptoms: pain", "prescription", "medical history", "x';--"}) {
        CHECK(!hms::AuditRecordId::parse(unsafe).has_value());
    }
    for (const std::string& unsafe_action : {
             "password=hunter2", "TOKEN_ISSUED", "DIAGNOSIS", "REQUEST_BODY"}) {
        bool rejected = false;
        try {
            (void)hms::AuditEvent{"1", "Doctor", unsafe_action, "42",
                                  "127.0.0.1"};
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        CHECK(rejected);
    }
    bool secret_user_id_rejected = false;
    try {
        (void)hms::AuditEvent{"GoodPass1!", "Doctor", "LOGIN", "42",
                              "127.0.0.1"};
    } catch (const std::invalid_argument&) {
        secret_user_id_rejected = true;
    }
    CHECK(secret_user_id_rejected);
}

} // namespace

int main()
{
    test_role_permissions();
    test_authentication();
    test_account_deactivation();
    test_user_deactivation_endpoint();
    test_audit_log_endpoint_security();
    test_postgres_authentication_when_enabled();

    std::cout << "All authentication and RBAC tests passed.\n";
    return 0;
}
