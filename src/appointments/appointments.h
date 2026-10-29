#pragma once
#include "reports/report_service.h"

#include <map>
#include <chrono>
#include <set>
#include <string>
#include <vector>


namespace hms {

struct Appointment {
    long long id = 0;
    std::string patient_id;
    long long doctor_id = 0;
    std::string slot_start;  // "YYYY-MM-DD HH:MM"
    std::string status;
};

enum class SaveStatus { Ok, SlotTaken, PatientMissing, DoctorMissing };

struct SaveResult {
    SaveStatus status = SaveStatus::Ok;
    Appointment appointment;
};

class AppointmentRepository {
public:
    virtual ~AppointmentRepository() = default;
    // Returns the doctor's working_hours JSON, or "" if the doctor is unknown.
    virtual std::string working_hours(long long doctor_id) const = 0;
    // Confirmed bookings for that doctor and date, as "HH:MM".
    virtual std::vector<std::string> booked_times(long long doctor_id,
                                                  const std::string& date) const = 0;
    // Must be atomic. The database version relies on the unique index
    // uq_appointments_confirmed_doctor_slot.
    virtual SaveResult insert(const Appointment& a) = 0;
    virtual std::vector<Appointment> appointments_for_doctor_on(
    long long doctor_id,
    const std::string& date) const = 0;
    virtual std::string doctor_id_for_user(
    const std::string& user_id) const = 0;
};

class InMemoryAppointmentRepository : public AppointmentRepository {
public:
    InMemoryAppointmentRepository();  // seeds doctor 1 and PID-00000001/2
    std::string working_hours(long long doctor_id) const override;
    std::vector<std::string> booked_times(long long doctor_id,
                                          const std::string& date) const override;
    SaveResult insert(const Appointment& a) override;
    std::vector<Appointment> appointments_for_doctor_on(
    long long doctor_id,
    const std::string& date) const override;

    std::string doctor_id_for_user(
        const std::string& user_id) const override;

    // Helpers for unit tests.
    void add_doctor(long long doctor_id,
                    const std::string& user_id,
                    const std::string& working_hours);

    void set_status(long long appointment_id,
                    const std::string& status);

private:
    std::map<long long, std::string> doctors_;
    std::map<std::string, long long> doctor_users_;
    std::set<std::string> patients_;
    std::vector<Appointment> items_;
    long long next_id_ = 1;
};

class PostgresAppointmentRepository final : public AppointmentRepository,
                                           public reports::ReportRepository {
public:
    explicit PostgresAppointmentRepository(
        const char* connection_info = nullptr);
    ~PostgresAppointmentRepository() override;

    PostgresAppointmentRepository(const PostgresAppointmentRepository&) = delete;
    PostgresAppointmentRepository& operator=(
        const PostgresAppointmentRepository&) = delete;

    std::string working_hours(long long doctor_id) const override;

    std::vector<std::string> booked_times(
        long long doctor_id, const std::string& date) const override;

    SaveResult insert(const Appointment& appointment) override;
    std::vector<Appointment> appointments_for_doctor_on(
    long long doctor_id,
    const std::string& date) const override;

    bool patient_belongs_to_user(
        const std::string& patient_id,
        const std::string& user_id) const;

    std::vector<reports::DailyReportRow> daily_report(
        const std::string& start_date,
        const std::string& end_date) const override;
    std::string doctor_id_for_user(const std::string& user_id) const override;

private:
    struct Connection;
    Connection* connection_;
};

struct BookingResult {
    bool ok = false;
    Appointment appointment;
    int http_status = 0;
    std::string code, message, field;
    std::vector<std::string> next_slots;       // used by the httplib JSON
    std::vector<std::string> suggested_slots;  // used by main's code and tests
};

struct ScheduleResult {
    bool ok = false;
    int http_status = 0;
    std::string code;
    std::string message;
    std::string field;
    std::vector<Appointment> appointments;
};

// 0 = Sunday ... 6 = Saturday. Date must be "YYYY-MM-DD".
int weekday_of(const std::string& date);
bool appointment_date_valid(const std::string& date);
// 30-minute slot starts ("HH:MM") for that date, from working_hours JSON.
std::vector<std::string> slots_for_date(const std::string& working_hours_json,
                                        const std::string& date);
std::vector<std::string> free_slots(const AppointmentRepository& repo,
                                    long long doctor_id,
                                    const std::string& date);
BookingResult book_appointment(AppointmentRepository& repo,
                               const std::string& patient_id,
                               long long doctor_id,
                               const std::string& date,
                               const std::string& time,
                               std::chrono::system_clock::time_point now =
                                   std::chrono::system_clock::now());

std::vector<std::string> next_free_slots(
    const AppointmentRepository& repo,
    long long doctor_id,
    const std::string& date,
    const std::string& after_time,
    size_t count = 3
);

std::string booking_error_to_json(
    const BookingResult& r,
    const std::string& request_id
);
std::string slots_to_json(const std::vector<std::string>& slots);
std::string appointment_to_json(const Appointment& a);
bool is_valid_calendar_date(const std::string& date);
ScheduleResult doctor_schedule(
    const AppointmentRepository& repo,
    const std::string& user_id,
    const std::string& date);

std::string schedule_to_json(
    const std::vector<Appointment>& appointments);

std::string schedule_error_to_json(
    const ScheduleResult& result,
    const std::string& request_id);

}  // namespace hms