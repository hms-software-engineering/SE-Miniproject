#pragma once
#include <map>
#include <string>
#include <vector>

#include "services/rbac_service.h"

namespace hms {

// HMS-F-030
struct Visit {
    long long id = 0;
    long long appointment_id = 0;
    std::string patient_id;
    long long doctor_id = 0;
    std::string symptoms;
    std::string diagnosis;
    std::string notes;
    std::string visited_at;  // "YYYY-MM-DD HH:MM"
};

// What a doctor needs to know about an appointment before recording a visit.
struct AppointmentInfo {
    long long id = 0;
    std::string patient_id;
    long long doctor_id = 0;
    std::string status;
};

// A PostgreSQL version will implement this same interface later.
class ClinicalRepository {
public:
    virtual ~ClinicalRepository() = default;
    // false if the appointment does not exist.
    virtual bool find_appointment(long long appointment_id, AppointmentInfo& out) const = 0;
    virtual bool has_visit_for_appointment(long long appointment_id) const = 0;
    virtual Visit insert_visit(const Visit& v) = 0;
};

class InMemoryClinicalRepository : public ClinicalRepository {
public:
    InMemoryClinicalRepository() = default;
    void add_appointment(const AppointmentInfo& a);

    bool find_appointment(long long appointment_id, AppointmentInfo& out) const override;
    bool has_visit_for_appointment(long long appointment_id) const override;
    Visit insert_visit(const Visit& v) override;

private:
    std::map<long long, AppointmentInfo> appointments_;
    std::vector<Visit> visits_;
    long long next_visit_id_ = 1;
};

// Who is making the request. doctor_id is set for doctors.
struct Caller {
    Role role = Role::Patient;
    long long doctor_id = 0;
};

struct ClinicalResult {
    bool ok = false;
    int http_status = 0;
    std::string code, message, field;
    Visit visit;
};

// HMS-F-030: the visit is stamped with now and the calling doctor's ID.
ClinicalResult record_visit(ClinicalRepository& repo,
                            const Caller& caller,
                            long long appointment_id,
                            const std::string& symptoms,
                            const std::string& diagnosis,
                            const std::string& notes,
                            const std::string& now);

}  // namespace hms
