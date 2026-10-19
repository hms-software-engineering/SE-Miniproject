
#include "../src/api/api.h"
#include "../src/services/auth_service.h"
#include "../src/services/rbac_service.h"

#include <cassert>
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
    // Test that a trusted internal caller can assign a role.
assert(auth.assign_role("test_auth_user", hms::Role::Doctor));

// Verify that login returns the newly assigned role.
const auto doctor_login =
    auth.authenticate_user("test_auth_user", "StrongPass1!");

assert(doctor_login.success);
assert(doctor_login.role == hms::Role::Doctor);

// Reject an invalid role.
assert(!auth.assign_role(
    "test_auth_user",
    static_cast<hms::Role>(999)
));

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
    // 3. RBAC tests (HMS-F-003 / TC-RBAC-01)
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
    // 4. Authorization result and denial-message tests
    // =========================================================

    std::string authorization_error = "old error";

    // A denied request returns false and the exact SRS message.
    assert(!hms::authorize(
        Role::Patient,
        Permission::RegisterPatient,
        authorization_error));

    assert(authorization_error == "Access denied");

    // An allowed request succeeds and clears the old error.
    assert(hms::authorize(
        Role::Receptionist,
        Permission::RegisterPatient,
        authorization_error));

    assert(authorization_error.empty());

    // Unknown role values must not gain permissions.
    const auto invalid_role = static_cast<Role>(999);

    assert(!hms::is_permitted(
        invalid_role, Permission::RegisterPatient));

    assert(!hms::is_permitted(
        invalid_role, Permission::LoginLogout));

    std::cout << "All API, authentication, and RBAC tests passed.\n";

    return 0;
}
