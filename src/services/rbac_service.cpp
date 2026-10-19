
#include "services/rbac_service.h"

namespace hms {
namespace {

bool is_valid_role(Role role) {
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

} // namespace

std::string_view role_name(Role role) {
    switch (role) {
    case Role::Patient:
        return "Patient";

    case Role::Receptionist:
        return "Receptionist";

    case Role::Doctor:
        return "Doctor";

    case Role::BillingOfficer:
        return "Billing Officer";

    case Role::Administrator:
        return "Administrator";
    }

    return "Unknown";
}

bool is_permitted(Role role, Permission permission) {
    // Reject invalid role values before checking permissions.
    if (!is_valid_role(role)) {
        return false;
    }

    switch (permission) {
    case Permission::LoginLogout:
        return true;

    case Permission::RegisterPatient:
    case Permission::UpdatePatientData:
        return role == Role::Receptionist;

    case Permission::SearchPatients:
        return role == Role::Receptionist ||
               role == Role::Doctor;

    case Permission::ViewFreeSlots:
        return role == Role::Patient ||
               role == Role::Receptionist;

    case Permission::BookAppointment:
    case Permission::RescheduleCancelAppointment:
        return role == Role::Patient ||
               role == Role::Receptionist;

    case Permission::ViewOwnSchedule:
    case Permission::RecordVisitPrescribe:
    case Permission::ViewPatientHistory:
        return role == Role::Doctor;

    case Permission::ViewOwnHistoryBills:
        return role == Role::Patient;

    case Permission::BillingPaymentsReceipts:
        return role == Role::BillingOfficer;

    case Permission::GenerateReports:
        return role == Role::BillingOfficer ||
               role == Role::Administrator;

    case Permission::ManageUsers:
    case Permission::ViewAuditLog:
        return role == Role::Administrator;
    }

    // Deny unknown permission values by default.
    return false;
}

bool authorize(Role role,
               Permission permission,
               std::string& error_message) {
    if (is_permitted(role, permission)) {
        error_message.clear();
        return true;
    }

    error_message = "Access denied";
    return false;
}

} // namespace hms
