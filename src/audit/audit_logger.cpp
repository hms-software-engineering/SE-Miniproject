#include "audit/audit_logger.h"

namespace hms {

namespace {

// Action codes that would hint at a secret being logged are refused outright.
const char* const kForbiddenWords[] = {"PASSWORD", "PASSWD", "SECRET", "TOKEN"};

}  // namespace

AuditEntry InMemoryAuditStore::append(const AuditEntry& e) {
    AuditEntry saved = e;
    saved.id = next_id_++;
    entries_.push_back(saved);
    return saved;
}

std::vector<AuditEntry> InMemoryAuditStore::all() const { return entries_; }

bool valid_audit_action(const std::string& action) {
    if (action.empty() || action.size() > 120) return false;
    for (char c : action) {
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    for (const char* w : kForbiddenWords) {
        if (action.find(w) != std::string::npos) return false;
    }
    return true;
}

bool valid_audit_record_id(const std::string& record_id) {
    if (record_id.size() > 120) return false;
    for (char c : record_id) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_'))
            return false;
    }
    return true;
}

bool write_audit(AuditStore& store, const AuditEntry& e, std::string& error_message) {
    if (!valid_audit_action(e.action)) {
        error_message = "Invalid audit action";
        return false;
    }
    if (!valid_audit_record_id(e.record_id)) {
        error_message = "Invalid audit record ID";
        return false;
    }
    store.append(e);
    error_message.clear();
    return true;
}

bool read_audit(const AuditStore& store,
                Role caller,
                const std::string& action_filter,
                const std::string& record_filter,
                std::vector<AuditEntry>& out,
                std::string& error_message) {
    out.clear();
    if (!authorize(caller, Permission::ViewAuditLog, error_message)) return false;
    for (const auto& e : store.all()) {
        if (!action_filter.empty() && e.action != action_filter) continue;
        if (!record_filter.empty() && e.record_id != record_filter) continue;
        out.push_back(e);
    }
    return true;
}

}  // namespace hms
