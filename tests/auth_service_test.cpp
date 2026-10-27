#include "services/auth_service.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
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
    }

    std::optional<hms::SessionPrincipal> find_session(
        const std::string& token_hash) const override
    {
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
    }

    std::vector<hms::AuditEvent> audit_events;

    std::size_t session_count() const
    {
        return sessions_.size();
    }

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
    CHECK(repository.audit_events.front().user_id == login.user_id);
    CHECK(repository.audit_events.front().role == "Patient");
    CHECK(repository.audit_events.front().action == "LOGIN");
    CHECK(repository.audit_events.front().record_id == login.user_id);
    CHECK(repository.audit_events.front().source_ip == "127.0.0.1");

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
    const char* enabled = std::getenv("HMS_AUTH_TEST_POSTGRES");

    if (enabled == nullptr || std::string(enabled) != "1") {
        return;
    }

    hms::PostgresAuthRepository repository;
    hms::AuthService service(repository);

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

    const auto principal =
        service.authenticate_token(login.token);
    CHECK(principal.has_value());
    CHECK(principal->user_id == registered.user_id);

    service.logout(login.token);
    CHECK(!service.authenticate_token(login.token).has_value());
}

} // namespace

int main()
{
    test_role_permissions();
    test_authentication();
    test_postgres_authentication_when_enabled();

    std::cout << "All authentication and RBAC tests passed.\n";
    return 0;
}
