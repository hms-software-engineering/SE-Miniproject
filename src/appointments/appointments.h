#pragma once
#include <map>
#include <string>
#include <vector>

namespace hms {

struct Appointment {
    std::string id, patient_id, doctor_id, date, time, status;
};

// Swap the in-memory version for PostgreSQL once T-02 is merged.
class AppointmentRepository {
public:
    virtual ~AppointmentRepository() = default;
    virtual bool is_booked(const std::string& doctor_id,
                           const std::string& date,
                           const std::string& time) const = 0;
    virtual Appointment save(Appointment a) = 0;  // assigns a unique id
};

class InMemoryAppointmentRepository : public AppointmentRepository {
public:
    bool is_booked(const std::string& d, const std::string& date,
                   const std::string& time) const override;
    Appointment save(Appointment a) override;
private:
    std::map<std::string, Appointment> items_;
    int next_ = 1;
};

struct BookingResult {
    bool ok = false;
    Appointment appointment;
    int http_status = 0;
    std::string code, message, field;
};

std::vector<std::string> day_slots();  // 09:00 ... 16:30
std::vector<std::string> free_slots(const AppointmentRepository&,
                                    const std::string& doctor_id,
                                    const std::string& date);
BookingResult book_appointment(AppointmentRepository&,
                               const std::string& patient_id,
                               const std::string& doctor_id,
                               const std::string& date,
                               const std::string& time);

std::string slots_to_json(const std::vector<std::string>& slots);
std::string appointment_to_json(const Appointment&);

} // namespace hms
