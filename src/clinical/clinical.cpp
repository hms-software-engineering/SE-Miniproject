#include "clinical/clinical.h"

#include <algorithm>
#include <cctype>

namespace hms {

namespace {

// Drug families: an allergy to the key also covers each listed medicine.
const std::map<std::string, std::vector<std::string>> kAllergyFamilies = {
    {"penicillin", {"amoxicillin", "ampicillin", "penicillin", "cloxacillin",
                    "piperacillin", "co-amoxiclav"}},
    {"sulfa", {"sulfamethoxazole", "sulfasalazine", "co-trimoxazole"}},
    {"nsaid", {"ibuprofen", "diclofenac", "naproxen", "aspirin", "ketorolac"}},
    {"aspirin", {"aspirin", "acetylsalicylic"}},
    {"cephalosporin", {"cefalexin", "cephalexin", "ceftriaxone", "cefuroxime", "cefixime"}},
};

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

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

void InMemoryClinicalRepository::set_allergies(const std::string& patient_id,
                                               std::vector<std::string> list) {
    allergies_[patient_id] = std::move(list);
}

bool InMemoryClinicalRepository::find_appointment(long long appointment_id,
                                                  AppointmentInfo& out) const {
    auto it = appointments_.find(appointment_id);
    if (it == appointments_.end()) return false;
    out = it->second;
    return true;
}

std::vector<std::string> InMemoryClinicalRepository::allergies(const std::string& patient_id) const {
    auto it = allergies_.find(patient_id);
    return it == allergies_.end() ? std::vector<std::string>{} : it->second;
}

bool InMemoryClinicalRepository::has_visit_for_appointment(long long appointment_id) const {
    return std::any_of(visits_.begin(), visits_.end(),
                       [&](const Visit& v) { return v.appointment_id == appointment_id; });
}

bool InMemoryClinicalRepository::find_visit(long long visit_id, Visit& out) const {
    for (const auto& v : visits_) {
        if (v.id == visit_id) {
            out = v;
            return true;
        }
    }
    return false;
}

Visit InMemoryClinicalRepository::insert_visit(const Visit& v) {
    Visit saved = v;
    saved.id = next_visit_id_++;
    visits_.push_back(saved);
    return saved;
}

PrescriptionItem InMemoryClinicalRepository::insert_prescription(const PrescriptionItem& p) {
    PrescriptionItem saved = p;
    saved.id = next_item_id_++;
    prescriptions_.push_back(saved);
    return saved;
}

std::vector<PrescriptionItem> InMemoryClinicalRepository::prescriptions_for_visit(long long visit_id) const {
    std::vector<PrescriptionItem> out;
    for (const auto& p : prescriptions_)
        if (p.visit_id == visit_id) out.push_back(p);
    return out;
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

std::vector<std::string> matching_allergies(const std::string& medicine,
                                            const std::vector<std::string>& allergies) {
    const std::string med = lower(medicine);
    std::vector<std::string> out;
    for (const auto& allergy : allergies) {
        const std::string a = lower(trim(allergy));
        if (a.empty()) continue;
        bool hit = med.find(a) != std::string::npos;
        auto fam = kAllergyFamilies.find(a);
        if (!hit && fam != kAllergyFamilies.end()) {
            for (const auto& drug : fam->second) {
                if (med.find(drug) != std::string::npos) {
                    hit = true;
                    break;
                }
            }
        }
        if (hit) out.push_back(trim(allergy));
    }
    return out;
}

ClinicalResult add_prescription(ClinicalRepository& repo,
                                const Caller& caller,
                                long long visit_id,
                                const std::string& medicine,
                                const std::string& dosage,
                                const std::string& frequency,
                                int days,
                                const std::string& override_reason) {
    std::string err;
    if (!authorize(caller.role, Permission::RecordVisitPrescribe, err)) return access_denied();

    if (trim(medicine).empty())
        return fail(400, "VALIDATION_ERROR", "Medicine is required", "medicine");
    if (trim(dosage).empty())
        return fail(400, "VALIDATION_ERROR", "Dosage is required", "dosage");
    if (trim(frequency).empty())
        return fail(400, "VALIDATION_ERROR", "Frequency is required", "frequency");
    if (days <= 0)
        return fail(400, "VALIDATION_ERROR", "Duration must be at least 1 day", "days");

    Visit visit;
    if (!repo.find_visit(visit_id, visit))
        return fail(404, "NOT_FOUND", "Visit not found", "visitId");
    if (visit.doctor_id != caller.doctor_id) return access_denied();

    PrescriptionItem item;
    item.visit_id = visit_id;
    item.medicine = trim(medicine);
    item.dosage = trim(dosage);
    item.frequency = trim(frequency);
    item.days = days;

    auto hits = matching_allergies(item.medicine, repo.allergies(visit.patient_id));
    if (!hits.empty()) {
        if (trim(override_reason).empty()) {
            std::string list;
            for (const auto& h : hits) list += (list.empty() ? "" : ", ") + h;
            return fail(409, "ALLERGY_WARNING",
                        "Patient has a recorded allergy to " + list +
                            ". Enter an override reason to save this item",
                        "overrideReason");
        }
        item.override_reason = trim(override_reason);
    }

    ClinicalResult r;
    r.ok = true;
    r.http_status = 201;
    r.item = repo.insert_prescription(item);
    return r;
}

}  // namespace hms
