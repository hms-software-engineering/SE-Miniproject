#include "clinical/clinical.h"

#include <algorithm>

namespace hms {

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

ClinicalResult fail(int status, const std::string& code, const std::string& msg,
                    const std::string& field = "") {
    ClinicalResult r;
    r.http_status = status;
    r.code = code;
    r.message = msg;
    r.field = field;
    return r;
}

ClinicalResult access_denied() { return fail(403, "ACCESS_DENIED", "Access denied"); }

}  // namespace

void InMemoryClinicalRepository::add_appointment(const AppointmentInfo& a) {
    appointments_[a.id] = a;
}

bool InMemoryClinicalRepository::find_appointment(long long appointment_id,
                                                  AppointmentInfo& out) const {
    auto it = appointments_.find(appointment_id);
    if (it == appointments_.end()) return false;
    out = it->second;
    return true;
}

bool InMemoryClinicalRepository::has_visit_for_appointment(long long appointment_id) const {
    return std::any_of(visits_.begin(), visits_.end(),
                       [&](const Visit& v) { return v.appointment_id == appointment_id; });
}

Visit InMemoryClinicalRepository::insert_visit(const Visit& v) {
    Visit saved = v;
    saved.id = next_visit_id_++;
    visits_.push_back(saved);
    return saved;
}

ClinicalResult record_visit(ClinicalRepository& repo,
                            const Caller& caller,
                            long long appointment_id,
                            const std::string& symptoms,
                            const std::string& diagnosis,
                            const std::string& notes,
                            const std::string& now) {
    std::string err;
    if (!authorize(caller.role, Permission::RecordVisitPrescribe, err)) return access_denied();

    if (trim(symptoms).empty())
        return fail(400, "VALIDATION_ERROR", "Symptoms are required", "symptoms");
    if (trim(diagnosis).empty())
        return fail(400, "VALIDATION_ERROR", "Diagnosis is required", "diagnosis");

    AppointmentInfo appt;
    if (!repo.find_appointment(appointment_id, appt))
        return fail(404, "NOT_FOUND", "Appointment not found", "appointmentId");
    // A doctor records visits only for their own appointments.
    if (appt.doctor_id != caller.doctor_id) return access_denied();
    if (appt.status == "Cancelled")
        return fail(400, "VALIDATION_ERROR", "Appointment is cancelled", "appointmentId");
    if (repo.has_visit_for_appointment(appointment_id))
        return fail(409, "VISIT_EXISTS", "A visit is already recorded for this appointment",
                    "appointmentId");

    Visit v;
    v.appointment_id = appointment_id;
    v.patient_id = appt.patient_id;
    v.doctor_id = caller.doctor_id;
    v.symptoms = trim(symptoms);
    v.diagnosis = trim(diagnosis);
    v.notes = trim(notes);
    v.visited_at = now;

    ClinicalResult r;
    r.ok = true;
    r.http_status = 201;
    r.visit = repo.insert_visit(v);
    return r;
}

}  // namespace hms
