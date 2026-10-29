#pragma once

#include <string>
#include <vector>

namespace hms::reports {

struct DailyReportRow {
    std::string date;
    int appointment_count = 0;
    int confirmed_count = 0;
    int cancelled_count = 0;
    int completed_count = 0;
    int no_show_count = 0;
    std::string revenue;
};

class ReportRepository {
public:
    virtual ~ReportRepository() = default;
    virtual std::vector<DailyReportRow> daily_report(
        const std::string& start_date,
        const std::string& end_date) const = 0;
};

std::string to_csv(const std::vector<DailyReportRow>& rows);
std::string to_pdf(const std::vector<DailyReportRow>& rows,
                   const std::string& start_date,
                   const std::string& end_date);

}  // namespace hms::reports
