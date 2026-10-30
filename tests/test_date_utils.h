#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

namespace test_support {

inline bool is_leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

inline int days_in_month(int year, int month)
{
    static const int month_lengths[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) return 29;
    return month_lengths[month - 1];
}

// Map each 30-minute bucket to a valid date in the 10,000-day window
// beginning 2000-01-01. This keeps PostgreSQL test dates deterministic and
// spreads separate test runs across dates without relying on C++20 calendars.
inline std::string date_from_half_hour_bucket(std::int64_t half_hour_bucket)
{
    std::int64_t day_offset = half_hour_bucket % 10000;
    if (day_offset < 0) day_offset += 10000;

    int year = 2000;
    while (day_offset >= (is_leap_year(year) ? 366 : 365)) {
        day_offset -= is_leap_year(year) ? 366 : 365;
        ++year;
    }

    int month = 1;
    while (day_offset >= days_in_month(year, month)) {
        day_offset -= days_in_month(year, month);
        ++month;
    }
    const int day = static_cast<int>(day_offset) + 1;

    char text[11];
    std::snprintf(text, sizeof(text), "%04d-%02d-%02d", year, month, day);
    return text;
}

inline std::string integration_test_date()
{
    const std::int64_t half_hour_bucket =
        std::chrono::duration_cast<std::chrono::minutes>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() / 30;
    return date_from_half_hour_bucket(half_hour_bucket);
}

}  // namespace test_support
