#pragma once

#include <chrono>
#include <vector>

namespace hms::health {

struct ProbeObservation {
    std::chrono::system_clock::time_point interval_start_utc;
    bool ready = false;
};

struct AvailabilityReport {
    std::chrono::seconds eligible_time{0};
    std::chrono::seconds available_time{0};
    std::chrono::seconds downtime{0};
    double availability_percent = 0.0;
    bool target_met = false;
};

// Each observation covers [interval_start_utc, interval_start_utc + interval).
// Missing intervals count as downtime. The UTC offset is explicit and fixed
// for this report; the host's local timezone is never consulted.
AvailabilityReport calculate_availability(
    std::chrono::system_clock::time_point period_start_utc,
    std::chrono::system_clock::time_point period_end_utc,
    const std::vector<ProbeObservation>& observations,
    int utc_offset_minutes,
    std::chrono::seconds probe_interval = std::chrono::minutes(1));

} // namespace hms::health
