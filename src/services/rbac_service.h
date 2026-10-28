#pragma once

#include <string>
#include <string_view>

namespace hms {

enum class Role {
    Patient,
    Receptionist,
    Doctor,
    BillingOfficer,
    Administrator
};

enum class Permission {
    LoginLogout,
    RegisterPatient,
    CreateOwnPatientProfile,
    SearchPatients,
    ViewPatientRecord,
    UpdatePatientData,
    ViewFreeSlots,
    BookAppointment,
    RescheduleCancelAppointment,
    ViewOwnSchedule,
    RecordVisitPrescribe,
    ViewPatientHistory,
    ViewOwnHistoryBills,
    BillingPaymentsReceipts,
    GenerateReports,
    ManageUsers,
    ViewAuditLog
};

std::string_view role_name(Role role);
bool parse_role(std::string_view value, Role& role);
bool is_permitted(Role role, Permission permission);
bool authorize(Role role, Permission permission, std::string& error_message);
bool authorize_record_access(Role role,
                             Permission permission,
                             std::string_view requesting_user_id,
                             std::string_view record_owner_user_id,
                             std::string& error_message);

} // namespace hms
