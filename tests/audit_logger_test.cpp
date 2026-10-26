#include "../src/audit/audit_logger.h"

#include <cstdlib>
#include <iostream>

#define CHECK(c)                                                          \
    do {                                                                  \
        if (!(c)) {                                                       \
            std::cerr << "FAILED: " #c " (line " << __LINE__ << ")\n";    \
            std::exit(1);                                                 \
        }                                                                 \
    } while (0)

int main() {
    using hms::Role;
    hms::InMemoryAuditStore store;
    std::string err;

    // TC-SEC-05: login, view and update each leave an entry.
    CHECK(hms::write_audit(store, {0, 3, Role::Doctor, "LOGIN_SUCCESS", "", "2026-10-20 09:00:00", "10.0.0.5"}, err));
    CHECK(hms::write_audit(store, {0, 3, Role::Doctor, "PATIENT_HISTORY_VIEW", "PID-00000001", "2026-10-20 09:01:00", "10.0.0.5"}, err));
    CHECK(hms::write_audit(store, {0, 3, Role::Doctor, "VISIT_CREATE", "42", "2026-10-20 09:05:00", "10.0.0.5"}, err));
    CHECK(store.all().size() == 3);

    // No passwords or clinical content can be written.
    CHECK(!hms::write_audit(store, {0, 3, Role::Doctor, "PASSWORD_CHANGE", "3", "t", ""}, err));
    CHECK(!hms::write_audit(store, {0, 3, Role::Doctor, "LOGIN password=hunter2", "", "t", ""}, err));
    CHECK(!hms::write_audit(store, {0, 3, Role::Doctor, "VISIT_CREATE", "diagnosis: viral infection", "t", ""}, err));
    CHECK(!hms::write_audit(store, {0, 3, Role::Doctor, "", "1", "t", ""}, err));
    CHECK(err == "Invalid audit action");
    CHECK(store.all().size() == 3);
    CHECK(hms::valid_audit_record_id("PID-00000001"));
    CHECK(!hms::valid_audit_record_id("x'; DROP TABLE audit_log;--"));

    // Only the Administrator can read the audit log.
    std::vector<hms::AuditEntry> out;
    CHECK(hms::read_audit(store, Role::Administrator, "", "", out, err));
    CHECK(out.size() == 3);
    for (Role r : {Role::Patient, Role::Receptionist, Role::Doctor, Role::BillingOfficer}) {
        CHECK(!hms::read_audit(store, r, "", "", out, err));
        CHECK(err == "Access denied");
        CHECK(out.empty());
    }

    // Filters (HMS-F-061 uses these).
    CHECK(hms::read_audit(store, Role::Administrator, "VISIT_CREATE", "", out, err));
    CHECK(out.size() == 1 && out[0].record_id == "42");
    CHECK(hms::read_audit(store, Role::Administrator, "", "PID-00000001", out, err));
    CHECK(out.size() == 1 && out[0].action == "PATIENT_HISTORY_VIEW");

    std::cout << "All audit logger tests passed.\n";
    return 0;
}
