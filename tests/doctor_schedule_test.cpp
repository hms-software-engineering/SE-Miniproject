#include "appointments/appointments.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

#define CHECK(c) do { \
    if (!(c)) { \
        std::cerr << "FAILED: " #c " (line " << __LINE__ << ")\n"; \
        std::exit(1); \
    } \
} while (0)

int main()
{
    using namespace hms;

    InMemoryAppointmentRepository repo;
    repo.add_doctor(
        1, "doctor-user-1",
        "{\"monday\":\"09:00-13:00\"}");
    repo.add_doctor(
        2, "doctor-user-2",
        "{\"monday\":\"09:00-13:00\"}");

    // Add appointments out of chronological order.
    const auto later = repo.insert({
        0, "PID-00000001", 1, "2026-10-26 11:00", "Confirmed"
    });
    const auto earlier = repo.insert({
        0, "PID-00000002", 1, "2026-10-26 09:00", "Confirmed"
    });
    const auto other_doctor = repo.insert({
        0, "PID-00000001", 2, "2026-10-26 10:00", "Confirmed"
    });
    const auto other_date = repo.insert({
        0, "PID-00000001", 1, "2026-10-27 10:00", "Confirmed"
    });

    CHECK(later.status == SaveStatus::Ok);
    CHECK(earlier.status == SaveStatus::Ok);
    CHECK(other_doctor.status == SaveStatus::Ok);
    CHECK(other_date.status == SaveStatus::Ok);

    auto result = doctor_schedule(repo, "doctor-user-1", "2026-10-26");

    CHECK(result.ok);
    CHECK(result.http_status == 200);
    CHECK(result.appointments.size() == 2);
    CHECK(result.appointments[0].slot_start == "2026-10-26 09:00");
    CHECK(result.appointments[1].slot_start == "2026-10-26 11:00");
    const auto success_json = nlohmann::json::parse(
    schedule_to_json(result.appointments));

    CHECK(success_json.is_array());
    CHECK(success_json.size() == 2);

    CHECK(success_json[0].contains("appointmentId"));
    CHECK(success_json[0].contains("patientId"));
    CHECK(success_json[0].contains("slotStart"));
    CHECK(success_json[0].contains("status"));

    CHECK(success_json[0]["slotStart"] == "2026-10-26 09:00");
    CHECK(success_json[0]["patientId"] == "PID-00000002");
    CHECK(success_json[0]["status"] == "Confirmed");

    // The schedule must not expose another doctor's appointments.
    for (const auto& appointment : result.appointments) {
        CHECK(appointment.doctor_id == 1);
    }

    // Cancelled appointments are excluded.
    repo.set_status(later.appointment.id, "Cancelled");
    result = doctor_schedule(repo, "doctor-user-1", "2026-10-26");
    CHECK(result.ok);
    CHECK(result.appointments.size() == 1);
    CHECK(result.appointments[0].slot_start == "2026-10-26 09:00");

    // A valid date with no appointments returns an empty schedule.
    result = doctor_schedule(repo, "doctor-user-1", "2026-10-28");
    CHECK(result.ok);
    CHECK(result.appointments.empty());
    CHECK(schedule_to_json(result.appointments) == "[]");

    // Invalid calendar dates are rejected.
    result = doctor_schedule(repo, "doctor-user-1", "2026-02-30");
    CHECK(!result.ok);
    CHECK(result.http_status == 400);
    CHECK(result.code == "VALIDATION_FAILED");
    CHECK(result.field == "date");

    // Invalid date formats and out-of-range month/day values.
    for (const std::string& invalid_date : {
            "abc",
            "2026-13-45",
            "26-10-2026"
        }) {
        const auto invalid_result = doctor_schedule(
            repo, "doctor-user-1", invalid_date);

        CHECK(!invalid_result.ok);
        CHECK(invalid_result.http_status == 400);
        CHECK(invalid_result.code == "VALIDATION_FAILED");
        CHECK(invalid_result.field == "date");
    }

    // Missing date is rejected.
    result = doctor_schedule(repo, "doctor-user-1", "");
    CHECK(!result.ok);
    CHECK(result.http_status == 400);
    CHECK(result.field == "date");

    // A user without a doctor profile cannot obtain a schedule.
    result = doctor_schedule(repo, "unknown-user", "2026-10-26");
    CHECK(!result.ok);
    CHECK(result.http_status == 404);
    CHECK(result.code == "NOT_FOUND");

    // Error JSON includes the field and request ID.
    // Error JSON includes code, field, and request ID.
    const auto invalid_for_json = doctor_schedule(
        repo, "doctor-user-1", "abc");

    const auto error_json = nlohmann::json::parse(
        schedule_error_to_json(invalid_for_json, "test-request-id"));

    CHECK(error_json.contains("error"));
    CHECK(error_json.contains("requestId"));
    CHECK(error_json["requestId"] == "test-request-id");
    CHECK(error_json["error"]["code"] == "VALIDATION_FAILED");
    CHECK(error_json["error"]["field"] == "date");

    std::cout << "All doctor schedule tests passed.\n";
    return 0;
}
