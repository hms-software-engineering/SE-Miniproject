#include "../src/reports/report_service.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::cerr << "FAILED: " #condition " (line " << __LINE__        \
                      << ")\n";                                             \
            std::exit(1);                                                   \
        }                                                                   \
    } while (false)

int main()
{
    const std::vector<hms::reports::DailyReportRow> rows{
        {"2026-10-26", 4, 2, 1, 1, 0, "1250.50"},
        {"2026-10-27", 0, 0, 0, 0, 0, "0.00"}
    };

    const std::string csv = hms::reports::to_csv(rows);
    CHECK(csv ==
          "date,appointment_count,confirmed_count,cancelled_count,"
          "completed_count,no_show_count,revenue\n"
          "2026-10-26,4,2,1,1,0,1250.50\n"
          "2026-10-27,0,0,0,0,0,0.00\n");

    const std::string pdf =
        hms::reports::to_pdf(rows, "2026-10-26", "2026-10-27");
    CHECK(pdf.rfind("%PDF-1.4\n", 0) == 0);
    CHECK(pdf.find("Daily appointment and revenue report") !=
          std::string::npos);
    CHECK(pdf.find("Date range: 2026-10-26 to 2026-10-27") !=
          std::string::npos);
    CHECK(pdf.find("1250.50") != std::string::npos);
    CHECK(pdf.find("xref\n") != std::string::npos);
    CHECK(pdf.find("%%EOF\n") != std::string::npos);

    const std::vector<hms::reports::DailyReportRow> escaped_row{
        {"2026-10-28,\"review\"\n", 1, 1, 0, 0, 0, "12,50\""}
    };
    const std::string escaped_csv = hms::reports::to_csv(escaped_row);
    CHECK(escaped_csv.find("\"2026-10-28,\"\"review\"\"\n\"") !=
          std::string::npos);
    CHECK(escaped_csv.find("\"12,50\"\"\"") != std::string::npos);

    const std::string escaped_pdf = hms::reports::to_pdf(
        {{"2026-10-28 (revised)\\final", 1, 1, 0, 0, 0, "12.50"},
         {"2026-10-29", 0, 0, 0, 0, 0, "0.00"}},
        "2026-10-28 (start)", "2026-10-29\\end");
    CHECK(escaped_pdf.find("2026-10-28 \\(start\\)") != std::string::npos);
    CHECK(escaped_pdf.find("2026-10-29\\\\end") != std::string::npos);
    CHECK(escaped_pdf.find("2026-10-28 \\(revised\\)\\\\final") !=
          std::string::npos);

    const std::string empty_pdf =
        hms::reports::to_pdf({}, "2026-10-30", "2026-10-30");
    CHECK(empty_pdf.find("/Count 1") != std::string::npos);
    CHECK(empty_pdf.find("/Type /Page ") != std::string::npos);

    std::vector<hms::reports::DailyReportRow> multi_page(43);
    for (std::size_t i = 0; i < multi_page.size(); ++i) {
        multi_page[i].date = "2026-10-26";
        multi_page[i].revenue = "0.00";
    }
    const std::string paged_pdf =
        hms::reports::to_pdf(multi_page, "2026-10-26", "2026-12-07");
    CHECK(paged_pdf.find("/Count 2") != std::string::npos);
    CHECK(paged_pdf.find("/Type /Page ") != std::string::npos);

    std::cout << "All report service tests passed.\n";
    return 0;
}
