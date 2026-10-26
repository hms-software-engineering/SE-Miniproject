#pragma once
#include <string>
#include <vector>

#include "services/rbac_service.h"

namespace hms {

// Mirrors the audit_log table. There is deliberately no free-text field:
// HMS-SEC-008 forbids passwords and clinical content in audit records.
struct AuditEntry {
    long long id = 0;
    long long user_id = 0;
    Role role = Role::Patient;
    std::string action;     // code such as "VISIT_CREATE"
    std::string record_id;  // identifier only, e.g. "PID-00000001" or "42"
    std::string timestamp;  // "YYYY-MM-DD HH:MM:SS"
    std::string source_ip;
};

// Insert-only. The database version relies on trg_audit_log_no_update_delete.
class AuditStore {
public:
    virtual ~AuditStore() = default;
    virtual AuditEntry append(const AuditEntry& e) = 0;
    virtual std::vector<AuditEntry> all() const = 0;
};

class InMemoryAuditStore : public AuditStore {
public:
    AuditEntry append(const AuditEntry& e) override;
    std::vector<AuditEntry> all() const override;

private:
    std::vector<AuditEntry> entries_;
    long long next_id_ = 1;
};

// "VISIT_CREATE": 1-120 chars of A-Z, 0-9 and '_', and not naming a secret.
bool valid_audit_action(const std::string& action);
// "PID-00000001", "42": 0-120 chars of A-Z, a-z, 0-9, '-' and '_'.
bool valid_audit_record_id(const std::string& record_id);

// Returns false (and stores nothing) if the entry could carry sensitive content.
bool write_audit(AuditStore& store, const AuditEntry& e, std::string& error_message);

// HMS-SEC-008: Administrator only. Optional filters match exactly; empty means any.
bool read_audit(const AuditStore& store,
                Role caller,
                const std::string& action_filter,
                const std::string& record_filter,
                std::vector<AuditEntry>& out,
                std::string& error_message);

}  // namespace hms
