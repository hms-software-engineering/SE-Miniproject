#include "appointments/appointments.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <nlohmann/json.hpp>

namespace hms {

namespace {

const char* const kDayNames[7] = {"sunday",   "monday", "tuesday", "wednesday",
                                  "thursday", "friday", "saturday"};

bool valid_date(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') {
        return false;
    }

    for (size_t i = 0; i < d.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }

        if (!std::isdigit(static_cast<unsigned char>(d[i]))) {
            return false;
        }
    }

    const int year = std::stoi(d.substr(0, 4));
    const int month = std::stoi(d.substr(5, 2));
    const int day = std::stoi(d.substr(8, 2));

    if (year < 1 || month < 1 || month > 12 || day < 1) {
        return false;
    }

    const bool leap_year =
        (year % 400 == 0) ||
        (year % 4 == 0 && year % 100 != 0);

    const int days_in_month[] = {
        31, leap_year ? 29 : 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    return day <= days_in_month[month - 1];
}


// Reads the "HH:MM-HH:MM" value for a day name from the JSON text.
std::string range_for_day(const std::string& json, const std::string& day) {
    std::string key = "\"" + day + "\"";
    size_t k = json.find(key);
    if (k == std::string::npos) return "";
    size_t colon = json.find(':', k + key.size());
    if (colon == std::string::npos) return "";
    size_t q1 = json.find('"', colon);
    if (q1 == std::string::npos) return "";
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    return json.substr(q1 + 1, q2 - q1 - 1);
}

bool parse_hhmm(const std::string& s, int& minutes) {
    if (s.size() != 5 || s[2] != ':') return false;
    for (size_t i : {0u, 1u, 3u, 4u})
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    int h = std::stoi(s.substr(0, 2));
    int m = std::stoi(s.substr(3, 2));
    if (h > 23 || m > 59) return false;
    minutes = h * 60 + m;
    return true;
}

BookingResult fail(int status, const std::string& code, const std::string& msg,
                   const std::string& field = "") {
    BookingResult r;
    r.http_status = status;
    r.code = code;
    r.message = msg;
    r.field = field;
    return r;
}

}  // namespace
bool is_valid_calendar_date(const std::string& date) {
    return valid_date(date);
}
// ---------- in-memory repository ----------

InMemoryAppointmentRepository::InMemoryAppointmentRepository() {
    doctors_[1] =
        "{\"monday\":\"09:00-13:00\",\"wednesday\":\"09:00-13:00\","
        "\"friday\":\"14:00-17:00\"}";
    patients_.insert("PID-00000001");
    patients_.insert("PID-00000002");
}

std::string InMemoryAppointmentRepository::working_hours(long long doctor_id) const {
    auto it = doctors_.find(doctor_id);
    return it == doctors_.end() ? "" : it->second;
}

std::vector<std::string> InMemoryAppointmentRepository::booked_times(
    long long doctor_id, const std::string& date) const {
    std::vector<std::string> out;
    for (const auto& a : items_) {
        if (a.doctor_id == doctor_id && a.status == "Confirmed" &&
            a.slot_start.substr(0, 10) == date)
            out.push_back(a.slot_start.substr(11));
    }
    return out;
}

SaveResult InMemoryAppointmentRepository::insert(const Appointment& a) {
    SaveResult r;
    if (patients_.find(a.patient_id) == patients_.end()) {
        r.status = SaveStatus::PatientMissing;
        return r;
    }
    if (doctors_.find(a.doctor_id) == doctors_.end()) {
        r.status = SaveStatus::DoctorMissing;
        return r;
    }
    for (const auto& x : items_) {
        if (x.doctor_id == a.doctor_id && x.status == "Confirmed" &&
            x.slot_start == a.slot_start) {
            r.status = SaveStatus::SlotTaken;
            return r;
        }
    }
    Appointment saved = a;
    saved.id = next_id_++;
    saved.status = "Confirmed";
    items_.push_back(saved);
    r.status = SaveStatus::Ok;
    r.appointment = saved;
    return r;
}

// ---------- logic ----------

int weekday_of(const std::string& date) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = std::stoi(date.substr(0, 4));
    int m = std::stoi(date.substr(5, 2));
    int d = std::stoi(date.substr(8, 2));
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

std::vector<std::string> slots_for_date(const std::string& working_hours_json,
                                        const std::string& date) {
    std::vector<std::string> out;
    if (!valid_date(date)) return out;
    std::string range = range_for_day(working_hours_json, kDayNames[weekday_of(date)]);
    size_t dash = range.find('-');
    if (dash == std::string::npos) return out;
    int start = 0, end = 0;
    if (!parse_hhmm(range.substr(0, dash), start) ||
        !parse_hhmm(range.substr(dash + 1), end))
        return out;
    for (int m = start; m + 30 <= end; m += 30) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
        out.push_back(buf);
    }
    return out;
}

std::vector<std::string> free_slots(const AppointmentRepository& repo,
                                    long long doctor_id, const std::string& date) {
    std::vector<std::string> out;
    std::string hours = repo.working_hours(doctor_id);
    if (hours.empty()) return out;
    std::vector<std::string> booked = repo.booked_times(doctor_id, date);
    for (const auto& s : slots_for_date(hours, date)) {
        if (std::find(booked.begin(), booked.end(), s) == booked.end())
            out.push_back(s);
    }
    return out;
}


