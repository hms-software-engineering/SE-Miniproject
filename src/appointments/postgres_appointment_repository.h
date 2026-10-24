#pragma once

#include "appointments/appointments.h"

namespace hms {

class PostgresAppointmentRepository final : public AppointmentRepository {
public:
    std::string working_hours(long long doctor_id) const override;

    std::vector<std::string> booked_times(
        long long doctor_id,
        const std::string& date) const override;

    SaveResult insert(const Appointment& appointment) override;
};

}  // namespace hms
