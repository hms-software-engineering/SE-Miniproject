#include "services/rbac_service.h"

namespace hms {
namespace {

bool is_valid_role(Role role)
{
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

std::string_view role_name(Role role)
{
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

bool parse_role(std::string_view value, Role& role)
{
    if (value == "Patient") {
        role = Role::Patient;
    } else if (value == "Receptionist") {
        role = Role::Receptionist;
    } else if (value == "Doctor") {
        role = Role::Doctor;
    } else if (value == "Billing Officer") {
        role = Role::BillingOfficer;
    } else if (value == "Administrator") {
        role = Role::Administrator;
    } else {
        return false;
    }

    return true;
}

bool is_permitted(Role role, Permission permission)
{
    if (!is_valid_role(role)) {
        return false;
    }

    switch (permission) {
    case Permission::LoginLogout:
        return true;

    case Permission::CreateOwnPatientProfile:
        return role == Role::Patient;
    case Permission::RegisterPatient:
    case Permission::UpdatePatientData:
        return role == Role::Receptionist;

    case Permission::ViewPatientRecord:
        return role == Role::Patient ||
               role == Role::Receptionist ||
               role == Role::Doctor;

    case Permission::SearchPatients:
        return role == Role::Receptionist ||
               role == Role::Doctor;

    case Permission::ViewFreeSlots:
    case Permission::BookAppointment:
    case Permission::RescheduleCancelAppointment:
        return role == Role::Patient ||
               role == Role::Receptionist;

    case Permission::ViewOwnSchedule:
    case Permission::RecordVisitPrescribe:
        return role == Role::Doctor;

    case Permission::ViewPatientHistory:
        return role == Role::Doctor || role == Role::Receptionist;
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

    return false;
}

bool authorize(Role role,
               Permission permission,
               std::string& error_message)
{
    if (is_permitted(role, permission)) {
        error_message.clear();
        return true;
    }

    error_message = "Access denied";
    return false;
}

bool authorize_record_access(Role role,
                             Permission permission,
                             std::string_view requesting_user_id,
                             std::string_view record_owner_user_id,
                             std::string& error_message)
{
    if (!is_permitted(role, permission)) {
        error_message = "Access denied";
        return false;
    }

    const bool patient_own_record =
        role == Role::Patient &&
        (permission == Permission::BookAppointment ||
         permission == Permission::RescheduleCancelAppointment ||
         permission == Permission::ViewOwnHistoryBills ||
         permission == Permission::ViewPatientRecord);

    if (patient_own_record &&
        (requesting_user_id.empty() ||
         record_owner_user_id.empty() ||
         requesting_user_id != record_owner_user_id)) {
        error_message = "Access denied";
        return false;
    }

    error_message.clear();
    return true;
}

} // namespace hms
