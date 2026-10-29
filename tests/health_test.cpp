#include "health/availability.h"
#include "health/health_service.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(condition)                                                     \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << "FAILED: " #condition " (line " << __LINE__      \
                      << ")\n";                                              \
            std::exit(1);                                                     \
        }                                                                     \
    } while (false)

namespace {

std::vector<hms::health::ProbeObservation> full_day_samples(
    std::chrono::system_clock::time_point day_start,
    const std::vector<int>& unavailable_minutes = {})
{
    using namespace std::chrono;
    std::vector<hms::health::ProbeObservation> samples;
    samples.reserve(24 * 60);
    for (int minute = 0; minute < 24 * 60; ++minute) {
        const bool ready = std::find(unavailable_minutes.begin(),
                                     unavailable_minutes.end(), minute) ==
                           unavailable_minutes.end();
        samples.push_back({day_start + minutes(minute), ready});
    }
    return samples;
}

void test_health_responses()
{
    const auto live = hms::health::liveness_response();
    CHECK(live.http_status == 200);
    CHECK(live.body ==
          "{\"status\":\"alive\",\"checks\":{\"process\":\"ok\"}}");

    const auto ready = hms::health::readiness_response(true);
    CHECK(ready.http_status == 200);
    CHECK(ready.body ==
          "{\"status\":\"ready\",\"checks\":{"
          "\"process\":\"ok\",\"postgres\":\"ok\"}}");

    const auto unavailable = hms::health::readiness_response(false);
    CHECK(unavailable.http_status == 503);
    CHECK(unavailable.body ==
          "{\"status\":\"unavailable\",\"checks\":{"
          "\"process\":\"ok\",\"postgres\":\"unavailable\"}}");

    for (const auto& response : {live, ready, unavailable}) {
        CHECK(response.body.front() == '{');
        CHECK(response.body.back() == '}');
        CHECK(response.body.find("password") == std::string::npos);
        CHECK(response.body.find("connectionString") == std::string::npos);
        CHECK(response.body.find("token") == std::string::npos);
        CHECK(response.body.find("stack") == std::string::npos);
    }
}

void test_availability_window_and_target()
{
    using namespace std::chrono;
    const auto day_start = system_clock::time_point{};
    const auto day_end = day_start + hours(24);

    // +05:30 means 06:00 local is 00:30 UTC and 22:00 local is 16:30 UTC.
    const auto healthy = full_day_samples(day_start);
    const auto all_available = hms::health::calculate_availability(
        day_start, day_end, healthy, 330);
    CHECK(all_available.eligible_time == hours(16));
    CHECK(all_available.downtime == seconds::zero());
    CHECK(all_available.availability_percent == 100.0);
    CHECK(all_available.target_met);

    // The 06:00 and 21:59 local intervals count; intervals outside do not.
    const auto boundary_samples = full_day_samples(
        day_start, {29, 30, 989, 990});
    const auto boundary_report = hms::health::calculate_availability(
        day_start, day_end, boundary_samples, 330);
    CHECK(boundary_report.eligible_time == hours(16));
    CHECK(boundary_report.downtime == minutes(2));

    // A full service day allows 4.8 minutes of downtime at 99.5%.
    const auto four_minutes_down = hms::health::calculate_availability(
        day_start, day_end, full_day_samples(day_start, {30, 31, 32, 33}),
        330);
    CHECK(four_minutes_down.downtime == minutes(4));
    CHECK(four_minutes_down.target_met);

    const auto five_minutes_down = hms::health::calculate_availability(
        day_start, day_end,
        full_day_samples(day_start, {30, 31, 32, 33, 34}), 330);
    CHECK(five_minutes_down.downtime == minutes(5));
    CHECK(!five_minutes_down.target_met);

    const auto no_measurements = hms::health::calculate_availability(
        day_start, day_end, {}, 330);
    CHECK(no_measurements.available_time == seconds::zero());
    CHECK(no_measurements.downtime == hours(16));
    CHECK(no_measurements.availability_percent == 0.0);
    CHECK(!no_measurements.target_met);
}

void test_invalid_availability_inputs()
{
    using namespace std::chrono;
    bool rejected = false;
    try {
        (void)hms::health::calculate_availability(
            system_clock::time_point{}, system_clock::time_point{} + hours(1),
            {}, 15 * 60);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
}

} // namespace

int main()
{
    test_health_responses();
    test_availability_window_and_target();
    test_invalid_availability_inputs();
    std::cout << "All health and availability tests passed.\n";
    return 0;
}
