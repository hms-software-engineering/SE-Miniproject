#include "health/availability.h"

#include <cstdint>
#include <map>
#include <stdexcept>

namespace hms::health {
namespace {

constexpr std::int64_t SECONDS_PER_DAY = 24 * 60 * 60;
constexpr std::int64_t SERVICE_START_SECONDS = 6 * 60 * 60;
constexpr std::int64_t SERVICE_END_SECONDS = 22 * 60 * 60;
constexpr double AVAILABILITY_TARGET_PERCENT = 99.5;

std::int64_t seconds_since_epoch(
    std::chrono::system_clock::time_point timestamp)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch());
    if (timestamp.time_since_epoch() != seconds) {
        throw std::invalid_argument(
            "Availability timestamps must have whole-second precision.");
    }
    return seconds.count();
}

std::int64_t floor_divide(std::int64_t value, std::int64_t divisor)
{
    std::int64_t quotient = value / divisor;
    if (value % divisor < 0) {
        --quotient;
    }
    return quotient;
}

std::int64_t eligible_seconds_in_slot(
    std::int64_t utc_start,
    std::int64_t utc_end,
    std::int64_t offset_seconds)
{
    const std::int64_t local_start = utc_start + offset_seconds;
    const std::int64_t local_end = utc_end + offset_seconds;
    const std::int64_t first_day = floor_divide(local_start, SECONDS_PER_DAY);
    const std::int64_t last_day = floor_divide(local_end - 1, SECONDS_PER_DAY);

    std::int64_t eligible = 0;
    for (std::int64_t day = first_day; day <= last_day; ++day) {
        const std::int64_t window_start =
            day * SECONDS_PER_DAY + SERVICE_START_SECONDS;
        const std::int64_t window_end =
            day * SECONDS_PER_DAY + SERVICE_END_SECONDS;
        const std::int64_t overlap_start =
            local_start > window_start ? local_start : window_start;
        const std::int64_t overlap_end =
            local_end < window_end ? local_end : window_end;
        if (overlap_end > overlap_start) {
            eligible += overlap_end - overlap_start;
        }
    }
    return eligible;
}

} // namespace

AvailabilityReport calculate_availability(
    std::chrono::system_clock::time_point period_start_utc,
    std::chrono::system_clock::time_point period_end_utc,
    const std::vector<ProbeObservation>& observations,
    int utc_offset_minutes,
    std::chrono::seconds probe_interval)
{
    if (period_end_utc <= period_start_utc) {
        throw std::invalid_argument(
            "Availability period end must be after its start.");
    }
    if (probe_interval <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("Probe interval must be positive.");
    }
    if (utc_offset_minutes < -14 * 60 || utc_offset_minutes > 14 * 60) {
        throw std::invalid_argument("UTC offset must be within +/-14 hours.");
    }

    const std::int64_t period_start = seconds_since_epoch(period_start_utc);
    const std::int64_t period_end = seconds_since_epoch(period_end_utc);
    const std::int64_t interval_seconds = probe_interval.count();
    const std::int64_t offset_seconds =
        static_cast<std::int64_t>(utc_offset_minutes) * 60;

    std::map<std::int64_t, bool> sample_by_start;
    for (const ProbeObservation& observation : observations) {
        const std::int64_t sample_time =
            seconds_since_epoch(observation.interval_start_utc);
        if (sample_time < period_start || sample_time >= period_end ||
            (sample_time - period_start) % interval_seconds != 0) {
            throw std::invalid_argument(
                "Probe observations must align to intervals inside the report period.");
        }
        if (!sample_by_start.emplace(sample_time, observation.ready).second) {
            throw std::invalid_argument(
                "Availability report contains duplicate probe timestamps.");
        }
    }

    AvailabilityReport report;
    for (std::int64_t slot_start = period_start;
         slot_start < period_end;
         slot_start += interval_seconds) {
        const std::int64_t slot_end =
            slot_start + interval_seconds < period_end
                ? slot_start + interval_seconds
                : period_end;
        const std::int64_t eligible = eligible_seconds_in_slot(
            slot_start, slot_end, offset_seconds);
        report.eligible_time += std::chrono::seconds(eligible);

        const auto sample = sample_by_start.find(slot_start);
        if (sample != sample_by_start.end() && sample->second) {
            report.available_time += std::chrono::seconds(eligible);
        }
    }

    report.downtime = report.eligible_time - report.available_time;
    if (report.eligible_time > std::chrono::seconds::zero()) {
        report.availability_percent =
            100.0 * static_cast<double>(report.available_time.count()) /
            static_cast<double>(report.eligible_time.count());
        report.target_met =
            report.availability_percent >= AVAILABILITY_TARGET_PERCENT;
    }
    return report;
}

} // namespace hms::health
