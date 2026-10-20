
#include "../src/api/api.h"
#include "../src/services/auth_service.h"
#include "../src/services/rbac_service.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

int main() {
    // =========================================================
    // 1. API error serialization tests
    // =========================================================

    hms::ApiError error{
        404,
        "NOT_FOUND",
        "The requested resource was not found",
        "req-test-001",
        ""
    };

    const std::string json = hms::make_error_json(error);

    assert(json.find("\"code\":\"NOT_FOUND\"") != std::string::npos);
    assert(json.find("\"message\":\"The requested resource was not found\"")
           != std::string::npos);
    assert(json.find("\"requestId\":\"req-test-001\"")
           != std::string::npos);
    assert(json.find("\"field\"") == std::string::npos);

    hms::ApiError field_error{
        400,
        "VALIDATION_FAILED",
        "Invalid input",
        "req-test-002",
        "name"
    };

    const std::string field_json = hms::make_error_json(field_error);
    assert(field_json.find("\"field\":\"name\"") != std::string::npos);

    hms::ApiError escaped_error{
        400,
        "VALIDATION_FAILED",
        "A \"quoted\" message",
        "req-test-003",
        ""
    };

    const std::string escaped_json =
        hms::make_error_json(escaped_error);

    assert(escaped_json.find("A \\\"quoted\\\" message")
           != std::string::npos);


    // =========================================================
    // 2. Authentication tests (HMS-F-001)
    // =========================================================

    hms::AuthService auth;

    const auto weak_password =
        auth.register_user("test_weak_user", "weak");

    assert(!weak_password.success);

    const auto registration =
        auth.register_user("test_auth_user", "StrongPass1!");

    assert(registration.success);
    assert(registration.user_id == "test_auth_user");
    assert(registration.role == hms::Role::Patient);

    const auto duplicate =
        auth.register_user("test_auth_user", "AnotherPass2!");

    assert(!duplicate.success);

    const auto correct_login =
        auth.authenticate_user("test_auth_user", "StrongPass1!");

    assert(correct_login.success);
    assert(correct_login.role == hms::Role::Patient);

    // Test trusted internal role assignment.
    assert(auth.assign_role("test_auth_user", hms::Role::Doctor));

    const auto doctor_login =
        auth.authenticate_user("test_auth_user", "StrongPass1!");

    assert(doctor_login.success);
    assert(doctor_login.role == hms::Role::Doctor);

    // Reject an invalid role.
    assert(!auth.assign_role(
        "test_auth_user",
        static_cast<hms::Role>(999)));

    // Reject role assignment to a nonexistent user.
    assert(!auth.assign_role("nonexistent_user", hms::Role::Doctor));

    const auto wrong_login =
        auth.authenticate_user("test_auth_user", "WrongPass1!");

    assert(!wrong_login.success);
    assert(wrong_login.message == "Invalid username or password");

    const auto unknown_login =
        auth.authenticate_user("unknown_test_user", "StrongPass1!");

    assert(!unknown_login.success);
    assert(unknown_login.message == "Invalid username or password");


    // =========================================================
    // 3. Account lockout tests (HMS-F-002)
    // =========================================================

    using TestClock = hms::AuthService::Clock;
    auto fake_now = TestClock::time_point{} + std::chrono::hours(1);

    hms::AuthService lockout_auth(
        [&fake_now]() { return fake_now; });

    const std::string f002_password = "StrongPass1!";
    const std::string f002_wrong_password = "WrongPass1!";
    const std::string f002_generic_error = "Invalid username or password";

    // Four failed attempts do not lock the account.
    assert(lockout_auth.register_user(
        "f002_boundary_user", f002_password).success);

    for (int i = 0; i < 4; ++i) {
        auto result = lockout_auth.authenticate_user(
            "f002_boundary_user", f002_wrong_password);
        assert(!result.success);
        assert(result.message == f002_generic_error);
    }

    assert(lockout_auth.authenticate_user(
        "f002_boundary_user", f002_password).success);

    // The fifth consecutive failed attempt locks the account.
    for (int i = 0; i < 4; ++i) {
        auto result = lockout_auth.authenticate_user(
            "f002_boundary_user", f002_wrong_password);
        assert(!result.success);
        assert(result.message == f002_generic_error);
    }

    auto fifth_failure = lockout_auth.authenticate_user(
        "f002_boundary_user", f002_wrong_password);

    assert(!fifth_failure.success);
    assert(fifth_failure.message == f002_generic_error);

    // Correct password is rejected while the account is locked.
    auto locked_login = lockout_auth.authenticate_user(
        "f002_boundary_user", f002_password);

    assert(!locked_login.success);
    assert(locked_login.message == f002_generic_error);

    // Lockout expires at the exact 15-minute boundary.
    fake_now += std::chrono::minutes(15) - std::chrono::milliseconds(1);

    auto before_expiry = lockout_auth.authenticate_user(
        "f002_boundary_user", f002_password);

    assert(!before_expiry.success);
    assert(before_expiry.message == f002_generic_error);

    fake_now += std::chrono::milliseconds(1);

    auto after_expiry = lockout_auth.authenticate_user(
        "f002_boundary_user", f002_password);

    assert(after_expiry.success);

    // Successful login resets consecutive failures.
    assert(lockout_auth.register_user(
        "f002_reset_user", f002_password).success);

    for (int i = 0; i < 3; ++i) {
        auto result = lockout_auth.authenticate_user(
            "f002_reset_user", f002_wrong_password);
        assert(!result.success);
    }

    assert(lockout_auth.authenticate_user(
        "f002_reset_user", f002_password).success);

    for (int i = 0; i < 4; ++i) {
        auto result = lockout_auth.authenticate_user(
            "f002_reset_user", f002_wrong_password);
        assert(!result.success);
    }

    // Four failures after a successful login do not lock the account.
    assert(lockout_auth.authenticate_user(
        "f002_reset_user", f002_password).success);

    // Lockout is tracked independently for each account.
    assert(lockout_auth.register_user(
        "f002_independent_a", f002_password).success);
    assert(lockout_auth.register_user(
        "f002_independent_b", f002_password).success);

    for (int i = 0; i < 4; ++i) {
        assert(!lockout_auth.authenticate_user(
            "f002_independent_a", f002_wrong_password).success);
        assert(!lockout_auth.authenticate_user(
            "f002_independent_b", f002_wrong_password).success);
    }

    // Account A locks; account B remains usable.
    assert(!lockout_auth.authenticate_user(
        "f002_independent_a", f002_wrong_password).success);
    assert(!lockout_auth.authenticate_user(
        "f002_independent_a", f002_password).success);
    assert(lockout_auth.authenticate_user(
        "f002_independent_b", f002_password).success);


    // =========================================================
    // 4. RBAC tests (HMS-F-003 / TC-RBAC-01)
    // =========================================================

    using hms::Permission;
    using hms::Role;

    const Role roles[] = {
        Role::Patient,
        Role::Receptionist,
        Role::Doctor,
        Role::BillingOfficer,
        Role::Administrator
    };

    // Every role can log in and log out.
    for (Role role : roles) {
        assert(hms::is_permitted(role, Permission::LoginLogout));
    }

    // Patient registration: Receptionist only.
    assert(hms::is_permitted(
        Role::Receptionist, Permission::RegisterPatient));
    assert(!hms::is_permitted(
        Role::Patient, Permission::RegisterPatient));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::RegisterPatient));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::RegisterPatient));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::RegisterPatient));

    // Patient search: Receptionist and Doctor.
    assert(hms::is_permitted(
        Role::Receptionist, Permission::SearchPatients));
    assert(hms::is_permitted(
        Role::Doctor, Permission::SearchPatients));
    assert(!hms::is_permitted(
        Role::Patient, Permission::SearchPatients));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::SearchPatients));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::SearchPatients));

    // Updating patient data: Receptionist only.
    assert(hms::is_permitted(
        Role::Receptionist, Permission::UpdatePatientData));
    assert(!hms::is_permitted(
        Role::Patient, Permission::UpdatePatientData));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::UpdatePatientData));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::UpdatePatientData));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::UpdatePatientData));

    // Viewing free appointment slots: Patient and Receptionist.
    assert(hms::is_permitted(
        Role::Patient, Permission::ViewFreeSlots));
    assert(hms::is_permitted(
        Role::Receptionist, Permission::ViewFreeSlots));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::ViewFreeSlots));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::ViewFreeSlots));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::ViewFreeSlots));

    // Booking appointments: Patient and Receptionist.
    assert(hms::is_permitted(
        Role::Patient, Permission::BookAppointment));
    assert(hms::is_permitted(
        Role::Receptionist, Permission::BookAppointment));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::BookAppointment));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::BookAppointment));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::BookAppointment));

    // Rescheduling or cancelling: Patient and Receptionist.
    assert(hms::is_permitted(
        Role::Patient, Permission::RescheduleCancelAppointment));
    assert(hms::is_permitted(
        Role::Receptionist, Permission::RescheduleCancelAppointment));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::RescheduleCancelAppointment));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::RescheduleCancelAppointment));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::RescheduleCancelAppointment));

    // Viewing own schedule: Doctor only.
    assert(hms::is_permitted(
        Role::Doctor, Permission::ViewOwnSchedule));
    assert(!hms::is_permitted(
        Role::Patient, Permission::ViewOwnSchedule));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::ViewOwnSchedule));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::ViewOwnSchedule));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::ViewOwnSchedule));

    // Recording visits and prescribing: Doctor only.
    assert(hms::is_permitted(
        Role::Doctor, Permission::RecordVisitPrescribe));
    assert(!hms::is_permitted(
        Role::Patient, Permission::RecordVisitPrescribe));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::RecordVisitPrescribe));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::RecordVisitPrescribe));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::RecordVisitPrescribe));

    // Viewing patient history: Doctor only.
    assert(hms::is_permitted(
        Role::Doctor, Permission::ViewPatientHistory));
    assert(!hms::is_permitted(
        Role::Patient, Permission::ViewPatientHistory));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::ViewPatientHistory));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::ViewPatientHistory));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::ViewPatientHistory));

    // Viewing own history and bills: Patient only.
    assert(hms::is_permitted(
        Role::Patient, Permission::ViewOwnHistoryBills));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::ViewOwnHistoryBills));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::ViewOwnHistoryBills));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::ViewOwnHistoryBills));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::ViewOwnHistoryBills));

    // Billing, payments and receipts: Billing Officer only.
    assert(hms::is_permitted(
        Role::BillingOfficer, Permission::BillingPaymentsReceipts));
    assert(!hms::is_permitted(
        Role::Patient, Permission::BillingPaymentsReceipts));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::BillingPaymentsReceipts));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::BillingPaymentsReceipts));
    assert(!hms::is_permitted(
        Role::Administrator, Permission::BillingPaymentsReceipts));

    // Reports: Billing Officer and Administrator.
    assert(hms::is_permitted(
        Role::BillingOfficer, Permission::GenerateReports));
    assert(hms::is_permitted(
        Role::Administrator, Permission::GenerateReports));
    assert(!hms::is_permitted(
        Role::Patient, Permission::GenerateReports));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::GenerateReports));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::GenerateReports));

    // Managing users: Administrator only.
    assert(hms::is_permitted(
        Role::Administrator, Permission::ManageUsers));
    assert(!hms::is_permitted(
        Role::Patient, Permission::ManageUsers));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::ManageUsers));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::ManageUsers));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::ManageUsers));

    // Viewing the audit log: Administrator only.
    assert(hms::is_permitted(
        Role::Administrator, Permission::ViewAuditLog));
    assert(!hms::is_permitted(
        Role::Patient, Permission::ViewAuditLog));
    assert(!hms::is_permitted(
        Role::Receptionist, Permission::ViewAuditLog));
    assert(!hms::is_permitted(
        Role::Doctor, Permission::ViewAuditLog));
    assert(!hms::is_permitted(
        Role::BillingOfficer, Permission::ViewAuditLog));


    // =========================================================
    // 5. Authorization result and denial-message tests
    // =========================================================

    std::string authorization_error = "old error";

    assert(!hms::authorize(
        Role::Patient,
        Permission::RegisterPatient,
        authorization_error));

    assert(authorization_error == "Access denied");

    assert(hms::authorize(
        Role::Receptionist,
        Permission::RegisterPatient,
        authorization_error));

    assert(authorization_error.empty());

    const auto invalid_role = static_cast<Role>(999);

    assert(!hms::is_permitted(
        invalid_role, Permission::RegisterPatient));

    assert(!hms::is_permitted(
        invalid_role, Permission::LoginLogout));


    // =========================================================
    // 6. Session management tests (HMS-SEC-003)
    // =========================================================

    auto session_now =
        TestClock::time_point{} + std::chrono::hours(100);

    hms::AuthService session_auth(
        [&session_now]() { return session_now; });

    const std::string sec003_password = "StrongPass1!";

    assert(session_auth.register_user(
        "sec003_session_user", sec003_password).success);

    const auto session_login = session_auth.authenticate_user(
        "sec003_session_user", sec003_password);

    assert(session_login.success);

    // Successful authentication creates a valid session.
    const auto created_session =
        session_auth.create_session(session_login);

    assert(created_session.success);
    assert(!created_session.token.empty());
    assert(created_session.user_id == "sec003_session_user");

    const auto valid_session =
        session_auth.validate_session(created_session.token);

    assert(valid_session.valid);
    assert(valid_session.user_id == "sec003_session_user");

    // Activity refreshes the idle timeout.
    session_now += std::chrono::minutes(9);
    assert(session_auth.validate_session(created_session.token).valid);

    // Nine minutes after activity, the session remains valid.
    session_now += std::chrono::minutes(9);
    assert(session_auth.validate_session(created_session.token).valid);

    // Ten minutes of inactivity expires the session at the boundary.
    session_now += std::chrono::minutes(10);
    assert(!session_auth.validate_session(created_session.token).valid);

    // Expired tokens cannot be reused.
    assert(!session_auth.validate_session(created_session.token).valid);

    // A separate session is invalidated immediately by logout.
    const auto second_login = session_auth.authenticate_user(
        "sec003_session_user", sec003_password);

    assert(second_login.success);

    const auto logout_session =
        session_auth.create_session(second_login);

    assert(logout_session.success);
    assert(session_auth.validate_session(logout_session.token).valid);

    assert(session_auth.logout(logout_session.token));
    assert(!session_auth.validate_session(logout_session.token).valid);

    // Logging out the same token twice must not succeed.
    assert(!session_auth.logout(logout_session.token));

    // Failed authentication must not create a session.
    hms::AuthResult failed_auth{
        false,
        "",
        "Invalid username or password",
        hms::Role::Patient
    };

    assert(!session_auth.create_session(failed_auth).success);


    std::cout
        << "All API, authentication, account-lockout, RBAC, and session tests passed.\n";

    return 0;
}
