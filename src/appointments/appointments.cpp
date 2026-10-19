#include "appointments/appointments.h"
#include <algorithm>
#include <cstdio>

namespace hms {

bool InMemoryAppointmentRepository::is_booked(const std::string& d,
        const std::string& date, const std::string& time) const {
    for (const auto& kv : items_) {
        const Appointment& a = kv.second;
        if (a.doctor_id == d && a.date == date && a.time == time &&
            a.status == "Confirmed") return true;
    }
    return false;
}

Appointment InMemoryAppointmentRepository::save(Appointment a) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "APT-%08d", next_++);
    a.id = buf;
    items_[a.id] = a;
    return a;
}

std::vector<std::string> day_slots() {
    std::vector<std::string> out;
    for (int m = 9 * 60; m < 17 * 60; m += 30) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
        out.push_back(buf);
    }
    return out;
}

std::vector<std::string> free_slots(const AppointmentRepository& repo,
        const std::string& doctor_id, const std::string& date) {
    std::vector<std::string> out;
    for (const auto& s : day_slots())
        if (!repo.is_booked(doctor_id, date, s)) out.push_back(s);
    return out;
}

static BookingResult fail(int status, const char* code,
                          const char* msg, const char* field = "") {
    BookingResult r;
    r.http_status = status; r.code = code; r.message = msg; r.field = field;
    return r;
}

BookingResult book_appointment(AppointmentRepository& repo,
        const std::string& patient_id, const std::string& doctor_id,
        const std::string& date, const std::string& time) {
    if (patient_id.empty()) return fail(400, "VALIDATION_FAILED", "patientId is required", "patientId");
    if (doctor_id.empty())  return fail(400, "VALIDATION_FAILED", "doctorId is required", "doctorId");
    if (date.empty())       return fail(400, "VALIDATION_FAILED", "date is required", "date");
    auto slots = day_slots();
    if (std::find(slots.begin(), slots.end(), time) == slots.end())
        return fail(400, "VALIDATION_FAILED", "time is not a valid slot", "time");
    // F-021 will replace this with a transactional check plus 3 alternatives.
    if (repo.is_booked(doctor_id, date, time))
        return fail(409, "SLOT_TAKEN", "Slot is no longer available");

    Appointment a{"", patient_id, doctor_id, date, time, "Confirmed"};
    BookingResult r;
    r.ok = true; r.http_status = 201; r.appointment = repo.save(a);
    return r;
}

std::string slots_to_json(const std::vector<std::string>& s) {
    std::string j = "{\"slots\":[";
    for (size_t i = 0; i < s.size(); ++i) j += (i ? ",\"" : "\"") + s[i] + "\"";
    return j + "]}";
}

std::string appointment_to_json(const Appointment& a) {
    return "{\"appointmentId\":\"" + a.id + "\",\"patientId\":\"" + a.patient_id +
           "\",\"doctorId\":\"" + a.doctor_id + "\",\"date\":\"" + a.date +
           "\",\"time\":\"" + a.time + "\",\"status\":\"" + a.status + "\"}";
}

} // namespace hms
