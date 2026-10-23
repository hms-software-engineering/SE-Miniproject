#include "appointments/postgres_appointment_repository.h"

#include <pqxx/pqxx>

#include <string>
#include <utility>
#include <vector>

namespace hms {

std::string PostgresAppointmentRepository::working_hours(
    long long doctor_id) const {
    pqxx::connection conn{
        "host=localhost port=5432 dbname=hms user=dakshasg"
    };
    pqxx::read_transaction tx{conn};

    const auto result = tx.exec(
        "SELECT working_hours::text FROM doctors WHERE doctor_id = $1",
        pqxx::params{doctor_id});

    if (result.empty()) {
        return "";
    }

    return result[0][0].as<std::string>();
}

std::vector<std::string> PostgresAppointmentRepository::booked_times(
    long long doctor_id, const std::string& date) const {
    pqxx::connection conn{
        "host=localhost port=5432 dbname=hms user=dakshasg"
    };
    pqxx::read_transaction tx{conn};

    const auto result = tx.exec(
        "SELECT to_char(slot_start, 'HH24:MI') "
        "FROM appointments "
        "WHERE doctor_id = $1 "
        "AND slot_start >= $2::date "
        "AND slot_start < ($2::date + INTERVAL '1 day') "
        "AND status = 'Confirmed' "
        "ORDER BY slot_start",
        pqxx::params{doctor_id, date});

    std::vector<std::string> times;
    times.reserve(result.size());

    for (const auto& row : result) {
        times.push_back(row[0].as<std::string>());
    }

    return times;
}

SaveResult PostgresAppointmentRepository::insert(const Appointment& a) {
    SaveResult result;

    try {
        pqxx::connection conn{
            "host=localhost port=5432 dbname=hms user=dakshasg"
        };
        pqxx::work tx{conn};

        // Validate both referenced records before inserting.
        const auto patient = tx.exec(
            "SELECT 1 FROM patients WHERE patient_id = $1",
            pqxx::params{a.patient_id});

        if (patient.empty()) {
            result.status = SaveStatus::PatientMissing;
            return result;
        }

        const auto doctor = tx.exec(
            "SELECT 1 FROM doctors WHERE doctor_id = $1",
            pqxx::params{a.doctor_id});

        if (doctor.empty()) {
            result.status = SaveStatus::DoctorMissing;
            return result;
        }

        const auto inserted = tx.exec(
            "INSERT INTO appointments "
            "(patient_id, doctor_id, slot_start, status) "
            "VALUES ($1, $2, $3::timestamp, 'Confirmed') "
            "RETURNING appointment_id, patient_id, doctor_id, "
            "to_char(slot_start, 'YYYY-MM-DD HH24:MI'), status",
            pqxx::params{a.patient_id, a.doctor_id, a.slot_start});

        const auto& row = inserted[0];

        Appointment saved;
        saved.id = row[0].as<long long>();
        saved.patient_id = row[1].as<std::string>();
        saved.doctor_id = row[2].as<long long>();
        saved.slot_start = row[3].as<std::string>();
        saved.status = row[4].as<std::string>();

        tx.commit();

        result.status = SaveStatus::Ok;
        result.appointment = std::move(saved);
        return result;
    } catch (const pqxx::unique_violation&) {
        // The database unique index prevents duplicate confirmed bookings.
        result.status = SaveStatus::SlotTaken;
        return result;
    } catch (const pqxx::foreign_key_violation&) {
        // A referenced record may have been removed between validation
        // and insertion. Do not misreport which record is missing.
        throw;
    }
}

}  // namespace hms
