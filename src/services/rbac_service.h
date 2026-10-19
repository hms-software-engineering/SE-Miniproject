
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
    SearchPatients,
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

bool is_permitted(Role role, Permission permission);

// Returns false unless the role is permitted. For denied requests,
// the message is exactly "Access denied".
bool authorize(Role role,
               Permission permission,
               std::string& error_message);

} // namespace hms
