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

// HMS-F-031
struct PrescriptionItem {
    long long id = 0;
    long long visit_id = 0;
    std::string medicine;
    std::string dosage;
    std::string frequency;
    int days = 0;
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
    virtual bool find_visit(long long visit_id, Visit& out) const = 0;
    virtual Visit insert_visit(const Visit& v) = 0;
    virtual PrescriptionItem insert_prescription(const PrescriptionItem& p) = 0;
    virtual std::vector<PrescriptionItem> prescriptions_for_visit(long long visit_id) const = 0;
};

class InMemoryClinicalRepository : public ClinicalRepository {
public:
    InMemoryClinicalRepository() = default;
    void add_appointment(const AppointmentInfo& a);

    bool find_appointment(long long appointment_id, AppointmentInfo& out) const override;
    bool has_visit_for_appointment(long long appointment_id) const override;
    bool find_visit(long long visit_id, Visit& out) const override;
    Visit insert_visit(const Visit& v) override;
    PrescriptionItem insert_prescription(const PrescriptionItem& p) override;
    std::vector<PrescriptionItem> prescriptions_for_visit(long long visit_id) const override;

private:
    std::map<long long, AppointmentInfo> appointments_;
    std::vector<Visit> visits_;
    std::vector<PrescriptionItem> prescriptions_;
    long long next_visit_id_ = 1;
    long long next_item_id_ = 1;
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
    PrescriptionItem item;
};

// HMS-F-030: the visit is stamped with now and the calling doctor's ID.
ClinicalResult record_visit(ClinicalRepository& repo,
                            const Caller& caller,
                            long long appointment_id,
                            const std::string& symptoms,
                            const std::string& diagnosis,
                            const std::string& notes,
                            const std::string& now);

// HMS-F-031
ClinicalResult add_prescription(ClinicalRepository& repo,
                                const Caller& caller,
                                long long visit_id,
                                const std::string& medicine,
                                const std::string& dosage,
                                const std::string& frequency,
                                int days);

}  // namespace hms