BookingResult book_appointment(AppointmentRepository& repo,
                               const std::string& patient_id,
                               long long doctor_id,
                               const std::string& date,
                               const std::string& time) {
    if (patient_id.empty())
        return fail(400, "VALIDATION_FAILED",
                    "patientId is required", "patientId");

    if (doctor_id <= 0)
        return fail(400, "VALIDATION_FAILED",
                    "doctorId is required", "doctorId");

    if (!valid_date(date))
        return fail(400, "INVALID_DATE",
                    "date must be YYYY-MM-DD", "date");

    std::string hours = repo.working_hours(doctor_id);
    if (hours.empty())
        return fail(404, "NOT_FOUND",
                    "Doctor not found", "doctorId");

    // Get today's date and current local time.
    const std::time_t now = std::time(nullptr);
    std::tm local_now = *std::localtime(&now);

    char today_buffer[11];
    char current_time_buffer[6];

    std::strftime(today_buffer, sizeof(today_buffer),
                  "%Y-%m-%d", &local_now);
    std::strftime(current_time_buffer, sizeof(current_time_buffer),
                  "%H:%M", &local_now);

    const std::string today(today_buffer);
    const std::string current_time(current_time_buffer);

    // Reject dates before today.
    if (date < today)
        return fail(400, "INVALID_DATE",
                    "Appointments cannot be booked in the past",
                    "slotStart");

    // Allow bookings through day 60, but not beyond it.
    std::tm max_date_tm = local_now;
    max_date_tm.tm_hour = 12;
    max_date_tm.tm_min = 0;
    max_date_tm.tm_sec = 0;
    max_date_tm.tm_mday += 60;
    max_date_tm.tm_isdst = -1;

    if (std::mktime(&max_date_tm) == -1)
        return fail(500, "INTERNAL_ERROR",
                    "Could not calculate booking date limit");

    char max_date_buffer[11];
    std::strftime(max_date_buffer, sizeof(max_date_buffer),
                  "%Y-%m-%d", &max_date_tm);

    const std::string max_date(max_date_buffer);

    if (date > max_date)
        return fail(400, "INVALID_DATE",
                    "Appointments can only be booked up to 60 days ahead",
                    "slotStart");

    // Reject a time that has already elapsed today.
    if (date == today) {
        int requested_minutes = 0;
        int current_minutes = 0;

        if (parse_hhmm(time, requested_minutes) &&
            parse_hhmm(current_time, current_minutes) &&
            requested_minutes <= current_minutes) {
            return fail(400, "INVALID_DATE",
                        "Appointment time has already elapsed",
                        "slotStart");
        }
    }

    // Validate the requested time against the doctor's working slots.
    auto slots = slots_for_date(hours, date);

    if (std::find(slots.begin(), slots.end(), time) == slots.end())
        return fail(400, "VALIDATION_FAILED",
                    "time is not a working slot for this doctor",
                    "time");

    Appointment a;
    a.patient_id = patient_id;
    a.doctor_id = doctor_id;
    a.slot_start = date + " " + time;
    a.status = "Confirmed";

    SaveResult s = repo.insert(a);

    switch (s.status) {
    case SaveStatus::SlotTaken: {
        BookingResult r = fail(
            409, "SLOT_TAKEN", "This slot is already booked.");

        r.next_slots = next_free_slots(
            repo, doctor_id, date, time, 3);

        return r;
    }

    case SaveStatus::PatientMissing:
        return fail(400, "VALIDATION_FAILED",
                    "Patient not found", "patientId");

    case SaveStatus::DoctorMissing:
        return fail(404, "NOT_FOUND",
                    "Doctor not found", "doctorId");

    case SaveStatus::Ok:
        break;
    }

    BookingResult r;
    r.ok = true;
    r.http_status = 201;
    r.appointment = s.appointment;
    return r;
}


std::vector<std::string> next_free_slots(
    const AppointmentRepository& repo,
    long long doctor_id,
    const std::string& date,
    const std::string& after_time,
    size_t count
) {
    std::vector<std::string> result;

    if (count == 0) {
        return result;
    }

    for (const auto& slot : free_slots(repo, doctor_id, date)) {
        // Slot strings use HH:MM, so lexical order is chronological.
        if (slot > after_time) {
            result.push_back(slot);

            if (result.size() >= count) {
                break;
            }
        }
    }

    return result;
}

std::string booking_error_to_json(
    const BookingResult& r,
    const std::string& request_id
) {
    nlohmann::json error = {
        {"code", r.code},
        {"message", r.message}
    };

    if (!r.field.empty()) {
        error["field"] = r.field;
    }

    if (!r.next_slots.empty()) {
        error["nextSlots"] = r.next_slots;
    }

    nlohmann::json response = {
        {"error", error},
        {"requestId", request_id}
    };

    return response.dump();
}
std::string slots_to_json(const std::vector<std::string>& slots) {
    std::string j = "{\"slots\":[";
    for (size_t i = 0; i < slots.size(); ++i) {
        if (i) j += ",";
        j += "\"" + slots[i] + "\"";
    }
    return j + "]}";
}

std::string appointment_to_json(const Appointment& a) {
    return "{\"appointmentId\":" + std::to_string(a.id) + ",\"patientId\":\"" +
           a.patient_id + "\",\"doctorId\":" + std::to_string(a.doctor_id) +
           ",\"slotStart\":\"" + a.slot_start + "\",\"status\":\"" + a.status + "\"}";
}

}  // namespace hms